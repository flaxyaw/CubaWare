//uhhhhhh

#include <obfuscation/syscall.hpp>
#include <obfuscation/api_hash.hpp>
#include <windows.h>

//needs C linkage so we can reference it by name

extern "C" {
    DWORD _nt_protect_id = 0xFFFFFFFF;
}

namespace syscall_utils {

    DWORD get_id(BYTE* fn) {
        if (!fn) return 0xFFFFFFFF;

        //standard stub: 4C 8B D1, B8 [id]
        if (fn[0] == 0x4C && fn[1] == 0x8B && fn[2] == 0xD1 && fn[3] == 0xB8)
            return *(DWORD*)(fn + 4);

        //halos gate first bytes are hooked, scan neighboring stubs
        //stubs are same size + sequential so id = neighbor_id -/+ n
        //sounding smart = smart
        for (int i = 1; i < 32; i++) {
            if (fn[i * 32] == 0x4C && fn[i * 32 + 1] == 0x8B &&
                fn[i * 32 + 2] == 0xD1 && fn[i * 32 + 3] == 0xB8)
                return *(DWORD*)(fn + i * 32 + 4) - i;
            if (fn[-i * 32] == 0x4C && fn[-i * 32 + 1] == 0x8B &&
                fn[-i * 32 + 2] == 0xD1 && fn[-i * 32 + 3] == 0xB8)
                return *(DWORD*)(fn - i * 32 + 4) + i;
        }
        return 0xFFFFFFFF;
    }

    void init(HMODULE ntdll) {
        //resolve via hash vs IAT
        constexpr DWORD h_nvpm = api_hash::hash("NtProtectVirtualMemory");
        _nt_protect_id = get_id((BYTE*)api_hash::get_proc(ntdll, h_nvpm));
    }

}
