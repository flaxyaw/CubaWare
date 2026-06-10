#pragma once
#include <windows.h>

namespace unhook {
    void   unhook_ntdll();
    LPVOID get_clean_ntdll(); //fresh disk-mapped copy kept alive for clean export resolution
}
