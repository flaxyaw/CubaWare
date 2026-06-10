#pragma once
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;
namespace utils {
    std::vector<fs::path> get_chromium_browsers();
    std::vector<fs::path> get_gecko_browsers();
}


