// RIFF WAVE files (the game's sounds and music loops): 8- or 16-bit PCM, mono or stereo.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ghg::fmt {

struct WavData {
    int channels = 1;
    int rate = 22050;
    std::vector<int16_t> samples;   // interleaved
};

bool read_wav(std::span<const uint8_t> bytes, WavData& out, std::string* error = nullptr);

}  // namespace ghg::fmt
