// A car (Data\Cars\*.car, CARFILE > CARDATA): its textures, materials, meshes and part list.
// Read straight from the bytes (a car is up to 22 MB of small tags); `header_only` stops after
// the identity and the parts, skipping the meshes and textures, which is what the game keeps for
// every car it knows about until one is shown.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ghg::fmt {

struct Vec2f {
    float x = 0, y = 0;
};
struct Vec3f {
    float x = 0, y = 0, z = 0;
};

struct CarTexture {
    std::string name;             // the artist's source file (never loaded)
    int flags = 0;
    int width = 0, height = 0;
    int format = 0;               // 16 RGB565, 8 paletted RGB, 32 BGRA
    std::vector<uint8_t> rgba;    // decoded, top row first
    bool has_alpha = false;       // a 32-bit texture with any alpha below 255
};

struct CarMaterial {
    std::string name;
    Vec3f color{1, 1, 1};
    int blend_src = 1, blend_dst = 0;   // 1,0 opaque; 4,5 alpha blend; 1,1 add
    int shade_alpha = 0, shade_color = 2;
    float alpha = 1;
    int flags = 0;
    int tex1 = -1, tex2 = -1;
};

struct CarVertex {
    Vec3f pos, normal;
};

struct CarPoly {
    std::array<int32_t, 3> v{};
    std::array<Vec2f, 3> uv{};
    int32_t material = 0;          // 666 = the car's paint
};

struct CarMesh {
    std::string name;
    int index = -1;
    Vec3f loc;
    Vec3f axis_x{1, 0, 0}, axis_y{0, 1, 0}, axis_z{0, 0, 1};
    std::vector<CarVertex> verts;
    std::vector<CarPoly> polys;
};

// A part's own materials as runs (PD_MULTIMAT): polygon i takes materials[k] until i reaches
// switch_polys[k] - 1, then k moves on (the list ends with -1).
struct CarMultiMat {
    std::vector<int32_t> materials;
    std::vector<int32_t> switch_polys;
};

struct CarPart {
    int id = 0;
    std::string name;
    bool custom = false;            // a customisation: never on a new car
    int region = 0;                 // 1 engine, 2 body, 3 running gear
    std::string mesh;
    float cost_min = 0, cost_max = 0;
    std::vector<int32_t> attach_dep;   // parts that must be on first (a negative id ends the list)
    std::vector<int32_t> remove_dep;   // parts that must come off first
    int mutex_set = 0;              // a bit mask: parts sharing a bit replace each other
    int amea = 0;                   // an attach dependency is met by any part sharing its mutex bits
    int vic = 0;                    // shown in the Complete view
    int special = 0;                // 1 engine block, 2 starter, 3 spins when running, 4 accessory
    std::vector<Vec3f> bolts;       // car space
    std::vector<CarMultiMat> multimat;
};

struct CarIgnitionSound {
    int part_id = 0;
    std::vector<uint8_t> wav;       // may be empty (the part only)
};

struct CarData {
    int version = 0;
    int id = 0;
    std::string name;
    int min_skill = 0;
    std::vector<CarTexture> textures;
    std::vector<CarMaterial> materials;
    std::vector<CarMesh> meshes;       // CD_AUTOMESH
    std::vector<CarMesh> spec_meshes;  // CD_SPECMESH: the shine pass, MESH_INDEX = its body mesh
    std::vector<CarPart> parts;
    std::vector<CarIgnitionSound> ignition;
    bool full = false;                 // meshes and textures read

    const CarPart* part(int id) const;
    const CarMesh* mesh(std::string_view name) const;
};

bool read_car(std::span<const uint8_t> bytes, CarData& out, bool header_only, std::string* error = nullptr);
std::optional<CarData> load_car(const std::filesystem::path& file, bool header_only, std::string* error = nullptr);

// Reads up to `bytes` at `offset` of a car file wherever it lives (a loose file, a member of the
// Android app's package); returns the count read.
using ReadAt = std::function<size_t(uint64_t offset, void* out, size_t bytes)>;
// The identity and parts alone (CD_ID, CD_NAME, CD_MINSKILL, the parts, the ignition sounds),
// skipping over the textures, materials and meshes.
std::optional<CarData> load_car_header(const ReadAt& read, uint64_t file_size, std::string* error = nullptr);

// One BITMAP container (width, height, format, data, palette) to RGBA, top row first.
bool decode_bitmap(int width, int height, int format, std::span<const uint8_t> data, std::span<const uint8_t> palette,
                   std::vector<uint8_t>& rgba, bool* has_alpha = nullptr);

}  // namespace ghg::fmt
