#ifndef SENDER_H
#define SENDER_H

#include "common.h"
#include <thread>
#include <mutex>
#include <d3d11.h>
#include <dxgi1_2.h>

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

    // DXGI降级策略：计数连续失败次数，只在多次失败后才降级
    int m_dxgiFailureCount = 0;
    static constexpr int DXGI_FAILURE_THRESHOLD = 10; // 连续失败10次才降级

    // 可配置参数
    int m_quality = 95;      // JPEG质量 (0-100)
    int m_frameRate = 30;    // 帧率 (1-60 FPS)
    mutable std::mutex m_paramMutex; // 参数访问互斥锁

public:
    // 设置JPEG质量 (0-100)
    void SetQuality(int quality) {
        std::lock_guard<std::mutex> lock(m_paramMutex);
        m_quality = std::max(0, std::min(100, quality));
    }

    // 设置帧率 (1-60 FPS)
    void SetFrameRate(int frameRate) {
        std::lock_guard<std::mutex> lock(m_paramMutex);
        m_frameRate = std::max(1, std::min(60, frameRate));
    }

    // 获取当前质量
    int GetQuality() const {
        std::lock_guard<std::mutex> lock(m_paramMutex);
        return m_quality;
    }

    // 获取当前帧率
    int GetFrameRate() const {
        std::lock_guard<std::mutex> lock(m_paramMutex);
        return m_frameRate;
    }
};

#endif // SENDER_H
