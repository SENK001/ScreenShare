#ifndef RECEIVER_H
#define RECEIVER_H

#include "common.h"
#include <queue>

// 帧重组状态
struct FrameReassemblyState {
    std::vector<BYTE> frameData;
    std::vector<bool> fragmentsReceived;
    uint32_t frameSize;
    uint16_t totalFragments;
    uint16_t receivedCount;
    steady_clock::time_point lastFragmentTime;
};

// 接收器类
class ScreenReceiver {
public:
    ScreenReceiver();
    ~ScreenReceiver();

    // 开始接收
    bool Start(const std::string& multicastGroup, int port, const std::string& localInterface, HWND hParentWnd = NULL);
    
    // 停止接收
    void Stop();
    
    // 检查是否正在接收
    bool IsReceiving() const { return m_bListening; }
    
    // 获取显示窗口句柄
    HWND GetDisplayWindow() const { return m_hDisplayWindow; }

private:
    // 图像显示窗口过程（静态，转发给实例）
    static LRESULT CALLBACK DisplayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    // 实例窗口过程
    LRESULT HandleDisplayWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    // 创建图像显示窗口
    HWND CreateDisplayWindow(HWND hParent);
    
    // 图像处理线程函数
    static DWORD WINAPI ImageProcessingThreadFunc(LPVOID lpParam);
    void ImageProcessingThread();
    
    // 处理接收到的分片
    void ProcessFragment(const FragmentHeader& header, const BYTE* fragmentData);
    
    // 清理超时的帧
    void CleanupTimedOutFrames();
    
    // 更新窗口标题
    void UpdateWindowTitle();
    
    // 接收线程函数
    static DWORD WINAPI RecvThreadFunc(LPVOID lpParam);
    void RecvThread();

    // 窗口相关
    HWND m_hDisplayWindow = NULL;
    bool m_bDisplayWindowTopMost = false;
    
    // 接收参数
    std::string m_multicastGroup;
    int m_port = 0;
    std::string m_localInterface;
    
    // 接收状态
    bool m_bListening = false;
    bool m_bImageProcessing = false;
    HANDLE m_hRecvThread = NULL;
    HANDLE m_hImageProcessingThread = NULL;
    SOCKET m_RecvSocket = INVALID_SOCKET;
    
    // 同步对象
    CRITICAL_SECTION m_ImageCS;
    CRITICAL_SECTION m_ImageQueueCS;
    CRITICAL_SECTION m_FramesCS;
    HANDLE m_hImageQueueEvent = NULL;
    
    // 图像数据
    HBITMAP m_hBitmap = NULL;
    std::map<uint32_t, FrameReassemblyState> m_Frames; // 帧ID到重组状态的映射
    std::queue<std::vector<BYTE>> m_ImageQueue; // 图像处理队列
    
    // FPS和源IP信息
    std::string m_senderIPAddress = ""; // 发送者IP地址
    uint32_t m_frameCount = 0;           // 帧计数
    steady_clock::time_point m_lastFpsUpdateTime; // 上次FPS更新时间
    double m_currentFPS = 0.0;           // 当前FPS
    
    // 超时设置
    static const milliseconds FRAME_TIMEOUT;
};

#endif // RECEIVER_H
