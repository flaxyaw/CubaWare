#pragma once
#include <string>
#include <cstdint>

struct GeckoCookie {
    std::string hostname;
    std::string name;
    std::string value;
    std::string path;
    int64_t     expires;
    bool        secure;
    bool        httpOnly;
};
