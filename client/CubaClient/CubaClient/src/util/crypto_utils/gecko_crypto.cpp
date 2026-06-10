#include "gecko_crypto.h"
#include <bcrypt.h>
#include <stdexcept>

namespace gecko_crypto {

static Bytes bcrypt_hash(LPCWSTR alg, const Bytes& data, ULONG flags, const Bytes* hmac_key = nullptr) {
    BCRYPT_ALG_HANDLE hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    BCryptOpenAlgorithmProvider(&hAlg, alg, nullptr, flags);

    ULONG hashLen = 0, dummy = 0;
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &dummy, 0);

    if (hmac_key)
        BCryptCreateHash(hAlg, &hHash, nullptr, 0, const_cast<PUCHAR>(hmac_key->data()), (ULONG)hmac_key->size(), 0);
    else
        BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);

    BCryptHashData(hHash, const_cast<PUCHAR>(data.data()), (ULONG)data.size(), 0);

    Bytes result(hashLen);
    BCryptFinishHash(hHash, result.data(), hashLen, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

Bytes sha1(const Bytes& data) {
    return bcrypt_hash(BCRYPT_SHA1_ALGORITHM, data, 0);
}

Bytes hmac_sha1(const Bytes& key, const Bytes& data) {
    return bcrypt_hash(BCRYPT_SHA1_ALGORITHM, data, BCRYPT_ALG_HANDLE_HMAC_FLAG, &key);
}

Bytes pbkdf2_sha256(const Bytes& password, const Bytes& salt, ULONG iterations, ULONG keylen) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    Bytes result(keylen);
    BCryptDeriveKeyPBKDF2(hAlg,
        const_cast<PUCHAR>(password.data()), (ULONG)password.size(),
        const_cast<PUCHAR>(salt.data()),     (ULONG)salt.size(),
        iterations, result.data(), keylen, 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

static Bytes bcrypt_sym_decrypt(LPCWSTR alg, LPCWSTR mode, const Bytes& key, const Bytes& iv, const Bytes& data) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    BCryptOpenAlgorithmProvider(&hAlg, alg, nullptr, 0);
    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)mode,
                      (ULONG)((wcslen(mode) + 1) * sizeof(wchar_t)), 0);
    BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                               const_cast<PUCHAR>(key.data()), (ULONG)key.size(), 0);
    Bytes iv_copy = iv;
    ULONG result_len = 0;
    Bytes result(data.size());
    BCryptDecrypt(hKey, const_cast<PUCHAR>(data.data()), (ULONG)data.size(), nullptr,
                  iv_copy.data(), (ULONG)iv_copy.size(),
                  result.data(), (ULONG)result.size(), &result_len, 0);
    result.resize(result_len);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

Bytes des3_cbc_decrypt(const Bytes& key24, const Bytes& iv8, const Bytes& data) {
    return bcrypt_sym_decrypt(BCRYPT_3DES_ALGORITHM, BCRYPT_CHAIN_MODE_CBC, key24, iv8, data);
}

Bytes aes256_cbc_decrypt(const Bytes& key32, const Bytes& iv, const Bytes& data) {
    return bcrypt_sym_decrypt(BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_CBC, key32, iv, data);
}

static Bytes concat(const Bytes& a, const Bytes& b) {
    Bytes r = a;
    r.insert(r.end(), b.begin(), b.end());
    return r;
}

KeyIV derive_3des(const Bytes& globalSalt, const Bytes& masterPass, const Bytes& entrySalt) {
    if (entrySalt.size() > 20) return {};

    Bytes hp  = sha1(concat(globalSalt, masterPass));
    Bytes chp = sha1(concat(hp, entrySalt));

    Bytes pes = entrySalt;
    pes.resize(20, 0);

    Bytes k1  = hmac_sha1(chp, concat(pes, entrySalt));
    Bytes tmp = hmac_sha1(chp, pes);
    Bytes k2  = hmac_sha1(chp, concat(tmp, entrySalt));

    Bytes k = concat(k1, k2);
    if (k.size() < 32) return {};

    KeyIV out;
    out.key   = Bytes(k.begin(), k.begin() + 24);
    out.iv    = Bytes(k.end() - 8, k.end());
    out.valid = true;
    return out;
}

Bytes derive_aes256(const Bytes& globalSalt, const Bytes& masterPass, const Bytes& entrySalt, ULONG iterations) {
    Bytes hp = sha1(concat(globalSalt, masterPass));
    return pbkdf2_sha256(hp, entrySalt, iterations, 32);
}

} //namespace gecko_crypto
