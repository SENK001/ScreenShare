#include "common.h"
#include "sender.h"
#include "receiver.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

// 全局变量
ScreenSender g_sender;
ScreenReceiver g_receiver;

// 控件句柄
HWND g_hWnd = NULL;
HWND g_hComboBoxSendAdapters = NULL;
HWND g_hEditSendMulticastAddr = NULL;
HWND g_hEditSendPort = NULL;
HWND g_hButtonSendStart = NULL;
HWND g_hButtonSendStop = NULL;

HWND g_hComboBoxRecvAdapters = NULL;
HWND g_hEditRecvMulticastAddr = NULL;
HWND g_hEditRecvPort = NULL;
HWND g_hButtonRecvStart = NULL;
HWND g_hButtonRecvStop = NULL;

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// 初始化通用控件
void SetupCommonControls() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
}

// 创建主窗口
HWND CreateMainWindow(HINSTANCE hInstance) {
    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ScreenShareAppClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"窗口注册失败", L"错误", MB_ICONERROR);
        return NULL;
    }

    // 创建窗口
    HWND hWnd = CreateWindow(
        L"ScreenShareAppClass",
        L"屏幕共享工具（发送与接收）",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL, NULL, hInstance, NULL
    );

    return hWnd;
}

// 创建控件
void CreateControls(HWND hWnd) {
    // 发送控制区域标签
    CreateWindow(L"STATIC", L"发送控制", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        10, 10, 480, 20, hWnd, NULL, NULL, NULL);

    // 发送控制 - 网络适配器
    CreateWindow(L"STATIC", L"发送适配器：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 40, 100, 20, hWnd, NULL, NULL, NULL);
    g_hComboBoxSendAdapters = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        120, 40, 250, 200, hWnd, NULL, NULL, NULL);

    // 发送控制 - 组播地址
    CreateWindow(L"STATIC", L"发送地址：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 70, 100, 20, hWnd, NULL, NULL, NULL);
    g_hEditSendMulticastAddr = CreateWindow(L"EDIT", L"239.0.0.1", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                            120, 70, 250, 20, hWnd, NULL, NULL, NULL);

    // 发送控制 - 端口
    CreateWindow(L"STATIC", L"发送端口：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 100, 100, 20, hWnd, NULL, NULL, NULL);
    g_hEditSendPort = CreateWindow(L"EDIT", L"9277", WS_CHILD | WS_VISIBLE | WS_BORDER,
        120, 100, 250, 20, hWnd, NULL, NULL, NULL);

    // 发送控制 - 按钮
    g_hButtonSendStart = CreateWindow(L"BUTTON", L"开始发送", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        120, 130, 100, 30, hWnd, (HMENU)1, NULL, NULL);
    g_hButtonSendStop = CreateWindow(L"BUTTON", L"停止发送", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        270, 130, 100, 30, hWnd, (HMENU)2, NULL, NULL);
    EnableWindow(g_hButtonSendStop, FALSE);

    // 分隔线
    CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        10, 180, 480, 1, hWnd, NULL, NULL, NULL);

    // 接收控制区域标签
    CreateWindow(L"STATIC", L"接收控制", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        10, 190, 480, 20, hWnd, NULL, NULL, NULL);

    // 接收控制 - 网络适配器
    CreateWindow(L"STATIC", L"接收适配器：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 220, 100, 20, hWnd, NULL, NULL, NULL);
    g_hComboBoxRecvAdapters = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        120, 220, 250, 200, hWnd, NULL, NULL, NULL);

    // 接收控制 - 组播地址
    CreateWindow(L"STATIC", L"接收地址：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 250, 100, 20, hWnd, NULL, NULL, NULL);
    g_hEditRecvMulticastAddr = CreateWindow(L"EDIT", L"239.0.0.1", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                            120, 250, 250, 20, hWnd, NULL, NULL, NULL);

    // 接收控制 - 端口
    CreateWindow(L"STATIC", L"接收端口：", WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 280, 100, 20, hWnd, NULL, NULL, NULL);
    g_hEditRecvPort = CreateWindow(L"EDIT", L"9277", WS_CHILD | WS_VISIBLE | WS_BORDER,
        120, 280, 250, 20, hWnd, NULL, NULL, NULL);

    // 接收控制 - 按钮
    g_hButtonRecvStart = CreateWindow(L"BUTTON", L"开始接收", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        120, 310, 100, 30, hWnd, (HMENU)3, NULL, NULL);
    g_hButtonRecvStop = CreateWindow(L"BUTTON", L"停止接收", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        270, 310, 100, 30, hWnd, (HMENU)4, NULL, NULL);
    EnableWindow(g_hButtonRecvStop, FALSE);
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
        CreateControls(hWnd);
        FillNetworkAdapters();
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
