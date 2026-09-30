// The resource archives (Data\Gfx24.dat, Scenes.dat, Sound16.dat, Jobs\random.dat, ...): the
// "cLib!317" format of ghg.exe (see Docs/Research.md, "Resource archives"). Members are looked
// up by their lower-case, '/'-separated names ("fonts/standard.tga").
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ghg::fmt {

class DatArchive {
public:
    struct Member {
        std::string name;        // as stored, lower case with '/'
        uint32_t flags = 0;      // 1 ICE-encrypted, 2 zlib
        uint32_t offset = 0;
        uint32_t stored = 0;
        uint32_t size = 0;       // unpacked
        uint64_t filetime = 0;
    };

    bool open(const std::filesystem::path& path, std::string* error = nullptr);
    // The archive already read (a member of the Android app's package); `name` is for messages.
    bool open(std::vector<uint8_t> bytes, const std::filesystem::path& name, std::string* error = nullptr);
    const std::filesystem::path& path() const { return path_; }
    const std::vector<Member>& members() const { return members_; }
    const Member* find(std::string_view name) const;
    // The member's bytes, unpacked. Thread-safe.
    std::optional<std::vector<uint8_t>> read(const Member& m, std::string* error = nullptr) const;
    std::optional<std::vector<uint8_t>> read(std::string_view name, std::string* error = nullptr) const;

    // The normalised form a member is found by: lower case, '/' separators, no leading "./".
    static std::string normalize(std::string_view name);

private:
    std::filesystem::path path_;
    std::vector<uint8_t> file_;    // the whole archive (the largest, Sound16.dat, is 11 MB)
    std::vector<Member> members_;
    std::unordered_map<std::string, size_t> index_;
};

// Thin-ICE (ICE level 0, eight rounds; Matthew Kwan's cipher, public domain) as the archives use
// it: decrypts every whole 8-byte block in place.
void ice_decrypt_archive(uint8_t* data, size_t size);
// The archives' 1024-byte XOR key table, applied from index 0.
void xor_key_table(uint8_t* data, size_t size);

}  // namespace ghg::fmt
