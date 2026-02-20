#include "common.h"

// 获取所有有效网络适配器信息
std::vector<NetworkAdapter> GetNetworkAdapters() {
    std::vector<NetworkAdapter> adapters;

    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
    ULONG outBufLen = 0;
    DWORD dwRetVal = 0;

    // 第一次调用获取所需缓冲区大小
    GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &outBufLen);

    pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
    if (!pAddresses) {
        return adapters;
    }

    // 获取适配器信息
    dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &outBufLen);

    if (dwRetVal == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;

        while (pCurrAddresses) {
            NetworkAdapter adapter;

            // 判断适配器有效
            if (pCurrAddresses->OperStatus == IfOperStatusUp) {
                // 获取IPv4地址
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                while (pUnicast)
                {
                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
                    {
                        sockaddr_in *sa_in = (sockaddr_in *)pUnicast->Address.lpSockaddr;
                        char ipStr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, INET_ADDRSTRLEN);
                        adapter.ipAddress = ipStr;
                        break;
                    }
                    pUnicast = pUnicast->Next;
                }

                if (!adapter.ipAddress.empty())
                {
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                    adapter.name = pCurrAddresses->FriendlyName;
                    adapter.name += L" (" + converter.from_bytes(adapter.ipAddress) + L")";
                    adapters.push_back(adapter);
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    }

    if (pAddresses) free(pAddresses);
    return adapters;
}

// 获取JPEG编码器的CLSID（用于发送端）
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;

    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)malloc(size);
    if (!pImageCodecInfo) return -1;

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[i].Clsid;
            free(pImageCodecInfo);
            return i;
        }
    }

    free(pImageCodecInfo);
    return -1;
}

// 获取当前屏幕分辨率
void GetCurrentScreenResolution(int& width, int& height) {
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
}

// 计算预估带宽（Mbps）
double CalculateEstimatedBandwidth(int width, int height, int frameRate, int quality) {
    // 基础带宽：宽度 × 高度 × 3字节（RGB）× 帧率
    double baseBandwidth = width * height * 3.0 * frameRate; // 字节/秒
    
    // 压缩系数：基于JPEG质量的经验公式
    // 质量越高（0-100），压缩率越低，文件越大
    double compressionFactor;
    if (quality >= 90) {
        compressionFactor = 0.15; // 高质量，压缩率低
    } else if (quality >= 70) {
        compressionFactor = 0.10; // 中等质量
    } else if (quality >= 50) {
        compressionFactor = 0.07; // 中等偏低质量
    } else if (quality >= 30) {
        compressionFactor = 0.05; // 低质量
    } else {
        compressionFactor = 0.03; // 极低质量
    }
    
    // 应用压缩系数
    double compressedBandwidth = baseBandwidth * compressionFactor;
    
    // 转换为Mbps（兆比特每秒）
    double bandwidthMbps = (compressedBandwidth * 8.0) / (1024.0 * 1024.0);
    
    return bandwidthMbps;
}
