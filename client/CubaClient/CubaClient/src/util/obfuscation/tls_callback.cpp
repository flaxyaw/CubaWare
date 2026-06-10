//we love pasting
#include <windows.h>
#include <intrin.h>

//force TLS directory to exist linker wont emit a .tls entry without a thread local variable
__thread volatile int _tls_anchor = 0;

static VOID NTAPI cuba_tls_cb(PVOID, DWORD reason, PVOID) {
    if (reason != DLL_PROCESS_ATTACH) return;

    //IsDebuggerPresent is safe this early, before CRT init
    if (IsDebuggerPresent()) ExitProcess(0);

    //NtGlobalFlag 0x70 = heap debug flags set by ntdll when a debugger attaches
    BYTE* peb = (BYTE*)__readgsqword(0x60);
    if (*(DWORD*)(peb + 0xBC) & 0x70) ExitProcess(0);

    //any nonzero DR register means a HWBP is armed
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(GetCurrentThread(), &ctx))
        if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) ExitProcess(0);

    //hypervisor bit set - check vendor before bailing, real hyper-v on bare metal is fine
    int cpu[4] = {};
    __cpuid(cpu, 1);
    if ((cpu[2] & (1 << 31)) != 0) {
        __cpuid(cpu, 0x40000000);
        //Microsoft Hv — compared as raw dwords, no string literal in .rdata
        if (cpu[1] != 0x7263694d || cpu[2] != 0x666f736f || cpu[3] != 0x76482074) ExitProcess(0);
    }
}

//register in .CRT$XLB - the CRT loader runs callbacks in this section before WinMain
__attribute__((section(".CRT$XLB"), used))
static PIMAGE_TLS_CALLBACK _cuba_tls_ptr = cuba_tls_cb;
