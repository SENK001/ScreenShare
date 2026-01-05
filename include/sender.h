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
    bool CaptureScreenDXGI(std::vector<BYTE>& pngData, FrameType& frameType);
    bool CaptureScreenGDI(std::vector<BYTE>& pngData, FrameType& frameType);
    
    // PNG编码函数
    bool EncodeBitmapToPNG(Bitmap* bitmap, std::vector<BYTE>& pngData);
    
    // 差异检测和编码
    bool DetectAndEncodeDelta(Bitmap* currentFrame, std::vector<BYTE>& deltaData);
    
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
    
    // 关键帧计数器（每N帧发送一次关键帧）
    uint32_t m_framesSinceKeyFrame = 0;
    static constexpr uint32_t KEY_FRAME_INTERVAL = 30; // 每30帧发送一次关键帧
    
    // 上一帧数据（用于差异检测）
    std::vector<BYTE> m_lastFrameData;
    int m_lastFrameWidth = 0;
    int m_lastFrameHeight = 0;

    // DXGI降级策略：计数连续失败次数，只在多次失败后才降级
    int m_dxgiFailureCount = 0;
    static constexpr int DXGI_FAILURE_THRESHOLD = 10; // 连续失败10次才降级
};

#endif // SENDER_H
