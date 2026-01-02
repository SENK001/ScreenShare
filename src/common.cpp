#include "common.h"

// 将宽字符字符串转换为多字节字符串
std::string WCharToMByte(LPCWSTR lpcwszStr) {
    std::string str;
    int len = WideCharToMultiByte(CP_ACP, 0, lpcwszStr, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";

    char* pszStr = new char[len];
    if (!pszStr) return "";

    WideCharToMultiByte(CP_ACP, 0, lpcwszStr, -1, pszStr, len, NULL, NULL);
    pszStr[len - 1] = 0;
    str = pszStr;
    delete[] pszStr;

    return str;
}

// 获取所有网络适配器信息
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
        int index = 1;

        while (pCurrAddresses) {
            NetworkAdapter adapter;
            adapter.index = index;

            // 转换宽字符字符串到多字节字符串
            adapter.name = pCurrAddresses->AdapterName;
            adapter.description = WCharToMByte(pCurrAddresses->Description);

            // 获取IPv4地址
            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
            while (pUnicast) {
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                    char ipStr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, INET_ADDRSTRLEN);
                    adapter.ipAddress = ipStr;
                    break;
                }
                pUnicast = pUnicast->Next;
            }

            if (!adapter.ipAddress.empty()) {
                adapters.push_back(adapter);
                index++;
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
