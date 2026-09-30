#include "Formats/Wav.hpp"

#include <cstring>

namespace ghg::fmt {

bool read_wav(std::span<const uint8_t> b, WavData& out, std::string* error) {
    auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    auto u16 = [&](size_t at) { return uint16_t(b[at] | b[at + 1] << 8); };
    auto u32 = [&](size_t at) { return uint32_t(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | uint32_t(b[at + 3]) << 24); };
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0)
        return fail("not a WAVE file");
    int format = 0, bits = 0;
    const uint8_t* data = nullptr;
    size_t data_size = 0;
    size_t at = 12;
    while (at + 8 <= b.size()) {
        const uint32_t size = u32(at + 4);
        const size_t body = at + 8;
        const size_t avail = b.size() - body;
        if (std::memcmp(b.data() + at, "fmt ", 4) == 0 && size >= 16 && avail >= 16) {
            format = u16(body);
            out.channels = u16(body + 2);
            out.rate = int(u32(body + 4));
            bits = u16(body + 14);
        } else if (std::memcmp(b.data() + at, "data", 4) == 0) {
            data = b.data() + body;
            data_size = size < avail ? size : avail;   // some files overstate their data
        }
        at = body + size + (size & 1);
    }
    if (format != 1) return fail("not PCM");
    if (!data) return fail("no data");
    if (out.channels < 1 || out.channels > 2 || out.rate <= 0) return fail("unsupported layout");
    if (bits == 16) {
        out.samples.resize(data_size / 2);
        std::memcpy(out.samples.data(), data, out.samples.size() * 2);
    } else if (bits == 8) {
        out.samples.resize(data_size);
        for (size_t i = 0; i < data_size; ++i) out.samples[i] = int16_t((int(data[i]) - 128) << 8);
    } else {
        return fail("unsupported sample size");
    }
    out.samples.resize(out.samples.size() / size_t(out.channels) * size_t(out.channels));
    return true;
}

}  // namespace ghg::fmt
