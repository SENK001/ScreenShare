# UI改进说明

## 完成的优化

### 1. 高DPI支持
- **添加应用清单文件** (`ScreenShare.manifest`)
  - 启用 `PerMonitorV2` DPI感知模式
  - 支持Windows 7到Windows 11
  - 启用通用控件v6以获得现代UI外观
  
- **DPI缩放实现**
  - 添加 `GetDPIScale()` 函数获取当前DPI缩放比例
  - 添加 `ScaleDPI()` 函数按DPI比例缩放所有控件尺寸
  - 在 `WinMain` 中调用 `SetProcessDPIAware()` 启用DPI感知

### 2. 标签页界面
- **使用Tab Control重构UI**
  - 将发送和接收功能分离到两个独立的标签页
  - 标签页1: "发送屏幕" - 包含所有发送相关控件
  - 标签页2: "接收屏幕" - 包含所有接收相关控件
  
- **标签页切换逻辑**
  - 添加 `WM_NOTIFY` 消息处理，响应 `TCN_SELCHANGE` 事件
  - 自动显示/隐藏相应标签页的控件
  - 保持各标签页状态独立

### 3. 字体优化
- **使用系统默认UI字体**
  - 通过 `SystemParametersInfo` 获取系统消息框字体
  - 根据DPI比例调整字体大小
  - 为所有控件应用统一字体，确保一致性
  
- **字体资源管理**
  - 在 `CreateDefaultFont()` 中创建字体
  - 在 `WM_DESTROY` 中释放字体资源

### 4. 布局优化
- **调整窗口尺寸**
  - 窗口大小从固定的500x400调整为550x450（基础96 DPI）
  - 所有尺寸通过 `ScaleDPI()` 动态缩放
  
- **改进控件布局**
  - 统一控件间距和对齐
  - 优化标签和输入框的位置关系
  - 按钮尺寸更大，易于点击

### 5. 代码结构改进
- **全局变量组织**
  - 为所有标签控件添加句柄变量
  - 分组管理发送页和接收页的控件句柄
  
- **初始化流程**
  - 在 `WM_CREATE` 中正确初始化DPI缩放
  - 创建字体后再创建控件
  - 控件创建后立即设置字体

## 技术要点

### DPI感知的三个层次
1. **应用清单**: 声明应用支持高DPI
2. **API调用**: `SetProcessDPIAware()` 告知系统
3. **缩放实现**: 所有尺寸都通过 `ScaleDPI()` 计算

### 标签页实现
```cpp
// 创建标签控件
g_hTabControl = CreateWindow(WC_TABCONTROL, ...);

// 添加标签页
TCITEM tie;
tie.mask = TCIF_TEXT;
tie.pszText = (LPWSTR)L"发送屏幕";
TabCtrl_InsertItem(g_hTabControl, 0, &tie);

// 获取内容区域
RECT rcTab;
GetClientRect(g_hTabControl, &rcTab);
TabCtrl_AdjustRect(g_hTabControl, FALSE, &rcTab);
```

### 控件显示/隐藏切换
```cpp
case WM_NOTIFY:
    if (pnmhdr->code == TCN_SELCHANGE) {
        int iPage = TabCtrl_GetCurSel(g_hTabControl);
        // 根据iPage显示/隐藏相应控件
    }
```

## 测试建议

1. **不同DPI设置测试**
   - 100% (96 DPI)
   - 125% (120 DPI)
   - 150% (144 DPI)
   - 200% (192 DPI)

2. **标签页功能测试**
   - 切换标签页是否正常
   - 各标签页的控件状态是否保持
   - 发送和接收功能是否独立工作

3. **字体显示测试**
   - 所有文本是否清晰可读
   - 不同DPI下字体大小是否合适
   - 控件内文字是否完整显示

## 兼容性

- **Windows版本**: Windows 7及以上
- **DPI模式**: 支持PerMonitorV2 (Windows 10 1703+)
- **降级支持**: 在旧版Windows上自动降级到可用的DPI模式

## 构建说明

项目使用CMake构建，manifest文件会自动嵌入到可执行文件中：

```bash
cd build
cmake ..
cmake --build . --config Release
```

编译后的程序位于 `build/bin/Release/ScreenShare.exe`
