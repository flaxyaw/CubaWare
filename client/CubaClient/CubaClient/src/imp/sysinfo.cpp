#include <windows.h>
#include <sysinfo.hpp>
#include <filesystem>
#include <crypto_utils/skCrypter.hpp>


namespace features {
    std::string get_username() {
        char buf[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD sz = sizeof(buf);
        if (GetComputerNameA(buf, &sz))
            return buf;
        return "Unknown";
    }

    std::string get_home() {
        static std::string cached = [] {
            const char* p = std::getenv("USERPROFILE");
            return p ? std::string(p) : std::string();
        }();
        return cached;
    }

    std::filesystem::path get_temp() {
        static std::filesystem::path cached = std::filesystem::temp_directory_path();
        return cached;
    }

    std::wstring get_version() {
        wchar_t product_name[256] = {};
        wchar_t current_build[32] = {};
        DWORD size_product = sizeof(product_name);
        DWORD size_build = sizeof(current_build);

        if (RegGetValueW(HKEY_LOCAL_MACHINE,
                skCrypt(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
                skCrypt(L"ProductName"),
                RRF_RT_REG_SZ, nullptr, product_name, &size_product) != ERROR_SUCCESS)
            return L"";

        if (RegGetValueW(HKEY_LOCAL_MACHINE,
                skCrypt(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
                skCrypt(L"CurrentBuild"),
                RRF_RT_REG_SZ, nullptr, current_build, &size_build) != ERROR_SUCCESS)
            return std::wstring(product_name);

        return std::wstring(product_name) + L" " + std::wstring(current_build);
    }


    //used to adjust functionality depending on permission. (duh)
    bool is_admin() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            return false;
        }

        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(elevation);
        BOOL is_admin = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
        CloseHandle(token);

        if (!is_admin) {
            return false;
        }
        return true;
    }
}
