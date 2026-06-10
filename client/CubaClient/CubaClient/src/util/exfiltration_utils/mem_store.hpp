#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cstdint>
//shoutout *** and claude lol
namespace mem_store {
    using Buf     = std::vector<uint8_t>;
    using FileMap = std::unordered_map<std::string, Buf>;

    //append text to a virtual file. create entry if none
    void append(const std::string& vpath, const std::string& text);
    //write raw bytes, replaces existing
    void write_bytes(const std::string& vpath, const uint8_t* data, size_t len);
    //read a real file from disk into the store. skips on error
    void import_file(const std::string& vpath, const std::filesystem::path& src);
    //recursively import a directory tree. strips src_root from vpaths //#whatdatmean
    void import_tree(const std::string& vprefix, const std::filesystem::path& src_root,
                     uintmax_t max_file_bytes = UINTMAX_MAX);

    const FileMap& files();
    void clear();
}
