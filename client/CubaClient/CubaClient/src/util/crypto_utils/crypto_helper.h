
#pragma once
#include <vector>
#include <windows.h>

namespace crypto_helper
{
    //DPAPI wrapper
    std::vector<BYTE> dpapi_unprotect(const std::vector<BYTE>& encrypted, const std::vector<BYTE>* entropy = nullptr);

	//AES-GCM BCrypt wrapper (modern chrome)
    std::vector<BYTE> aes_gcm_decrypt(const std::vector<BYTE>& ciphertext, const std::vector<BYTE>& key);
}