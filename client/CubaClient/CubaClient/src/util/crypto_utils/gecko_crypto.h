#pragma once
#include <vector>
#include <windows.h>

namespace gecko_crypto {
    using Bytes = std::vector<BYTE>;

    Bytes sha1(const Bytes& data);
    Bytes hmac_sha1(const Bytes& key, const Bytes& data);
    Bytes pbkdf2_sha256(const Bytes& password, const Bytes& salt, ULONG iterations, ULONG keylen);
    Bytes des3_cbc_decrypt(const Bytes& key24, const Bytes& iv8, const Bytes& data);
    Bytes aes256_cbc_decrypt(const Bytes& key32, const Bytes& iv, const Bytes& data);

    struct KeyIV { Bytes key; Bytes iv; bool valid = false; };
    
    KeyIV derive_3des(const Bytes& globalSalt, const Bytes& masterPass, const Bytes& entrySalt);
    Bytes derive_aes256(const Bytes& globalSalt, const Bytes& masterPass, const Bytes& entrySalt, ULONG iterations = 1);
}
