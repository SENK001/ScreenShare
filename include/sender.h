#ifndef SENDER_H
#define SENDER_H

#include "common.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <chrono>

// 高精度计时器类
class HighResolutionTimer {
public:
    HighResolutionTimer();
    void Reset();
    double GetElapsedSeconds() const;
    uint64_t GetElapsedMicroseconds() const;
    uint64_t GetFrequency() const { return m_frequency; }

private:
    uint64_t m_frequency;
    uint64_t m_startTime;
};

// 帧数据结构
struct CapturedFrame {
    std::vector<BYTE> jpegData;
    uint32_t frameId;
    std::chrono::high_resolution_clock::time_point timestamp;
};

// 线程安全的帧缓存队列（最多2帧以减少延迟）
class FrameQueue {
public:
    FrameQueue(size_t maxSize = 2);
    
    // 推送帧，如果队列满则丢弃最旧的帧
    void Push(const CapturedFrame& frame);
    
    // 尝试弹出帧
    bool TryPop(CapturedFrame& frame);
    
    // 获取队列大小
    size_t Size() const;
    
    // 清空队列
    void Clear();

private:
    std::queue<CapturedFrame> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    size_t m_maxSize;
};

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
    
    // 捕获线程函数
    void CaptureThreadFunc();
    
    // 发送线程函数
    void SendThreadFunc(const std::string& multicastGroup, int port, const std::string& localInterface);

    // DXGI相关变量
    ID3D11Device* m_d3dDevice = nullptr;
    ID3D11DeviceContext* m_d3dContext = nullptr;
    IDXGIOutputDuplication* m_deskDupl = nullptr;
    ID3D11Texture2D* m_stagingTexture = nullptr;
    RECT m_outputRect = { 0 };
    HANDLE m_frameAvailableEvent = nullptr;

    // 发送状态
    bool m_bSending = false;
    std::thread m_captureThread;
    std::thread m_sendThread;
    SOCKET m_sendSocket = INVALID_SOCKET;
    
    // 帧缓存队列
    FrameQueue m_frameQueue;
    
    // 帧ID计数器
    uint32_t m_frameId = 0;

    // 可配置参数
    int m_quality = 95;      // JPEG质量 (0-100)
    int m_frameRate = 30;    // 帧率 (1-60 FPS)
    int m_resolutionWidth = 0;  // 分辨率宽度 (0表示使用屏幕分辨率)
    int m_resolutionHeight = 0; // 分辨率高度 (0表示使用屏幕分辨率)
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

    // 设置分辨率 (0表示使用屏幕分辨率)
    void SetResolution(int width, int height) {
        std::lock_guard<std::mutex> lock(m_paramMutex);
        m_resolutionWidth = std::max(0, width);
        m_resolutionHeight = std::max(0, height);
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

    // 获取当前分辨率
    void GetResolution(int& width, int& height) const {
        std::lock_guard<std::mutex> lock(m_paramMutex);
        width = m_resolutionWidth;
        height = m_resolutionHeight;
    }
};

#endif // SENDER_H
