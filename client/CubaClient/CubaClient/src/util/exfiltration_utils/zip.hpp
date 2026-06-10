#pragma once
#include <exfiltration_utils/mem_store.hpp>
#include <vector>
#include <cstdint>

namespace zip_utils {
    //builds an AES-256 encrypted zip entirely in memory from the mem_store file map 
    std::vector<uint8_t> zip_to_buf(const mem_store::FileMap& files);
}
