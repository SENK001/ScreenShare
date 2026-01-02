#ifndef SENDER_H
#define SENDER_H

#include "common.h"
#include <thread>
#include <mutex>
#include <d3d11.h>
#include <dxgi1_2.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// 发送器类
class ScreenSender {
public:
    ScreenSender();
    ~ScreenSender();

    // 开始发送
    bool Start(const std::string& multicastGroup, int port, const std::string& localInterface);
    
    // 停止发送
    void Stop();
    
    // 检查是否正在发送
    bool IsSending() const { return m_bSending; }

private:
    // DXGI相关函数
    bool InitDXGIDuplication();
    bool CaptureScreenDXGI(std::vector<BYTE>& jpegData);
    bool CaptureScreenGDI(std::vector<BYTE>& jpegData);
    
    // 发送线程函数
    void SendThreadFunc(const std::string& multicastGroup, int port, const std::string& localInterface);

    // DXGI相关变量
    ID3D11Device* m_d3dDevice = nullptr;
    ID3D11DeviceContext* m_d3dContext = nullptr;
    IDXGIOutputDuplication* m_deskDupl = nullptr;
    ID3D11Texture2D* m_stagingTexture = nullptr;
    RECT m_outputRect = { 0 };

    // 发送状态
    bool m_bSending = false;
    std::thread m_sendThread;
    SOCKET m_sendSocket = INVALID_SOCKET;
    
    // 帧ID计数器
    uint32_t m_frameId = 0;
};

#endif // SENDER_H
