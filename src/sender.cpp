#include "sender.h"
#include <chrono>
#include <algorithm>

using namespace std::chrono;

// ==================== HighResolutionTimer ====================
HighResolutionTimer::HighResolutionTimer() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_frequency = static_cast<uint64_t>(freq.QuadPart);
    Reset();
}

void HighResolutionTimer::Reset() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    m_startTime = static_cast<uint64_t>(now.QuadPart);
}

double HighResolutionTimer::GetElapsedSeconds() const {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    uint64_t currentTime = static_cast<uint64_t>(now.QuadPart);
    return static_cast<double>(currentTime - m_startTime) / m_frequency;
}

uint64_t HighResolutionTimer::GetElapsedMicroseconds() const {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    uint64_t currentTime = static_cast<uint64_t>(now.QuadPart);
    return ((currentTime - m_startTime) * 1000000) / m_frequency;
}

// ==================== FrameQueue ====================
FrameQueue::FrameQueue(size_t maxSize) : m_maxSize(maxSize) {}

void FrameQueue::Push(const CapturedFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 如果队列满，移除最旧的帧
    if (m_queue.size() >= m_maxSize) {
        m_queue.pop();
    }
    
    m_queue.push(frame);
    m_cv.notify_one();
}

bool FrameQueue::TryPop(CapturedFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_queue.empty()) {
        return false;
    }
    
    frame = m_queue.front();
    m_queue.pop();
    return true;
}

size_t FrameQueue::Size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void FrameQueue::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty()) {
        m_queue.pop();
    }
}

// ==================== ScreenSender ====================
ScreenSender::ScreenSender() {
    // 构造函数
}

ScreenSender::~ScreenSender() {
    Stop(); // 确保停止发送
}

bool ScreenSender::Start(const std::string& multicastGroup, int port, const std::string& localInterface) {
    if (m_bSending) {
        return false; // 已经在发送
    }

    m_bSending = true;
    m_frameId = 0;
    
    // 启动捕获线程
    m_captureThread = std::thread(&ScreenSender::CaptureThreadFunc, this);
    
    // 启动发送线程
    m_sendThread = std::thread(&ScreenSender::SendThreadFunc, this, multicastGroup, port, localInterface);
    return true;
}

void ScreenSender::Stop() {
    if (!m_bSending) {
        return;
    }

    m_bSending = false;

    // 关闭套接字以中断发送
    if (m_sendSocket != INVALID_SOCKET) {
        closesocket(m_sendSocket);
        m_sendSocket = INVALID_SOCKET;
    }

    // 等待捕获线程结束
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }

    // 等待发送线程结束
    if (m_sendThread.joinable()) {
        m_sendThread.join();
    }

    // 清理DXGI资源
    if (m_frameAvailableEvent) {
        CloseHandle(m_frameAvailableEvent);
        m_frameAvailableEvent = nullptr;
    }
    if (m_stagingTexture) {
        m_stagingTexture->Release();
        m_stagingTexture = nullptr;
    }
    if (m_deskDupl) {
        m_deskDupl->Release();
        m_deskDupl = nullptr;
    }
    if (m_d3dContext) {
        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }
    if (m_d3dDevice) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }
    
    // 清空帧队列
    m_frameQueue.Clear();
}

bool ScreenSender::InitDXGIDuplication() {
    HRESULT hr = S_OK;

    // 创建D3D设备
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    D3D_FEATURE_LEVEL featureLevel;

    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, _countof(featureLevels), D3D11_SDK_VERSION,
        &m_d3dDevice, &featureLevel, &m_d3dContext);

    if (FAILED(hr)) {
        return false;
    }

    // 获取DXGI设备
    IDXGIDevice* dxgiDevice = nullptr;
    hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
        m_d3dContext->Release();
        m_d3dContext = nullptr;
        return false;
    }

    // 获取DXGI适配器
    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));
    dxgiDevice->Release();
    if (FAILED(hr)) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
        m_d3dContext->Release();
        m_d3dContext = nullptr;
        return false;
    }

    // 获取DXGI输出
    IDXGIOutput* dxgiOutput = nullptr;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    dxgiAdapter->Release();
    if (FAILED(hr)) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
        m_d3dContext->Release();
        m_d3dContext = nullptr;
        return false;
    }

    // 获取DXGI输出1
    IDXGIOutput1* dxgiOutput1 = nullptr;
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgiOutput1));
    dxgiOutput->Release();
    if (FAILED(hr)) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
        m_d3dContext->Release();
        m_d3dContext = nullptr;
        return false;
    }

    // 创建桌面复制接口
    hr = dxgiOutput1->DuplicateOutput(m_d3dDevice, &m_deskDupl);
    dxgiOutput1->Release();
    if (FAILED(hr)) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
        m_d3dContext->Release();
        m_d3dContext = nullptr;
        return false;
    }

    // 获取输出复制描述
    DXGI_OUTDUPL_DESC outputDuplDesc;
    m_deskDupl->GetDesc(&outputDuplDesc);

    // 保存输出矩形用于光标绘制位置调整
    m_outputRect.left = 0;
    m_outputRect.top = 0;
    m_outputRect.right = outputDuplDesc.ModeDesc.Width;
    m_outputRect.bottom = outputDuplDesc.ModeDesc.Height;

    // 创建暂存纹理
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = outputDuplDesc.ModeDesc.Width;
    desc.Height = outputDuplDesc.ModeDesc.Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = outputDuplDesc.ModeDesc.Format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool ScreenSender::CaptureScreenDXGI(std::vector<BYTE>& jpegData) {
    if (!m_deskDupl) {
        return false;
    }

    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    // 获取帧（使用0超时立即返回）
    HRESULT hr = m_deskDupl->AcquireNextFrame(0, &frameInfo, &desktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // 没有新帧，这是正常情况
        return false;
    }
    if (FAILED(hr)) {
        return false;
    }

    // 获取纹理
    ID3D11Texture2D* desktopTexture = nullptr;
    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&desktopTexture));
    desktopResource->Release();
    if (FAILED(hr)) {
        m_deskDupl->ReleaseFrame();
        return false;
    }

    // 获取输出复制描述
    DXGI_OUTDUPL_DESC outputDuplDesc;
    m_deskDupl->GetDesc(&outputDuplDesc);

    // 复制到暂存纹理
    m_d3dContext->CopyResource(m_stagingTexture, desktopTexture);
    desktopTexture->Release();
    m_deskDupl->ReleaseFrame();

    // 映射暂存纹理
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_d3dContext->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        return false;
    }

    // 创建GDI+位图
    Bitmap bitmap(outputDuplDesc.ModeDesc.Width,
        outputDuplDesc.ModeDesc.Height,
        mapped.RowPitch, PixelFormat32bppARGB, (BYTE*)mapped.pData);

    // 绘制鼠标光标
    CURSORINFO cursorInfo = { 0 };
    cursorInfo.cbSize = sizeof(cursorInfo);
    if (GetCursorInfo(&cursorInfo) && (cursorInfo.flags & CURSOR_SHOWING)) {
        Graphics graphics(&bitmap);

        // 获取光标图标
        ICONINFO iconInfo = { 0 };
        if (GetIconInfo(cursorInfo.hCursor, &iconInfo)) {
            // 使用RAII确保资源释放
            struct IconInfoCleanup {
                ICONINFO& info;
                ~IconInfoCleanup() {
                    if (info.hbmMask) DeleteObject(info.hbmMask);
                    if (info.hbmColor) DeleteObject(info.hbmColor);
                }
            } cleanup{ iconInfo };

            // 计算光标位置（调整热点偏移）
            int x = cursorInfo.ptScreenPos.x - iconInfo.xHotspot - m_outputRect.left;
            int y = cursorInfo.ptScreenPos.y - iconInfo.yHotspot - m_outputRect.top;

            // 绘制光标
            HDC hdc = graphics.GetHDC();
            DrawIconEx(hdc, x, y, cursorInfo.hCursor, 0, 0, 0, NULL, DI_NORMAL);
            graphics.ReleaseHDC(hdc);
        }
    }

    // 编码为JPEG
    IStream* stream = NULL;
    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (FAILED(hr) || !stream) {
        m_d3dContext->Unmap(m_stagingTexture, 0);
        return false;
    }

    CLSID clsid;
    if (!GetEncoderClsid(L"image/jpeg", &clsid)) {
        stream->Release();
        m_d3dContext->Unmap(m_stagingTexture, 0);
        return false;
    }

    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;

    ULONG quality = GetQuality();
    encoderParams.Parameter[0].Value = &quality;

    Status status = bitmap.Save(stream, &clsid, &encoderParams);
    if (status != Ok) {
        stream->Release();
        m_d3dContext->Unmap(m_stagingTexture, 0);
        return false;
    }

    // 获取流数据
    STATSTG stats;
    hr = stream->Stat(&stats, STATFLAG_NONAME);
    if (FAILED(hr)) {
        stream->Release();
        m_d3dContext->Unmap(m_stagingTexture, 0);
        return false;
    }
    DWORD streamSize = stats.cbSize.LowPart;

    HGLOBAL hGlobal = NULL;
    hr = GetHGlobalFromStream(stream, &hGlobal);
    if (FAILED(hr) || !hGlobal) {
        stream->Release();
        m_d3dContext->Unmap(m_stagingTexture, 0);
        return false;
    }

    BYTE* pData = (BYTE*)GlobalLock(hGlobal);
    if (!pData) {
        stream->Release();
        m_d3dContext->Unmap(m_stagingTexture, 0);
        return false;
    }

    jpegData.resize(streamSize);
    memcpy(jpegData.data(), pData, streamSize);

    GlobalUnlock(hGlobal);
    stream->Release();
    m_d3dContext->Unmap(m_stagingTexture, 0);

    return true;
}

// 捕获线程函数：持续捕获屏幕并将帧存放在队列中
void ScreenSender::CaptureThreadFunc() {
    // 初始化DXGI
    if (!InitDXGIDuplication()) {
        return;
    }

    // 性能统计变量
    uint32_t framesCaptured = 0;
    HighResolutionTimer statsTimer;
    statsTimer.Reset();

    while (m_bSending) {
        std::vector<BYTE> jpegData;
        
        // 尝试捕获屏幕
        if (CaptureScreenDXGI(jpegData)) {
            // 创建帧对象
            CapturedFrame frame;
            frame.jpegData = std::move(jpegData);
            frame.frameId = ++m_frameId;
            frame.timestamp = high_resolution_clock::now();
            
            // 将帧推送到队列（如果队列满会自动丢弃最旧的帧）
            m_frameQueue.Push(frame);
            
            // 更新性能统计
            framesCaptured++;
            
            // 每秒输出一次捕获帧率统计
            if (statsTimer.GetElapsedSeconds() >= 1.0) {
                double captureFPS = framesCaptured / statsTimer.GetElapsedSeconds();
                // 调试输出：实际捕获帧率
                // printf("捕获帧率: %.1f FPS\n", captureFPS);
                framesCaptured = 0;
                statsTimer.Reset();
            }
        } else {
            // 没有新帧时短暂等待，避免CPU占用过高
            Sleep(1);
        }
    }
}

void ScreenSender::SendThreadFunc(const std::string& multicastGroup, int port, const std::string& localInterface) {
    WSADATA wsaData;
    sockaddr_in multicastAddr;
    int ttl = 1; // 生存时间

    // 初始化Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return;
    }

    // 创建UDP套接字
    m_sendSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sendSocket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    // 设置TTL
    if (setsockopt(m_sendSocket, IPPROTO_IP, IP_MULTICAST_TTL,
        (char*)&ttl, sizeof(ttl)) == SOCKET_ERROR) {
        closesocket(m_sendSocket);
        WSACleanup();
        return;
    }

    // 设置出口接口
    in_addr interfaceAddr;
    interfaceAddr.s_addr = inet_addr(localInterface.c_str());
    if (setsockopt(m_sendSocket, IPPROTO_IP, IP_MULTICAST_IF,
        (char*)&interfaceAddr, sizeof(interfaceAddr)) == SOCKET_ERROR) {
        closesocket(m_sendSocket);
        WSACleanup();
        return;
    }

    // 设置组播地址和端口
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_addr.s_addr = inet_addr(multicastGroup.c_str());
    multicastAddr.sin_port = htons(port);

    // 发送缓冲区
    std::vector<char> sendBuffer(sizeof(FragmentHeader) + MAX_FRAGMENT_SIZE);

    // 使用高精度计时器
    HighResolutionTimer frameRateTimer;
    frameRateTimer.Reset();

    // 性能统计变量
    uint32_t framesSent = 0;
    HighResolutionTimer statsTimer;
    statsTimer.Reset();

    while (m_bSending) {
        // 获取配置的帧率
        int currentFrameRate = GetFrameRate();
        double frameIntervalMicroseconds = 1000000.0 / currentFrameRate;

        // 检查帧率控制：确保帧间隔时间已过
        uint64_t elapsedMicroseconds = frameRateTimer.GetElapsedMicroseconds();
        if (elapsedMicroseconds < static_cast<uint64_t>(frameIntervalMicroseconds)) {
            uint64_t remainingMicroseconds = static_cast<uint64_t>(frameIntervalMicroseconds) - elapsedMicroseconds;
            // 使用更精确的等待方法
            if (remainingMicroseconds >= 1000) {
                // 毫秒级等待
                DWORD sleepTime = static_cast<DWORD>(remainingMicroseconds / 1000);
                Sleep(sleepTime);
            } else if (remainingMicroseconds > 0) {
                // 微秒级等待，使用高精度忙等待
                LARGE_INTEGER start, end, freq;
                QueryPerformanceFrequency(&freq);
                QueryPerformanceCounter(&start);
                double waitSeconds = remainingMicroseconds / 1000000.0;
                double waitTicks = waitSeconds * freq.QuadPart;
                
                do {
                    QueryPerformanceCounter(&end);
                } while ((end.QuadPart - start.QuadPart) < waitTicks);
            }
        }

        // 从队列中尝试取一帧
        CapturedFrame frame;
        if (m_frameQueue.TryPop(frame)) {
            // 发送帧的每个分片
            size_t frameSize = frame.jpegData.size();
            uint16_t totalFragments = static_cast<uint16_t>((frameSize + MAX_FRAGMENT_SIZE - 1) / MAX_FRAGMENT_SIZE);

            for (uint16_t fragmentIndex = 0; fragmentIndex < totalFragments; fragmentIndex++) {
                // 计算当前分片的大小和偏移量
                size_t offset = fragmentIndex * MAX_FRAGMENT_SIZE;
                uint16_t fragmentSize = static_cast<uint16_t>(
                    std::min(MAX_FRAGMENT_SIZE, frameSize - offset));

                // 准备分片头
                FragmentHeader header;
                header.magic = htons(FRAGMENT_MAGIC);
                header.frameId = htonl(frame.frameId);
                header.frameSize = htonl(static_cast<uint32_t>(frameSize));
                header.totalFragments = htons(totalFragments);
                header.fragmentIndex = htons(fragmentIndex);
                header.fragmentSize = htons(fragmentSize);

                // 复制头和数据到发送缓冲区
                memcpy(sendBuffer.data(), &header, sizeof(FragmentHeader));
                memcpy(sendBuffer.data() + sizeof(FragmentHeader),
                    frame.jpegData.data() + offset, fragmentSize);

                // 发送分片
                int sent = sendto(m_sendSocket, sendBuffer.data(),
                    sizeof(FragmentHeader) + fragmentSize,
                    0, (sockaddr*)&multicastAddr, sizeof(multicastAddr));

                if (sent == SOCKET_ERROR) {
                    continue;
                }
            }

            // 更新性能统计
            framesSent++;
            
            // 重置计时器以开始新的帧间隔
            frameRateTimer.Reset();
        } else {
            // 没有可用的帧，短暂等待
            Sleep(1);
        }

        // 每秒输出一次发送帧率统计
        if (statsTimer.GetElapsedSeconds() >= 1.0) {
            double sendFPS = framesSent / statsTimer.GetElapsedSeconds();
            // 调试输出：实际发送帧率
            // printf("发送帧率: %.1f FPS (目标: %d FPS)\n", sendFPS, currentFrameRate);
            framesSent = 0;
            statsTimer.Reset();
        }
    }

    // 清理网络资源
    closesocket(m_sendSocket);
    WSACleanup();
    m_sendSocket = INVALID_SOCKET;
}

