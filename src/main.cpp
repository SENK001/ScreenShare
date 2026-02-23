#include "common.h"
#include "sender.h"
#include "receiver.h"
#include "resource.h"
#include <commctrl.h>

// 全局变量
ScreenSender g_sender;
ScreenReceiver g_receiver;

// 刷子句柄
HBRUSH g_hBrushBackground = NULL;

// 控件句柄
HWND g_hWnd = NULL;
HWND g_hTabControl = NULL;
HFONT g_hDefaultFont = NULL;
float g_dpiScale = 1.0f;

// 发送页控件
HWND g_hSendAdapterLabel = NULL;
HWND g_hComboBoxSendAdapters = NULL;
HWND g_hSendAddrLabel = NULL;
HWND g_hEditSendMulticastAddr = NULL;
HWND g_hSendPortLabel = NULL;
HWND g_hEditSendPort = NULL;
HWND g_hButtonSendStart = NULL;
HWND g_hButtonSendStop = NULL;

// 接收页控件
HWND g_hRecvAdapterLabel = NULL;
HWND g_hComboBoxRecvAdapters = NULL;
HWND g_hRecvAddrLabel = NULL;
HWND g_hEditRecvMulticastAddr = NULL;
HWND g_hRecvPortLabel = NULL;
HWND g_hEditRecvPort = NULL;
HWND g_hButtonRecvStart = NULL;
HWND g_hButtonRecvStop = NULL;

// 设置页控件
HWND g_hQualityLabel = NULL;
HWND g_hQualityTrackbar = NULL;
HWND g_hQualityValueLabel = NULL;
HWND g_hFrameRateLabel = NULL;
HWND g_hFrameRateTrackbar = NULL;
HWND g_hFrameRateValueLabel = NULL;
HWND g_hResolutionLabel = NULL;
HWND g_hResolutionValueLabel = NULL;
HWND g_hBandwidthLabel = NULL;
HWND g_hBandwidthValueLabel = NULL;
HWND g_hApplySettingsButton = NULL;

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// DPI相关函数
int ScaleDPI(int value);
float GetDPIScale(HWND hWnd);

// 创建并设置字体
void CreateDefaultFont();

// DPI相关函数
int ScaleDPI(int value) {
    return static_cast<int>(value * g_dpiScale);
}

float GetDPIScale(HWND hWnd) {
    HDC hdc = GetDC(hWnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hWnd, hdc);
    return dpi / 96.0f;
}

// 创建并设置字体
void CreateDefaultFont() {
    if (g_hDefaultFont) {
        DeleteObject(g_hDefaultFont);
    }
    
    NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    
    // 根据DPI调整字体大小
    ncm.lfMessageFont.lfHeight = static_cast<LONG>(ncm.lfMessageFont.lfHeight * g_dpiScale);
    g_hDefaultFont = CreateFontIndirect(&ncm.lfMessageFont);
}

// 初始化通用控件
void SetupCommonControls() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);
}

// 创建主窗口
HWND CreateMainWindow(HINSTANCE hInstance) {
    // 注册窗口类（使用WNDCLASSEX支持大小图标）
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ScreenShareAppClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    
    // 加载大图标（32x32或更大，根据DPI自动选择）
    wc.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAINICON), 
                                IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    // 加载小图标（16x16或更大，根据DPI自动选择）
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAINICON),
                                  IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), 
                                  GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"窗口注册失败", L"错误", MB_ICONERROR);
        return NULL;
    }

    // 创建窗口
    HWND hWnd = CreateWindow(
        L"ScreenShareAppClass",
        L"屏幕共享工具",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL, NULL, hInstance, NULL);

    return hWnd;
}

// 创建控件
void CreateControls(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    // 创建标签控件 - 调整大小使右下也有边距
    g_hTabControl = CreateWindow(WC_TABCONTROL, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        ScaleDPI(10), ScaleDPI(10), rc.right - ScaleDPI(20), rc.bottom - ScaleDPI(20),
        hWnd, NULL, NULL, NULL);
    
    SendMessage(g_hTabControl, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    
    // 添加标签页
    TCITEM tie;
    tie.mask = TCIF_TEXT;
    
    tie.pszText = (LPWSTR)L"发送屏幕";
    TabCtrl_InsertItem(g_hTabControl, 0, &tie);
    
    tie.pszText = (LPWSTR)L"接收屏幕";
    TabCtrl_InsertItem(g_hTabControl, 1, &tie);
    
    tie.pszText = (LPWSTR)L"设置";
    TabCtrl_InsertItem(g_hTabControl, 2, &tie);
    
    // 获取标签内容区域
    RECT rcTab;
    GetClientRect(g_hTabControl, &rcTab);
    TabCtrl_AdjustRect(g_hTabControl, FALSE, &rcTab);
    
    int tabLeft = rcTab.left + ScaleDPI(20);
    int tabTop = rcTab.top + ScaleDPI(20);
    int tabWidth = rcTab.right - rcTab.left - ScaleDPI(20);
    
    // ========== 发送控制面板 ==========
    
    // 发送控制 - 网络适配器
    g_hSendAdapterLabel = CreateWindow(L"STATIC", L"网络适配器：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        tabLeft, tabTop, ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    g_hComboBoxSendAdapters = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        tabLeft + ScaleDPI(110), tabTop, tabWidth - ScaleDPI(110), ScaleDPI(200), hWnd, NULL, NULL, NULL);
    
    // 发送控制 - 组播地址
    g_hSendAddrLabel = CreateWindow(L"STATIC", L"组播地址：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        tabLeft, tabTop + ScaleDPI(40), ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    g_hEditSendMulticastAddr = CreateWindow(L"EDIT", L"239.255.0.1", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(40), tabWidth - ScaleDPI(110), ScaleDPI(23), hWnd, NULL, NULL, NULL);

    // 发送控制 - 端口
    g_hSendPortLabel = CreateWindow(L"STATIC", L"端口：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        tabLeft, tabTop + ScaleDPI(80), ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    g_hEditSendPort = CreateWindow(L"EDIT", L"9277", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(80), ScaleDPI(150), ScaleDPI(23), hWnd, NULL, NULL, NULL);
    
    // 发送控制 - 按钮
    g_hButtonSendStart = CreateWindow(L"BUTTON", L"开始发送", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(120), ScaleDPI(120), ScaleDPI(32), hWnd, (HMENU)1, NULL, NULL);
    g_hButtonSendStop = CreateWindow(L"BUTTON", L"停止发送", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        tabLeft + ScaleDPI(240), tabTop + ScaleDPI(120), ScaleDPI(120), ScaleDPI(32), hWnd, (HMENU)2, NULL, NULL);
    EnableWindow(g_hButtonSendStop, FALSE);
    
    // 设置发送控件字体
    SendMessage(g_hSendAdapterLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hComboBoxSendAdapters, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hSendAddrLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hEditSendMulticastAddr, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hSendPortLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hEditSendPort, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hButtonSendStart, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hButtonSendStop, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    
    // ========== 接收控制面板 ==========
    
    // 接收控制 - 网络适配器
    g_hRecvAdapterLabel = CreateWindow(L"STATIC", L"网络适配器：", WS_CHILD | SS_LEFT,
        tabLeft, tabTop, ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    g_hComboBoxRecvAdapters = CreateWindow(L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        tabLeft + ScaleDPI(110), tabTop, tabWidth - ScaleDPI(110), ScaleDPI(200), hWnd, NULL, NULL, NULL);

    // 接收控制 - 组播地址
    g_hRecvAddrLabel = CreateWindow(L"STATIC", L"组播地址：", WS_CHILD | SS_LEFT,
        tabLeft, tabTop + ScaleDPI(40), ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    g_hEditRecvMulticastAddr = CreateWindow(L"EDIT", L"239.255.0.1", WS_CHILD | WS_BORDER | ES_LEFT,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(40), tabWidth - ScaleDPI(110), ScaleDPI(23), hWnd, NULL, NULL, NULL);

    // 接收控制 - 端口
    g_hRecvPortLabel = CreateWindow(L"STATIC", L"端口：", WS_CHILD | SS_LEFT,
        tabLeft, tabTop + ScaleDPI(80), ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    g_hEditRecvPort = CreateWindow(L"EDIT", L"9277", WS_CHILD | WS_BORDER | ES_LEFT,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(80), ScaleDPI(150), ScaleDPI(23), hWnd, NULL, NULL, NULL);
    
    // 接收控制 - 按钮
    g_hButtonRecvStart = CreateWindow(L"BUTTON", L"开始接收", WS_CHILD | BS_PUSHBUTTON,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(120), ScaleDPI(120), ScaleDPI(32), hWnd, (HMENU)3, NULL, NULL);
    g_hButtonRecvStop = CreateWindow(L"BUTTON", L"停止接收", WS_CHILD | BS_PUSHBUTTON,
        tabLeft + ScaleDPI(240), tabTop + ScaleDPI(120), ScaleDPI(120), ScaleDPI(32), hWnd, (HMENU)4, NULL, NULL);
    EnableWindow(g_hButtonRecvStop, FALSE);
    
    // 设置接收控件字体
    SendMessage(g_hRecvAdapterLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hComboBoxRecvAdapters, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hRecvAddrLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hEditRecvMulticastAddr, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hRecvPortLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hEditRecvPort, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hButtonRecvStart, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hButtonRecvStop, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    
    // 默认显示发送页面（发送控件可见，接收控件隐藏）
    ShowWindow(g_hRecvAdapterLabel, SW_HIDE);
    ShowWindow(g_hComboBoxRecvAdapters, SW_HIDE);
    ShowWindow(g_hRecvAddrLabel, SW_HIDE);
    ShowWindow(g_hEditRecvMulticastAddr, SW_HIDE);
    ShowWindow(g_hRecvPortLabel, SW_HIDE);
    ShowWindow(g_hEditRecvPort, SW_HIDE);
    ShowWindow(g_hButtonRecvStart, SW_HIDE);
    ShowWindow(g_hButtonRecvStop, SW_HIDE);
    
    // ========== 设置控制面板 ==========
    
    // 图片质量设置
    g_hQualityLabel = CreateWindow(L"STATIC", L"图片质量：", WS_CHILD | SS_LEFT,
        tabLeft, tabTop, ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    g_hQualityTrackbar = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        tabLeft + ScaleDPI(110), tabTop, tabWidth - ScaleDPI(160), ScaleDPI(30), hWnd, NULL, NULL, NULL);
    g_hQualityValueLabel = CreateWindow(L"STATIC", L"95", WS_CHILD | SS_LEFT,
        tabLeft + tabWidth - ScaleDPI(40), tabTop, ScaleDPI(40), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    
    // 设置质量滑块范围 (0-100)
    SendMessage(g_hQualityTrackbar, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(g_hQualityTrackbar, TBM_SETPOS, TRUE, 95);
    SendMessage(g_hQualityTrackbar, TBM_SETTICFREQ, 10, 0);
    
    // 帧率设置
    g_hFrameRateLabel = CreateWindow(L"STATIC", L"帧率 (FPS)：", WS_CHILD | SS_LEFT,
        tabLeft, tabTop + ScaleDPI(40), ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    g_hFrameRateTrackbar = CreateWindow(TRACKBAR_CLASS, L"", WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(40), tabWidth - ScaleDPI(160), ScaleDPI(30), hWnd, NULL, NULL, NULL);
    g_hFrameRateValueLabel = CreateWindow(L"STATIC", L"30", WS_CHILD | SS_LEFT,
        tabLeft + tabWidth - ScaleDPI(40), tabTop + ScaleDPI(40), ScaleDPI(40), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    
    // 设置帧率滑块范围 (1-60)
    SendMessage(g_hFrameRateTrackbar, TBM_SETRANGE, TRUE, MAKELONG(1, 60));
    SendMessage(g_hFrameRateTrackbar, TBM_SETPOS, TRUE, 30);
    SendMessage(g_hFrameRateTrackbar, TBM_SETTICFREQ, 5, 0);
    
    // 分辨率显示
    g_hResolutionLabel = CreateWindow(L"STATIC", L"当前分辨率：", WS_CHILD | SS_LEFT,
        tabLeft, tabTop + ScaleDPI(80), ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    wchar_t resolutionText[64];
    swprintf_s(resolutionText, L"%d × %d", screenWidth, screenHeight);
    g_hResolutionValueLabel = CreateWindow(L"STATIC", resolutionText, WS_CHILD | SS_LEFT,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(80), tabWidth - ScaleDPI(110), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    
    // 带宽预估显示
    g_hBandwidthLabel = CreateWindow(L"STATIC", L"预计带宽：", WS_CHILD | SS_LEFT,
        tabLeft, tabTop + ScaleDPI(110), ScaleDPI(100), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    
    // 计算初始带宽
    double bandwidth = CalculateEstimatedBandwidth(screenWidth, screenHeight, 30, 95);
    wchar_t bandwidthText[64];
    swprintf_s(bandwidthText, L"%.1f Mbps", bandwidth);
    g_hBandwidthValueLabel = CreateWindow(L"STATIC", bandwidthText, WS_CHILD | SS_LEFT,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(110), tabWidth - ScaleDPI(110), ScaleDPI(20), hWnd, NULL, NULL, NULL);
    
    // 应用设置按钮
    g_hApplySettingsButton = CreateWindow(L"BUTTON", L"应用设置", WS_CHILD | BS_PUSHBUTTON,
        tabLeft + ScaleDPI(110), tabTop + ScaleDPI(140), ScaleDPI(120), ScaleDPI(32), hWnd, (HMENU)5, NULL, NULL);
    
    // 设置设置控件字体
    SendMessage(g_hQualityLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hQualityTrackbar, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hQualityValueLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hFrameRateLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hFrameRateTrackbar, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hFrameRateValueLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hResolutionLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hResolutionValueLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hBandwidthLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hBandwidthValueLabel, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    SendMessage(g_hApplySettingsButton, WM_SETFONT, (WPARAM)g_hDefaultFont, TRUE);
    
    // 默认隐藏设置页面
    ShowWindow(g_hQualityLabel, SW_HIDE);
    ShowWindow(g_hQualityTrackbar, SW_HIDE);
    ShowWindow(g_hQualityValueLabel, SW_HIDE);
    ShowWindow(g_hFrameRateLabel, SW_HIDE);
    ShowWindow(g_hFrameRateTrackbar, SW_HIDE);
    ShowWindow(g_hFrameRateValueLabel, SW_HIDE);
    ShowWindow(g_hResolutionLabel, SW_HIDE);
    ShowWindow(g_hResolutionValueLabel, SW_HIDE);
    ShowWindow(g_hBandwidthLabel, SW_HIDE);
    ShowWindow(g_hBandwidthValueLabel, SW_HIDE);
    ShowWindow(g_hApplySettingsButton, SW_HIDE);
}

// 更新带宽预估
void UpdateBandwidthEstimation() {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    int quality = SendMessage(g_hQualityTrackbar, TBM_GETPOS, 0, 0);
    int frameRate = SendMessage(g_hFrameRateTrackbar, TBM_GETPOS, 0, 0);
    
    double bandwidth = CalculateEstimatedBandwidth(screenWidth, screenHeight, frameRate, quality);
    wchar_t bandwidthText[64];
    swprintf_s(bandwidthText, L"%.1f Mbps", bandwidth);
    SetWindowText(g_hBandwidthValueLabel, bandwidthText);
}

// 应用设置
void ApplySettings() {
    int quality = SendMessage(g_hQualityTrackbar, TBM_GETPOS, 0, 0);
    int frameRate = SendMessage(g_hFrameRateTrackbar, TBM_GETPOS, 0, 0);
    
    g_sender.SetQuality(quality);
    g_sender.SetFrameRate(frameRate);
    
    MessageBox(g_hWnd, L"设置已应用", L"成功", MB_ICONINFORMATION);
}

// 填充网络适配器到组合框
void FillNetworkAdapters() {
    std::vector<NetworkAdapter> adapters = GetNetworkAdapters();
    
    // 发送适配器组合框（仅实际接口）
    for (const auto& adapter : adapters) {
        SendMessage(g_hComboBoxSendAdapters, CB_ADDSTRING, 0, (LPARAM)adapter.name.c_str());
    }
    if (!adapters.empty()) {
        SendMessage(g_hComboBoxSendAdapters, CB_SETCURSEL, 0, 0);
    }
    
    // 接收适配器组合框（添加"任意接口"选项）
    SendMessage(g_hComboBoxRecvAdapters, CB_ADDSTRING, 0, (LPARAM)L"任意接口 (0.0.0.0)");
    for (const auto& adapter : adapters) {
        SendMessage(g_hComboBoxRecvAdapters, CB_ADDSTRING, 0, (LPARAM)adapter.name.c_str());
    }
    SendMessage(g_hComboBoxRecvAdapters, CB_SETCURSEL, 0, 0);
}

// 开始发送
void StartSending() {
    // 获取选中的适配器
    int selectedIndex = SendMessage(g_hComboBoxSendAdapters, CB_GETCURSEL, 0, 0);
    if (selectedIndex == CB_ERR) {
        MessageBox(g_hWnd, L"请选择发送网络适配器", L"错误", MB_ICONERROR);
        return;
    }
    
    std::vector<NetworkAdapter> adapters = GetNetworkAdapters();
    if (selectedIndex >= (int)adapters.size()) {
        MessageBox(g_hWnd, L"适配器选择无效", L"错误", MB_ICONERROR);
        return;
    }
    
    std::string localInterface = adapters[selectedIndex].ipAddress;
    
    // 获取组播地址
    char multicastAddr[256];
    GetWindowTextA(g_hEditSendMulticastAddr, multicastAddr, 256);
    std::string sMulticastAddr = multicastAddr;
    
    // 获取端口
    char port[256];
    GetWindowTextA(g_hEditSendPort, port, 256);
    std::string sPort = port;
    int iPort = std::stoi(sPort);
    
    // 开始发送
    if (g_sender.Start(sMulticastAddr, iPort, localInterface)) {
        EnableWindow(g_hButtonSendStart, FALSE);
        EnableWindow(g_hButtonSendStop, TRUE);
        EnableWindow(g_hComboBoxSendAdapters, FALSE);
        EnableWindow(g_hEditSendMulticastAddr, FALSE);
        EnableWindow(g_hEditSendPort, FALSE);
    } else {
        MessageBox(g_hWnd, L"启动发送失败", L"错误", MB_ICONERROR);
    }
}

// 停止发送
void StopSending() {
    g_sender.Stop();
    
    EnableWindow(g_hButtonSendStart, TRUE);
    EnableWindow(g_hButtonSendStop, FALSE);
    EnableWindow(g_hComboBoxSendAdapters, TRUE);
    EnableWindow(g_hEditSendMulticastAddr, TRUE);
    EnableWindow(g_hEditSendPort, TRUE);
}

// 开始接收
void StartReceiving() {
    // 获取选中的适配器
    int selectedIndex = SendMessage(g_hComboBoxRecvAdapters, CB_GETCURSEL, 0, 0);
    if (selectedIndex == CB_ERR) {
        MessageBox(g_hWnd, L"请选择接收网络适配器", L"错误", MB_ICONERROR);
        return;
    }
    
    std::string localInterface;
    std::vector<NetworkAdapter> adapters = GetNetworkAdapters();
    
    if (selectedIndex == 0) {
        localInterface = "0.0.0.0";
    } else if (selectedIndex - 1 < (int)adapters.size()) {
        localInterface = adapters[selectedIndex - 1].ipAddress;
    } else {
        MessageBox(g_hWnd, L"适配器选择无效", L"错误", MB_ICONERROR);
        return;
    }
    
    // 获取组播地址
    char multicastAddr[256];
    GetWindowTextA(g_hEditRecvMulticastAddr, multicastAddr, 256);
    std::string sMulticastAddr = multicastAddr;
    
    // 获取端口
    char port[256];
    GetWindowTextA(g_hEditRecvPort, port, 256);
    std::string sPort = port;
    int iPort = std::stoi(sPort);
    
    // 开始接收
    if (g_receiver.Start(sMulticastAddr, iPort, localInterface, g_hWnd)) {
        EnableWindow(g_hButtonRecvStart, FALSE);
        EnableWindow(g_hButtonRecvStop, TRUE);
        EnableWindow(g_hComboBoxRecvAdapters, FALSE);
        EnableWindow(g_hEditRecvMulticastAddr, FALSE);
        EnableWindow(g_hEditRecvPort, FALSE);
    } else {
        MessageBox(g_hWnd, L"启动接收失败", L"错误", MB_ICONERROR);
    }
}

// 停止接收
void StopReceiving() {
    g_receiver.Stop();
    
    EnableWindow(g_hButtonRecvStart, TRUE);
    EnableWindow(g_hButtonRecvStop, FALSE);
    EnableWindow(g_hComboBoxRecvAdapters, TRUE);
    EnableWindow(g_hEditRecvMulticastAddr, TRUE);
    EnableWindow(g_hEditRecvPort, TRUE);
}

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_hWnd = hWnd;
        
        // 获取DPI缩放
        g_dpiScale = GetDPIScale(hWnd);
        
        // 创建字体
        CreateDefaultFont();
        
        // 创建背景刷子
        g_hBrushBackground = GetSysColorBrush(COLOR_WINDOW);
        
        // 设置窗口图标（支持高DPI）
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
        
        // 加载大图标（用于Alt+Tab和任务栏）
        HICON hIconLarge = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAINICON),
                                           IMAGE_ICON, GetSystemMetrics(SM_CXICON), 
                                           GetSystemMetrics(SM_CYICON), LR_SHARED);
        if (hIconLarge) {
            SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
        }
        
        // 加载小图标（用于标题栏和任务栏小图标）
        HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAINICON),
                                           IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), 
                                           GetSystemMetrics(SM_CYSMICON), LR_SHARED);
        if (hIconSmall) {
            SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        }
        
        // 重新设置窗口大小
        SetWindowPos(hWnd, nullptr, CW_USEDEFAULT, CW_USEDEFAULT, ScaleDPI(500), ScaleDPI(280),
                     SWP_NOZORDER | SWP_NOMOVE);

        // 创建控件
        CreateControls(hWnd);
        FillNetworkAdapters();
        break;
    }
    case WM_CTLCOLORSTATIC: {
        // 处理STATIC控件的颜色，去除灰色背景
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        return (INT_PTR)g_hBrushBackground;
    }
    case WM_NOTIFY: {
        LPNMHDR pnmhdr = (LPNMHDR)lParam;
        if (pnmhdr->hwndFrom == g_hTabControl && pnmhdr->code == TCN_SELCHANGE) {
            int iPage = TabCtrl_GetCurSel(g_hTabControl);
            
            // 发送页面的所有控件
            HWND sendControls[] = {
                g_hSendAdapterLabel,
                g_hComboBoxSendAdapters,
                g_hSendAddrLabel,
                g_hEditSendMulticastAddr,
                g_hSendPortLabel,
                g_hEditSendPort,
                g_hButtonSendStart,
                g_hButtonSendStop
            };
            
            // 接收页面的所有控件
            HWND recvControls[] = {
                g_hRecvAdapterLabel,
                g_hComboBoxRecvAdapters,
                g_hRecvAddrLabel,
                g_hEditRecvMulticastAddr,
                g_hRecvPortLabel,
                g_hEditRecvPort,
                g_hButtonRecvStart,
                g_hButtonRecvStop
            };
            
            // 设置页面的所有控件
            HWND settingsControls[] = {
                g_hQualityLabel,
                g_hQualityTrackbar,
                g_hQualityValueLabel,
                g_hFrameRateLabel,
                g_hFrameRateTrackbar,
                g_hFrameRateValueLabel,
                g_hResolutionLabel,
                g_hResolutionValueLabel,
                g_hBandwidthLabel,
                g_hBandwidthValueLabel,
                g_hApplySettingsButton
            };
            
            if (iPage == 0) {
                // 显示发送页面
                for (HWND hwnd : sendControls) {
                    if (hwnd) ShowWindow(hwnd, SW_SHOW);
                }
                // 隐藏接收页面
                for (HWND hwnd : recvControls) {
                    if (hwnd) ShowWindow(hwnd, SW_HIDE);
                }
                // 隐藏设置页面
                for (HWND hwnd : settingsControls) {
                    if (hwnd) ShowWindow(hwnd, SW_HIDE);
                }
            } else if (iPage == 1) {
                // 隐藏发送页面
                for (HWND hwnd : sendControls) {
                    if (hwnd) ShowWindow(hwnd, SW_HIDE);
                }
                // 显示接收页面
                for (HWND hwnd : recvControls) {
                    if (hwnd) ShowWindow(hwnd, SW_SHOW);
                }
                // 隐藏设置页面
                for (HWND hwnd : settingsControls) {
                    if (hwnd) ShowWindow(hwnd, SW_HIDE);
                }
            } else if (iPage == 2) {
                // 隐藏发送页面
                for (HWND hwnd : sendControls) {
                    if (hwnd) ShowWindow(hwnd, SW_HIDE);
                }
                // 隐藏接收页面
                for (HWND hwnd : recvControls) {
                    if (hwnd) ShowWindow(hwnd, SW_HIDE);
                }
                // 显示设置页面
                for (HWND hwnd : settingsControls) {
                    if (hwnd) ShowWindow(hwnd, SW_SHOW);
                }
            }
        }
        break;
    }
    case WM_HSCROLL: {
        // 处理滑块滚动事件
        HWND hTrackbar = (HWND)lParam;
        
        if (hTrackbar == g_hQualityTrackbar) {
            // 更新质量值显示
            int quality = SendMessage(g_hQualityTrackbar, TBM_GETPOS, 0, 0);
            wchar_t qualityText[16];
            swprintf_s(qualityText, L"%d", quality);
            SetWindowText(g_hQualityValueLabel, qualityText);
            
            // 更新带宽预估
            UpdateBandwidthEstimation();
        } else if (hTrackbar == g_hFrameRateTrackbar) {
            // 更新帧率值显示
            int frameRate = SendMessage(g_hFrameRateTrackbar, TBM_GETPOS, 0, 0);
            wchar_t frameRateText[16];
            swprintf_s(frameRateText, L"%d", frameRate);
            SetWindowText(g_hFrameRateValueLabel, frameRateText);
            
            // 更新带宽预估
            UpdateBandwidthEstimation();
        }
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        
        switch (wmId) {
        case 1: // 开始发送按钮
            StartSending();
            break;
        case 2: // 停止发送按钮
            StopSending();
            break;
        case 3: // 开始接收按钮
            StartReceiving();
            break;
        case 4: // 停止接收按钮
            StopReceiving();
            break;
        case 5: // 应用设置按钮
            ApplySettings();
            break;
        }
        break;
    }
    case WM_CLOSE: {
        // 停止发送和接收
        g_sender.Stop();
        g_receiver.Stop();
        
        DestroyWindow(hWnd);
        break;
    }
    case WM_DESTROY: {
        // 停止接收并关闭接收窗口
        if (g_receiver.IsReceiving()) {
            g_receiver.Stop();
        }
        HWND hRecvWindow = g_receiver.GetDisplayWindow();
        if (hRecvWindow && IsWindow(hRecvWindow)) {
            DestroyWindow(hRecvWindow);
        }
        
        // 清理字体
        if (g_hDefaultFont) {
            DeleteObject(g_hDefaultFont);
            g_hDefaultFont = NULL;
        }
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 设置DPI感知
    SetProcessDPIAware();
    
    // 初始化GDI+
    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    // 初始化通用控件
    SetupCommonControls();
    
    // 创建主窗口
    HWND hWnd = CreateMainWindow(hInstance);
    if (!hWnd) {
        return 0;
    }
    
    // 显示窗口
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 清理GDI+
    GdiplusShutdown(gdiplusToken);
    
    return (int)msg.wParam;
}
