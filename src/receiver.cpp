#include "receiver.h"
#include <algorithm>

using namespace std::chrono;

// 静态常量定义
const milliseconds ScreenReceiver::FRAME_TIMEOUT(1000); // 帧超时时间(1秒)

// 构造函数
ScreenReceiver::ScreenReceiver() {
    InitializeCriticalSection(&m_ImageCS);
    InitializeCriticalSection(&m_ImageQueueCS);
    InitializeCriticalSection(&m_FramesCS);
    m_hImageQueueEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_lastFpsUpdateTime = steady_clock::now();
}

// 析构函数
ScreenReceiver::~ScreenReceiver() {
    Stop(); // 确保停止接收
    
    DeleteCriticalSection(&m_ImageCS);
    DeleteCriticalSection(&m_ImageQueueCS);
    DeleteCriticalSection(&m_FramesCS);
    
    if (m_hImageQueueEvent) {
        CloseHandle(m_hImageQueueEvent);
    }
    
    // 清理位图
    EnterCriticalSection(&m_ImageCS);
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = NULL;
    }
    LeaveCriticalSection(&m_ImageCS);
}

// 开始接收
bool ScreenReceiver::Start(const std::string& multicastGroup, int port, const std::string& localInterface, HWND hParentWnd) {
    if (m_bListening) {
        return false; // 已经在接收
    }

    // 保存参数
    m_multicastGroup = multicastGroup;
    m_port = port;
    m_localInterface = localInterface;
    m_hParentWindow = hParentWnd; // 保存主窗口句柄

    // 创建图像显示窗口（如果不存在）
    if (!m_hDisplayWindow && hParentWnd) {
        m_hDisplayWindow = CreateDisplayWindow(hParentWnd);
        if (m_hDisplayWindow) {
            ShowWindow(m_hDisplayWindow, SW_SHOW);
            // 恢复置顶状态
            if (m_bDisplayWindowTopMost) {
                SetWindowPos(m_hDisplayWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
        }
    }

    // 清空之前的帧状态和图像队列
    EnterCriticalSection(&m_FramesCS);
    m_Frames.clear();
    LeaveCriticalSection(&m_FramesCS);

    EnterCriticalSection(&m_ImageQueueCS);
    while (!m_ImageQueue.empty()) {
        m_ImageQueue.pop();
    }
    LeaveCriticalSection(&m_ImageQueueCS);
    
    // 重置FPS统计
    m_frameCount = 0;
    m_currentFPS = 0.0;
    m_lastFpsUpdateTime = steady_clock::now();
    m_senderIPAddress = "";

    // 启动图像处理线程
    m_bImageProcessing = true;
    m_hImageProcessingThread = CreateThread(NULL, 0, ImageProcessingThreadFunc, this, 0, NULL);

    // 启动接收线程
    m_bListening = true;
    m_hRecvThread = CreateThread(NULL, 0, RecvThreadFunc, this, 0, NULL);

    return true;
}

// 停止接收
void ScreenReceiver::Stop() {
    m_bListening = false;
    m_bImageProcessing = false;

    // 关闭套接字以中断recvfrom
    if (m_RecvSocket != INVALID_SOCKET) {
        closesocket(m_RecvSocket);
        m_RecvSocket = INVALID_SOCKET;
    }

    // 通知图像处理线程退出
    SetEvent(m_hImageQueueEvent);

    // 等待线程结束
    if (m_hRecvThread) {
        WaitForSingleObject(m_hRecvThread, INFINITE);
        CloseHandle(m_hRecvThread);
        m_hRecvThread = NULL;
    }

    if (m_hImageProcessingThread) {
        WaitForSingleObject(m_hImageProcessingThread, INFINITE);
        CloseHandle(m_hImageProcessingThread);
        m_hImageProcessingThread = NULL;
    }

    // 清空帧状态和图像队列
    EnterCriticalSection(&m_FramesCS);
    m_Frames.clear();
    LeaveCriticalSection(&m_FramesCS);

    EnterCriticalSection(&m_ImageQueueCS);
    while (!m_ImageQueue.empty()) {
        m_ImageQueue.pop();
    }
    LeaveCriticalSection(&m_ImageQueueCS);
}

// 图像显示窗口过程（静态）
LRESULT CALLBACK ScreenReceiver::DisplayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    ScreenReceiver* pReceiver = nullptr;
    
    if (message == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pReceiver = reinterpret_cast<ScreenReceiver*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pReceiver));
    } else {
        pReceiver = reinterpret_cast<ScreenReceiver*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }
    
    if (pReceiver) {
        return pReceiver->HandleDisplayWindowMessage(hWnd, message, wParam, lParam);
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// 实例窗口过程
LRESULT ScreenReceiver::HandleDisplayWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 获取窗口客户区大小
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        int windowWidth = clientRect.right - clientRect.left;
        int windowHeight = clientRect.bottom - clientRect.top;

        // 创建内存DC和位图用于双缓冲
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, windowWidth, windowHeight);
        SelectObject(hdcMem, hbmMem);

        // 填充背景
        HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));
        FillRect(hdcMem, &clientRect, hBrush);
        DeleteObject(hBrush);

        // 锁定图像互斥锁
        EnterCriticalSection(&m_ImageCS);

        if (m_hBitmap) {
            // 创建位图DC
            HDC hdcBmp = CreateCompatibleDC(hdc);
            SelectObject(hdcBmp, m_hBitmap);

            // 获取位图尺寸
            BITMAP bm;
            GetObject(m_hBitmap, sizeof(BITMAP), &bm);
            int imageWidth = bm.bmWidth;
            int imageHeight = bm.bmHeight;

            // 计算保持原比例的最大显示尺寸
            float widthRatio = static_cast<float>(windowWidth) / imageWidth;
            float heightRatio = static_cast<float>(windowHeight) / imageHeight;
            float scale = std::min(widthRatio, heightRatio);

            int displayWidth = static_cast<int>(imageWidth * scale);
            int displayHeight = static_cast<int>(imageHeight * scale);

            // 居中显示
            int x = (windowWidth - displayWidth) / 2;
            int y = (windowHeight - displayHeight) / 2;

            // 使用高质量缩放
            SetStretchBltMode(hdcMem, HALFTONE);
            SetBrushOrgEx(hdcMem, 0, 0, NULL);

            // 绘制图像
            StretchBlt(hdcMem, x, y, displayWidth, displayHeight,
                hdcBmp, 0, 0, imageWidth, imageHeight, SRCCOPY);

            DeleteDC(hdcBmp);
        }
        else {
            // 没有图像时显示提示
            RECT textRect = { 0, 0, windowWidth, windowHeight };
            SetBkMode(hdcMem, TRANSPARENT);
            SetTextColor(hdcMem, RGB(100, 100, 100));
            DrawText(hdcMem, L"等待接收图像...", -1, &textRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // 将内存DC内容复制到屏幕
        BitBlt(hdc, 0, 0, windowWidth, windowHeight, hdcMem, 0, 0, SRCCOPY);

        // 清理
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        LeaveCriticalSection(&m_ImageCS);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_ERASEBKGND: {
        // 阻止背景擦除，减少闪烁
        return 1;
    }
    case WM_SIZE: {
        // 窗口大小改变时重绘
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
    case WM_CONTEXTMENU: {
        // 创建右键菜单
        HMENU hMenu = CreatePopupMenu();
        if (hMenu) {
            // 添加"置顶"菜单项
            MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
            mii.fMask = MIIM_STRING | MIIM_STATE | MIIM_ID;
            mii.fState = m_bDisplayWindowTopMost ? MFS_CHECKED : MFS_UNCHECKED;
            mii.wID = WM_TOGGLE_TOP_MOST;
            wchar_t itemName[10] = { L"置顶" };
            mii.dwTypeData = itemName;
            mii.cch = lstrlen(mii.dwTypeData);

            InsertMenuItem(hMenu, 0, TRUE, &mii);

            // 添加分隔线
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

            // 添加"关闭"菜单项
            AppendMenu(hMenu, MF_STRING, 1, L"关闭");

            // 显示菜单
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);

            DestroyMenu(hMenu);
        }
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == WM_TOGGLE_TOP_MOST) {
            // 切换置顶状态
            m_bDisplayWindowTopMost = !m_bDisplayWindowTopMost;
            SetWindowPos(hWnd,
                m_bDisplayWindowTopMost ? HWND_TOPMOST : HWND_NOTOPMOST,
                0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE);
        }
        else if (LOWORD(wParam) == 1) {
            // 关闭窗口
            DestroyWindow(hWnd);
        }
        break;
    }
    case WM_DESTROY: {
        // 清理位图
        EnterCriticalSection(&m_ImageCS);
        if (m_hBitmap) {
            DeleteObject(m_hBitmap);
            m_hBitmap = NULL;
        }
        LeaveCriticalSection(&m_ImageCS);

        // 重置窗口句柄
        m_hDisplayWindow = NULL;
        m_bDisplayWindowTopMost = false;

        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 创建图像显示窗口
HWND ScreenReceiver::CreateDisplayWindow(HWND hParent) {
    // 注册显示窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = DisplayWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ImageDisplayClass";
    wc.hbrBackground = NULL; // 设置为NULL，我们自己处理背景
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // 为接收窗口添加图标
    wc.hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(101), 
                                IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

    RegisterClass(&wc);

    // 创建独立窗口（不作为主窗口的子窗口）
    HWND hWnd = CreateWindow(
        L"ImageDisplayClass",
        L"接收的图像",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, GetModuleHandle(NULL), this  // 改为 NULL，创建独立顶层窗口
    );

    return hWnd;
}

// 图像处理线程函数（静态）
DWORD WINAPI ScreenReceiver::ImageProcessingThreadFunc(LPVOID lpParam) {
    ScreenReceiver* pReceiver = reinterpret_cast<ScreenReceiver*>(lpParam);
    if (pReceiver) {
        pReceiver->ImageProcessingThread();
    }
    return 0;
}

// 图像处理线程
void ScreenReceiver::ImageProcessingThread() {
    while (m_bImageProcessing) {
        // 等待图像数据事件或退出信号
        DWORD dwWaitResult = WaitForSingleObject(m_hImageQueueEvent, 100);

        if (!m_bImageProcessing) break;

        if (dwWaitResult == WAIT_OBJECT_0) {
            std::vector<BYTE> jpegData;

            // 从队列获取图像数据
            EnterCriticalSection(&m_ImageQueueCS);
            if (!m_ImageQueue.empty()) {
                jpegData = std::move(m_ImageQueue.front());
                m_ImageQueue.pop();

                // 如果队列还有数据，重新设置事件
                if (!m_ImageQueue.empty()) {
                    SetEvent(m_hImageQueueEvent);
                }
            }
            LeaveCriticalSection(&m_ImageQueueCS);

            if (jpegData.empty()) continue;

            // 从内存加载JPEG图像
            IStream* stream = NULL;
            HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
            if (FAILED(hr)) {
                continue;
            }

            // 写入JPEG数据到流
            ULONG written = 0;
            hr = stream->Write(jpegData.data(), static_cast<ULONG>(jpegData.size()), &written);
            if (FAILED(hr) || written != jpegData.size()) {
                stream->Release();
                continue;
            }

            // 重置流位置
            LARGE_INTEGER zero = { 0 };
            stream->Seek(zero, STREAM_SEEK_SET, NULL);

            // 从流创建位图
            Bitmap bitmap(stream, FALSE);
            
            if (bitmap.GetLastStatus() != Ok) {
                stream->Release();
                continue;
            }

            // 转换为HBITMAP
            HBITMAP hNewBitmap = NULL;
            Status status = bitmap.GetHBITMAP(Color(255, 255, 255), &hNewBitmap);
            
            // 释放流（不再需要）
            stream->Release();

            if (status != Ok || !hNewBitmap) {
                // GetHBITMAP失败，继续下一个
                continue;
            }

            // 更新全局位图
            EnterCriticalSection(&m_ImageCS);
            if (m_hBitmap) {
                DeleteObject(m_hBitmap);
            }
            m_hBitmap = hNewBitmap;
            LeaveCriticalSection(&m_ImageCS);

            // 通知显示窗口更新
            if (m_hDisplayWindow) {
                InvalidateRect(m_hDisplayWindow, NULL, FALSE);
            }
        }

        // 控制帧率，避免更新过快
        Sleep(16); // ~60 FPS
    }
}

// 处理接收到的分片
void ScreenReceiver::ProcessFragment(const FragmentHeader& header, const BYTE* fragmentData) {
    EnterCriticalSection(&m_FramesCS);

    // 使用帧ID作为唯一标识
    uint32_t frameId = header.frameId;

    // 检查是否为新帧
    if (m_Frames.find(frameId) == m_Frames.end()) {
        // 创建新的帧重组状态
        FrameReassemblyState newState;
        newState.frameSize = header.frameSize;
        newState.totalFragments = header.totalFragments;
        newState.receivedCount = 0;
        newState.frameData.resize(header.frameSize);
        newState.fragmentsReceived.resize(header.totalFragments, false);
        newState.lastFragmentTime = steady_clock::now();

        m_Frames[frameId] = newState;
        
        // 更新FPS计数
        m_frameCount++;
        auto now = steady_clock::now();
        auto elapsed = duration_cast<milliseconds>(now - m_lastFpsUpdateTime);
        if (elapsed.count() >= 1000) {
            m_currentFPS = (m_frameCount * 1000.0) / elapsed.count();
            m_frameCount = 0;
            m_lastFpsUpdateTime = now;
            
            // 更新窗口标题
            if (m_hDisplayWindow) {
                UpdateWindowTitle();
            }
        }
    }

    FrameReassemblyState& state = m_Frames[frameId];

    // 更新最后接收时间
    state.lastFragmentTime = steady_clock::now();

    // 检查是否已经接收过这个分片
    if (state.fragmentsReceived[header.fragmentIndex]) {
        LeaveCriticalSection(&m_FramesCS);
        return;
    }

    // 计算分片偏移量
    size_t offset = header.fragmentIndex * MAX_FRAGMENT_SIZE;
    if (offset + header.fragmentSize > state.frameSize) {
        // 分片超出帧大小，丢弃
        LeaveCriticalSection(&m_FramesCS);
        return;
    }

    // 复制分片数据
    memcpy(state.frameData.data() + offset, fragmentData, header.fragmentSize);
    state.fragmentsReceived[header.fragmentIndex] = true;
    state.receivedCount++;

    // 检查是否已接收所有分片
    if (state.receivedCount == state.totalFragments) {
        // 将完整帧添加到图像处理队列
        EnterCriticalSection(&m_ImageQueueCS);
        
        // 限制队列大小，防止内存无限增长
        const size_t MAX_QUEUE_SIZE = 10; // 最多缓存10帧
        if (m_ImageQueue.size() < MAX_QUEUE_SIZE) {
            m_ImageQueue.push(std::move(state.frameData));
            SetEvent(m_hImageQueueEvent); // 通知有新图像数据
        }
        // 如果队列已满，丢弃该帧（避免内存泄漏）
        
        LeaveCriticalSection(&m_ImageQueueCS);

        // 移除已完成的帧
        m_Frames.erase(frameId);
    }

    LeaveCriticalSection(&m_FramesCS);
}

// 清理超时的帧
void ScreenReceiver::CleanupTimedOutFrames() {
    EnterCriticalSection(&m_FramesCS);
    auto now = steady_clock::now();

    for (auto it = m_Frames.begin(); it != m_Frames.end(); ) {
        if (duration_cast<milliseconds>(now - it->second.lastFragmentTime) > FRAME_TIMEOUT) {
            it = m_Frames.erase(it);
        }
        else {
            ++it;
        }
    }
    LeaveCriticalSection(&m_FramesCS);
}

// 更新窗口标题
void ScreenReceiver::UpdateWindowTitle() {
    if (!m_hDisplayWindow) return;
    
    // 构建窗口标题：IP - FPS
    std::string title = "画面来自";
    
    if (!m_senderIPAddress.empty()) {
        title += " - ";
        title += m_senderIPAddress;
    }
    
    if (m_currentFPS > 0) {
        char fpsStr[32];
        sprintf_s(fpsStr, sizeof(fpsStr), " - %.1f FPS", m_currentFPS);
        title += fpsStr;
    }
    
    // 转换为宽字符
    int titleLen = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, NULL, 0);
    std::wstring wtitle(titleLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, &wtitle[0], titleLen);
    wtitle.pop_back(); // 移除末尾的null字符
    
    // 设置窗口标题
    SetWindowText(m_hDisplayWindow, wtitle.c_str());
}

// 接收线程函数（静态）
DWORD WINAPI ScreenReceiver::RecvThreadFunc(LPVOID lpParam) {
    ScreenReceiver* pReceiver = reinterpret_cast<ScreenReceiver*>(lpParam);
    if (pReceiver) {
        pReceiver->RecvThread();
    }
    return 0;
}

// 接收线程
void ScreenReceiver::RecvThread() {
    std::string multicastGroup = m_multicastGroup;
    int port = m_port;
    std::string localInterface = m_localInterface;
    
    WSADATA wsaData;
    sockaddr_in recvAddr;
    struct ip_mreq mreq;
    char recvBuf[sizeof(FragmentHeader) + MAX_FRAGMENT_SIZE];

    // 初始化Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return;
    }

    // 创建UDP套接字
    m_RecvSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_RecvSocket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    // 允许地址重用
    int reuse = 1;
    if (setsockopt(m_RecvSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        closesocket(m_RecvSocket);
        WSACleanup();
        return;
    }

    // 绑定到本地端口
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    recvAddr.sin_port = htons(port);

    if (bind(m_RecvSocket, (sockaddr*)&recvAddr, sizeof(recvAddr)) == SOCKET_ERROR) {
        closesocket(m_RecvSocket);
        WSACleanup();
        return;
    }

    // 加入组播组
    mreq.imr_multiaddr.s_addr = inet_addr(multicastGroup.c_str());
    if (localInterface == "0.0.0.0") {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    }
    else {
        mreq.imr_interface.s_addr = inet_addr(localInterface.c_str());
    }

    if (setsockopt(m_RecvSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) {
        closesocket(m_RecvSocket);
        WSACleanup();
        return;
    }

    // 设置接收缓冲区大小（提高网络性能）
    int recvBufSize = 1024 * 1024; // 1MB
    setsockopt(m_RecvSocket, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufSize, sizeof(recvBufSize));

    // 设置非阻塞模式以便定期检查超时
    u_long mode = 1;
    ioctlsocket(m_RecvSocket, FIONBIO, &mode);

    // 接收消息循环
    while (m_bListening) {
        sockaddr_in fromAddr;
        int fromLen = sizeof(fromAddr);

        // 接收数据
        int recvLen = recvfrom(m_RecvSocket, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&fromAddr, &fromLen);

        if (recvLen == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                // 没有数据可读，清理超时帧并稍等
                CleanupTimedOutFrames();
                Sleep(1);
                continue;
            }

            if (m_bListening) { // 如果不是因为停止监听而导致的错误
                // 可以记录错误，但不中断循环
            }
            break;
        }

        // 检查数据长度是否足够包含分片头
        if (recvLen < static_cast<int>(sizeof(FragmentHeader))) {
            continue;
        }

        // 解析分片头
        FragmentHeader header;
        memcpy(&header, recvBuf, sizeof(FragmentHeader));

        // 校验魔数并转换网络字节序到主机字节序
        header.magic = ntohs(header.magic);
        if (header.magic != FRAGMENT_MAGIC) {
            continue;
        }
        header.frameId = ntohl(header.frameId);
        header.frameSize = ntohl(header.frameSize);
        header.totalFragments = ntohs(header.totalFragments);
        header.fragmentIndex = ntohs(header.fragmentIndex);
        header.fragmentSize = ntohs(header.fragmentSize);

        // 检查数据长度是否匹配
        if (recvLen != static_cast<int>(sizeof(FragmentHeader) + header.fragmentSize)) {
            continue;
        }
        
        // 获取发送者IP地址（第一次或IP变化时更新）
        std::string senderIP = inet_ntoa(fromAddr.sin_addr);
        if (senderIP != m_senderIPAddress) {
            m_senderIPAddress = senderIP;
            // IP变化时更新标题
            if (m_hDisplayWindow) {
                UpdateWindowTitle();
            }
        }

        // 处理分片
        ProcessFragment(header, reinterpret_cast<BYTE*>(recvBuf + sizeof(FragmentHeader)));

        // 定期清理超时帧
        static auto lastCleanup = steady_clock::now();
        if (duration_cast<milliseconds>(steady_clock::now() - lastCleanup) > milliseconds(100)) {
            CleanupTimedOutFrames();
            lastCleanup = steady_clock::now();
        }
    }

    // 离开组播组
    setsockopt(m_RecvSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));

    // 清理
    closesocket(m_RecvSocket);
    WSACleanup();
    m_RecvSocket = INVALID_SOCKET;
}
