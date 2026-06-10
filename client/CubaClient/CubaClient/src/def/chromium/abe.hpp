#pragma once
#include <windows.h>
#include <vector>
#include <string>

namespace abe {
    //spawn browser, intercept the key from R15/R14 when the elevation service returns it
    //returns raw 32 byte AES key or {} on failure
    std::vector<BYTE> get_abe_key(const std::string& browser_root);
}
