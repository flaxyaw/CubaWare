#pragma once
//scary code! again, big shoutout to MossadAgent and PTCruiser (and AI)
//https://www.unknowncheats.me/forum/general-programming-and-reversing/592630-ntdll-documentation.html
//http://undocumented.ntinternals.net/

#include <windows.h>
#include <intrin.h>

//mingw winternl.h only exposes InMemoryOrder variants so define the full structs ourselves
//offsets from x64 ntdll on win10/11, havent changed since vista

namespace api_hash {

    struct _uni_str { USHORT Length; USHORT MaximumLength; WCHAR* Buffer; };

    struct _ldr_entry {
        LIST_ENTRY InLoadOrderLinks;           //0x00
        LIST_ENTRY InMemoryOrderLinks;         //0x10
        LIST_ENTRY InInitializationOrderLinks; //0x20
        PVOID      DllBase;                    //0x30
        PVOID      EntryPoint;                 //0x38
        ULONG      SizeOfImage;               //0x40
        ULONG      _pad;                       //0x44
        _uni_str   FullDllName;               //0x48
        _uni_str   BaseDllName;               //0x58
    };

    struct _ldr_data {
        ULONG      Length;                    //0x00
        BOOL       Initialized;              //0x04
        PVOID      SsHandle;                 //0x08
        LIST_ENTRY InLoadOrderModuleList;    //0x10
    };

    //fnv-1a, case insensitive
    constexpr DWORD hash(const char* s, DWORD h = 2166136261u) {
        return *s ? hash(s + 1,
            (h ^ ((*s >= 'A' && *s <= 'Z') ? (char)(*s | 0x20) : *s)) * 16777619u) : h;
    }
    constexpr DWORD hash(const wchar_t* s, DWORD h = 2166136261u) {
        return *s ? hash(s + 1,
            (h ^ ((*s >= L'A' && *s <= L'Z') ? (char)(*s | 0x20) : (char)*s)) * 16777619u) : h;
    }

    //peb.ldr is at 0x18 on x64
    inline HMODULE get_module(DWORD target) {
        auto ldr  = *(_ldr_data**)(__readgsqword(0x60) + 0x18);
        auto list = &ldr->InLoadOrderModuleList;

        for (auto e = list->Flink; e != list; e = e->Flink) {
            auto m = CONTAINING_RECORD(e, _ldr_entry, InLoadOrderLinks);
            if (m->BaseDllName.Buffer && hash(m->BaseDllName.Buffer) == target)
                return (HMODULE)m->DllBase;
        }
        return nullptr;
    }

    inline FARPROC get_proc(HMODULE mod, DWORD target) {
        if (!mod) return nullptr;
        auto dos  = (PIMAGE_DOS_HEADER)mod;
        auto nt   = (PIMAGE_NT_HEADERS)((BYTE*)mod + dos->e_lfanew);
        auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress) return nullptr;

        auto exp   = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)mod + dir.VirtualAddress);
        auto names = (DWORD*)((BYTE*)mod + exp->AddressOfNames);
        auto ords  = (WORD*) ((BYTE*)mod + exp->AddressOfNameOrdinals);
        auto funcs = (DWORD*)((BYTE*)mod + exp->AddressOfFunctions);

        for (DWORD i = 0; i < exp->NumberOfNames; i++) {
            if (hash((const char*)((BYTE*)mod + names[i])) == target)
                return (FARPROC)((BYTE*)mod + funcs[ords[i]]);
        }
        return nullptr;
    }

} //namespace api_hash

#define RESOLVE(mod_hash, fn_hash) \
    api_hash::get_proc(api_hash::get_module(mod_hash), fn_hash)
