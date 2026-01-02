#ifndef COMMON_H
#define COMMON_H

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <chrono>
#include <objidl.h>
#include <gdiplus.h>
#include <queue>
#include <codecvt>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using namespace std::chrono;

// 定义自定义消息
#define WM_UPDATE_IMAGE (WM_USER + 1)
#define WM_TOGGLE_TOP_MOST (WM_USER + 2)

// 协议魔数（网络字节序写入，接收端校验）
constexpr uint16_t FRAGMENT_MAGIC = 0x5353; // "SS" ScreenShare

// 分片信息结构（保持紧凑布局，并保证至少2字节对齐）
#pragma pack(push, 1)
struct alignas(2) FragmentHeader {
    uint16_t magic;          // 协议标识
    uint32_t frameId;        // 帧ID（递增）
    uint32_t frameSize;      // 整个帧的大小
    uint16_t totalFragments; // 总片段数
    uint16_t fragmentIndex;  // 当前片段索引
    uint16_t fragmentSize;   // 当前片段大小
};
#pragma pack(pop)

static_assert(sizeof(FragmentHeader) == 16, "FragmentHeader layout mismatch");

const size_t MAX_FRAGMENT_SIZE = 1400; // 每个分片的最大大小

// 网络适配器信息结构
struct NetworkAdapter {
    std::wstring name;
    std::string ipAddress;
};

// 将宽字符字符串转换为多字节字符串
std::string WCharToMByte(LPCWSTR lpcwszStr);

// 获取所有网络适配器信息
std::vector<NetworkAdapter> GetNetworkAdapters();

// 获取JPEG编码器的CLSID（用于发送端）
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

#endif // COMMON_H
