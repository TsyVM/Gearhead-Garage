#include "Formats/DatArchive.hpp"

#include "Engine/Core/Inflate.hpp"

#include <array>
#include <cstring>
#include <fstream>

namespace ghg::fmt {

namespace {

// ── The key table (ghg.exe 0x47d2a0) ────────────────────────────────────────────

constexpr char kKeyString[] =
    "98ru91nb98fH98ufkfQkRAnf09i09kmvVnSjzAc9iu28rir09akdnBCn289ua9f09aRFnf029ur09akQRtanmzlkAvjb983ur9jaopVAkfmlzrWknlRzxc90i3r09uqtARankfnQdf";

const std::array<uint8_t, 1024>& key_table() {
    static const std::array<uint8_t, 1024> table = [] {
        std::array<uint8_t, 1024> k{};
        for (size_t i = 0; i < k.size(); ++i) k[i] = uint8_t(i);
        const size_t n = sizeof(kKeyString) - 1;
        size_t s = 0;
        for (int round = 0; round < 7; ++round) {
            for (size_t i = 0; i < 1024; ++i) {
                k[i] = uint8_t(k[i] + round + 0x11);
                k[i] = uint8_t(int8_t(kKeyString[s]) * int8_t(k[i]));
                const uint8_t add = uint8_t(uint8_t(kKeyString[n - 1 - s]) + k[i]);
                k[(i + 0x13D) & 0x3FF] = uint8_t(k[(i + 0x13D) & 0x3FF] + add);
                if (++s >= n) s = 0;
            }
        }
        return k;
    }();
    return table;
}

// ── ICE ────────────────────────────────────────────────────────────────────────

constexpr int kSmod[4][4] = {{333, 313, 505, 369}, {379, 375, 319, 391}, {361, 445, 451, 397}, {397, 425, 395, 505}};
constexpr int kSxor[4][4] = {{0x83, 0x85, 0x9b, 0xcd}, {0xcc, 0xa7, 0xad, 0x41}, {0x4b, 0x2e, 0xd4, 0x33}, {0xea, 0xcb, 0x2e, 0x04}};
constexpr uint32_t kPbox[32] = {
    0x00000001, 0x00000080, 0x00000400, 0x00002000, 0x00080000, 0x00200000, 0x01000000, 0x40000000,
    0x00000008, 0x00000020, 0x00000100, 0x00004000, 0x00010000, 0x00800000, 0x04000000, 0x20000000,
    0x00000004, 0x00000010, 0x00000200, 0x00008000, 0x00020000, 0x00400000, 0x08000000, 0x10000000,
    0x00000002, 0x00000040, 0x00000800, 0x00001000, 0x00040000, 0x00100000, 0x02000000, 0x80000000};
constexpr int kKeyrot[8] = {0, 1, 2, 3, 2, 1, 3, 0};
// The key: these eight bytes, byte i & 7 XORed with each of the 40 bytes at ghg.exe 0x4f1794.
constexpr uint8_t kIceSeed[8] = {0x7f, 0x2d, 0x1d, 0x3b, 0x44, 0xc7, 0xfe, 0x86};
constexpr uint8_t kIceMix[40] = {0x98, 0x7c, 0xc7, 0x4a, 0x17, 0x1d, 0x31, 0x3b, 0xc3, 0x3f, 0x1f, 0xf3, 0xdd, 0xd1,
                                 0xc9, 0x34, 0x24, 0x81, 0xb9, 0x7d, 0x59, 0xed, 0x0e, 0x37, 0xb1, 0x1f, 0x07, 0x50,
                                 0x90, 0xdc, 0x81, 0x89, 0x5b, 0x39, 0xc2, 0x7e, 0xac, 0xf4, 0xc9, 0xd9};

uint32_t gf_mult(uint32_t a, uint32_t b, uint32_t m) {
    uint32_t r = 0;
    while (b) {
        if (b & 1) r ^= a;
        a <<= 1;
        b >>= 1;
        if (a >= 256) a ^= m;
    }
    return r;
}

uint32_t gf_exp7(uint32_t b, uint32_t m) {
    if (b == 0) return 0;
    uint32_t x = gf_mult(b, b, m);
    x = gf_mult(b, x, m);
    x = gf_mult(x, x, m);
    return gf_mult(b, x, m);
}

uint32_t perm32(uint32_t x) {
    uint32_t r = 0;
    for (int i = 0; x; ++i, x >>= 1)
        if (x & 1) r |= kPbox[i];
    return r;
}

struct Ice {
    uint32_t sbox[4][1024];
    uint32_t sched[8][3];

    Ice() {
        for (int i = 0; i < 1024; ++i) {
            const int col = (i >> 1) & 0xff, row = (i & 1) | ((i & 0x200) >> 8);
            sbox[0][i] = perm32(gf_exp7(uint32_t(col ^ kSxor[0][row]), uint32_t(kSmod[0][row])) << 24);
            sbox[1][i] = perm32(gf_exp7(uint32_t(col ^ kSxor[1][row]), uint32_t(kSmod[1][row])) << 16);
            sbox[2][i] = perm32(gf_exp7(uint32_t(col ^ kSxor[2][row]), uint32_t(kSmod[2][row])) << 8);
            sbox[3][i] = perm32(gf_exp7(uint32_t(col ^ kSxor[3][row]), uint32_t(kSmod[3][row])));
        }
        uint8_t key[8];
        std::memcpy(key, kIceSeed, 8);
        for (int i = 0; i < 40; ++i) key[i & 7] ^= kIceMix[i];
        uint16_t kb[4];
        for (int i = 0; i < 4; ++i) kb[3 - i] = uint16_t(key[i * 2] << 8 | key[i * 2 + 1]);
        for (int i = 0; i < 8; ++i) {
            uint32_t* sk = sched[i];
            sk[0] = sk[1] = sk[2] = 0;
            for (int j = 0; j < 15; ++j)
                for (int k = 0; k < 4; ++k) {
                    uint16_t& w = kb[(kKeyrot[i] + k) & 3];
                    const uint32_t bit = w & 1;
                    sk[j % 3] = (sk[j % 3] << 1) | bit;
                    w = uint16_t((w >> 1) | ((bit ^ 1) << 15));
                }
        }
    }

    uint32_t f(uint32_t p, const uint32_t* sk) const {
        const uint32_t tl = ((p >> 16) & 0x3ff) | (((p >> 14) | (p << 18)) & 0xffc00);
        const uint32_t tr = (p & 0x3ff) | ((p << 2) & 0xffc00);
        uint32_t al = sk[2] & (tl ^ tr);
        uint32_t ar = al ^ tr;
        al ^= tl;
        al ^= sk[0];
        ar ^= sk[1];
        return sbox[0][al >> 10] | sbox[1][al & 0x3ff] | sbox[2][ar >> 10] | sbox[3][ar & 0x3ff];
    }

    void decrypt(uint8_t* b) const {
        uint32_t l = uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
        uint32_t r = uint32_t(b[4]) << 24 | uint32_t(b[5]) << 16 | uint32_t(b[6]) << 8 | b[7];
        for (int i = 7; i > 0; i -= 2) {
            l ^= f(r, sched[i]);
            r ^= f(l, sched[i - 1]);
        }
        for (int i = 0; i < 4; ++i) {
            b[3 - i] = uint8_t(r);
            b[7 - i] = uint8_t(l);
            r >>= 8;
            l >>= 8;
        }
    }
};

const Ice& ice() {
    static const Ice instance;
    return instance;
}

uint32_t rd32(const uint8_t* p) { return uint32_t(p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24); }

}  // namespace

void ice_decrypt_archive(uint8_t* data, size_t size) {
    const Ice& c = ice();
    for (size_t o = 0; o + 8 <= size; o += 8) c.decrypt(data + o);
}

void xor_key_table(uint8_t* data, size_t size) {
    const auto& k = key_table();
    for (size_t i = 0; i < size; ++i) data[i] ^= k[i & 0x3FF];
}

std::string DatArchive::normalize(std::string_view name) {
    std::string s;
    s.reserve(name.size());
    for (char c : name) {
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        s.push_back(c);
    }
    while (s.starts_with("./")) s.erase(0, 2);
    while (s.starts_with("/")) s.erase(0, 1);
    return s;
}

bool DatArchive::open(const std::filesystem::path& path, std::string* error) {
    auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    std::ifstream in(path, std::ios::binary);
    if (!in) return fail("cannot open");
    in.seekg(0, std::ios::end);
    std::vector<uint8_t> bytes(size_t(in.tellg()));
    in.seekg(0);
    if (!bytes.empty() && !in.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()))) return fail("cannot read");
    return open(std::move(bytes), path, error);
}

bool DatArchive::open(std::vector<uint8_t> bytes, const std::filesystem::path& name, std::string* error) {
    auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    path_ = name;
    members_.clear();
    index_.clear();
    file_ = std::move(bytes);
    const size_t size = file_.size();

    // The text header ends at the first zero byte (within the first 1024 bytes).
    size_t z = 0;
    while (z < size && z < 1024 && file_[z] != 0) ++z;
    if (z >= size || z >= 1024) return fail("no header terminator");
    size_t pos = z + 1;
    if (pos + 16 > size) return fail("truncated");
    char magic[8];
    for (int i = 0; i < 8; ++i) magic[i] = char(file_[pos + i] ^ file_[pos + 8 + i]);
    if (std::memcmp(magic, "cLib!317", 8) != 0) return fail("not a cLib!317 archive");
    pos += 16;

    while (pos + 0x13C <= size) {
        uint8_t e[0x13C];
        std::memcpy(e, file_.data() + pos, sizeof e);
        xor_key_table(e, sizeof e);
        for (int i = 0; i < 7; ++i) {
            const uint32_t v = rd32(e + i * 4) ^ rd32(e + 0x120 + i * 4);
            std::memcpy(e + i * 4, &v, 4);
        }
        Member m;
        m.flags = rd32(e + 0);
        m.offset = rd32(e + 4);
        m.stored = rd32(e + 8);
        m.size = rd32(e + 0xC);
        const uint32_t next = rd32(e + 0x10);
        m.filetime = uint64_t(rd32(e + 0x14)) | uint64_t(rd32(e + 0x18)) << 32;
        size_t n = 0;
        while (n < 260 && e[0x1C + n]) ++n;
        m.name = normalize(std::string_view(reinterpret_cast<const char*>(e + 0x1C), n));
        if (m.name != "the lstream header") {
            if (size_t(m.offset) + m.stored > size) return fail("member runs past the end");
            index_[m.name] = members_.size();
            members_.push_back(std::move(m));
        }
        if (next <= pos) break;
        pos = next;
    }
    return true;
}

const DatArchive::Member* DatArchive::find(std::string_view name) const {
    const auto it = index_.find(normalize(name));
    return it == index_.end() ? nullptr : &members_[it->second];
}

std::optional<std::vector<uint8_t>> DatArchive::read(const Member& m, std::string* error) const {
    std::vector<uint8_t> data(file_.begin() + m.offset, file_.begin() + m.offset + m.stored);
    if (m.flags & 1) ice_decrypt_archive(data.data(), data.size());
    xor_key_table(data.data(), data.size());
    if (m.flags & 2) {
        std::vector<uint8_t> out;
        if (!eng::zlib_inflate(data, out, m.size)) {
            if (error) *error = "corrupt packed data in " + m.name;
            return std::nullopt;
        }
        return out;
    }
    return data;
}

std::optional<std::vector<uint8_t>> DatArchive::read(std::string_view name, std::string* error) const {
    const Member* m = find(name);
    if (!m) {
        if (error) *error = "no member " + std::string(name);
        return std::nullopt;
    }
    return read(*m, error);
}

}  // namespace ghg::fmt
