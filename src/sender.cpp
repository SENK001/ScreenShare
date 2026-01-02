#include "sender.h"
#include <chrono>

using namespace std::chrono;

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

    // 等待线程结束
    if (m_sendThread.joinable()) {
        m_sendThread.join();
    }

    // 清理DXGI资源
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

    // 重置失败计数器
    m_dxgiFailureCount = 0;
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

    // 获取DXGI输出1 - 关键修复：使用 IDXGIOutput1 而不是变量名 dxgiOutput1
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

    // 获取帧
    HRESULT hr = m_deskDupl->AcquireNextFrame(0, &frameInfo, &desktopResource);
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
        ICONINFO iconInfo;
        if (GetIconInfo(cursorInfo.hCursor, &iconInfo)) {
            // 计算光标位置（调整热点偏移）
            int x = cursorInfo.ptScreenPos.x - iconInfo.xHotspot - m_outputRect.left;
            int y = cursorInfo.ptScreenPos.y - iconInfo.yHotspot - m_outputRect.top;

            // 绘制光标
            DrawIconEx(graphics.GetHDC(), x, y, cursorInfo.hCursor, 0, 0, 0, NULL, DI_NORMAL);
            graphics.ReleaseHDC(graphics.GetHDC());

            // 清理资源
            if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
            if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        }
    }

    // 编码为JPEG
    IStream* stream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);

    CLSID clsid;
    GetEncoderClsid(L"image/jpeg", &clsid);

    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;

    ULONG quality = 95;
    encoderParams.Parameter[0].Value = &quality;

    bitmap.Save(stream, &clsid, &encoderParams);

    // 获取流数据
    STATSTG stats;
    stream->Stat(&stats, STATFLAG_NONAME);
    DWORD streamSize = stats.cbSize.LowPart;

    HGLOBAL hGlobal = NULL;
    GetHGlobalFromStream(stream, &hGlobal);
    BYTE* pData = (BYTE*)GlobalLock(hGlobal);

    jpegData.resize(streamSize);
    memcpy(jpegData.data(), pData, streamSize);

    GlobalUnlock(hGlobal);
    stream->Release();
    m_d3dContext->Unmap(m_stagingTexture, 0);

    return true;
}

bool ScreenSender::CaptureScreenGDI(std::vector<BYTE>& jpegData) {
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hBitmap);

    // 使用BitBlt而不是PrintWindow，减少闪烁
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);

    // 绘制鼠标光标
    CURSORINFO cursorInfo = { 0 };
    cursorInfo.cbSize = sizeof(cursorInfo);
    if (GetCursorInfo(&cursorInfo) && (cursorInfo.flags & CURSOR_SHOWING)) {
        // 获取光标图标
        ICONINFO iconInfo;
        if (GetIconInfo(cursorInfo.hCursor, &iconInfo)) {
            // 计算光标位置（调整热点偏移）
            int x = cursorInfo.ptScreenPos.x - iconInfo.xHotspot;
            int y = cursorInfo.ptScreenPos.y - iconInfo.yHotspot;

            // 绘制光标
            DrawIconEx(hdcMem, x, y, cursorInfo.hCursor, 0, 0, 0, NULL, DI_NORMAL);

            // 清理资源
            if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
            if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        }
    }

    // 使用GDI+将位图转换为JPEG
    Bitmap bitmap(hBitmap, NULL);

    // 创建内存流
    IStream* stream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);

    // 编码参数 - 设置JPEG质量
    CLSID clsid;
    GetEncoderClsid(L"image/jpeg", &clsid);

    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;

    // 设置质量（0-100，越高质量越好但文件越大）
    ULONG quality = 95;
    encoderParams.Parameter[0].Value = &quality;

    // 保存为JPEG到内存流
    bitmap.Save(stream, &clsid, &encoderParams);

    // 获取流大小
    STATSTG stats;
    stream->Stat(&stats, STATFLAG_NONAME);
    DWORD streamSize = stats.cbSize.LowPart;

    // 读取流数据
    HGLOBAL hGlobal = NULL;
    GetHGlobalFromStream(stream, &hGlobal);
    BYTE* pData = (BYTE*)GlobalLock(hGlobal);

    jpegData.resize(streamSize);
    memcpy(jpegData.data(), pData, streamSize);

    // 清理
    GlobalUnlock(hGlobal);
    stream->Release();
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return true;
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

    // 帧ID计数器
    m_frameId = 0;

    // 30FPS的帧间隔（约33.3毫秒）
    const milliseconds frameInterval(33);

    // 发送缓冲区
    std::vector<char> sendBuffer(sizeof(FragmentHeader) + MAX_FRAGMENT_SIZE);

    // 尝试初始化DXGI桌面复制
    bool useDXGI = InitDXGIDuplication();

    while (m_bSending) {
        auto startTime = high_resolution_clock::now();

        // 递增帧ID
        m_frameId++;

        // 捕获屏幕并转换为JPEG
        std::vector<BYTE> jpegData;
        bool captureSuccess = false;

        if (useDXGI) {
            captureSuccess = CaptureScreenDXGI(jpegData);
            
            // 改进的降级策略：根据连续失败次数决定是否降级
            if (!captureSuccess) {
                m_dxgiFailureCount++;
                
                // 连续失败次数超过阈值才永久降级
                if (m_dxgiFailureCount >= DXGI_FAILURE_THRESHOLD) {
                    useDXGI = false;
                    // 清理DXGI资源
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
                }
            } else {
                // 成功捕获，重置失败计数
                m_dxgiFailureCount = 0;
            }
        }

        // 如果失败，则使用GDI
        if (!captureSuccess) {
            captureSuccess = CaptureScreenGDI(jpegData);
        }

        if (captureSuccess) {
            // 计算需要多少分片
            size_t frameSize = jpegData.size();
            uint16_t totalFragments = static_cast<uint16_t>((frameSize + MAX_FRAGMENT_SIZE - 1) / MAX_FRAGMENT_SIZE);

            // 发送每个分片
            for (uint16_t fragmentIndex = 0; fragmentIndex < totalFragments; fragmentIndex++) {
                // 计算当前分片的大小和偏移量
                size_t offset = fragmentIndex * MAX_FRAGMENT_SIZE;
                uint16_t fragmentSize = static_cast<uint16_t>(
                    std::min(MAX_FRAGMENT_SIZE, frameSize - offset));

                // 准备分片头
                FragmentHeader header;
                header.frameId = htonl(m_frameId);            // 添加帧ID
                header.frameSize = htonl(static_cast<uint32_t>(frameSize));
                header.totalFragments = htons(totalFragments);
                header.fragmentIndex = htons(fragmentIndex);
                header.fragmentSize = htons(fragmentSize);

                // 复制头和数据到发送缓冲区
                memcpy(sendBuffer.data(), &header, sizeof(FragmentHeader));
                memcpy(sendBuffer.data() + sizeof(FragmentHeader),
                    jpegData.data() + offset, fragmentSize);

                // 发送分片
                int sent = sendto(m_sendSocket, sendBuffer.data(),
                    sizeof(FragmentHeader) + fragmentSize,
                    0, (sockaddr*)&multicastAddr, sizeof(multicastAddr));

                if (sent == SOCKET_ERROR) {
                    // 错误处理，但继续发送下一个分片
                    continue;
                }
            }
        }

        // 计算剩余时间并等待
        auto endTime = high_resolution_clock::now();
        auto elapsed = duration_cast<milliseconds>(endTime - startTime);

        if (elapsed < frameInterval) {
            Sleep(static_cast<DWORD>((frameInterval - elapsed).count()));
        }
    }

    // 清理网络资源
    closesocket(m_sendSocket);
    WSACleanup();
    m_sendSocket = INVALID_SOCKET;
}
