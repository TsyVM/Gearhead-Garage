// Autodesk 3D Studio (.3ds) scenes: the auction yard, the car lot and the three junkyard areas in
// Data\Scenes.dat. What the game uses: meshes (world-space vertices, faces, UVs, a material per
// face), materials (colours, transparency, a texture map), omni lights, cameras, and the named
// marker objects the program looks for (Floor, CamFocus, play_01/play_02, car_NN, Shelf01..).
// 3DS is Z up; nothing is converted here.
#pragma once

#include "Formats/CarFile.hpp"   // Vec2f, Vec3f

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ghg::fmt {

struct Max3dsMaterial {
    std::string name;
    Vec3f ambient{0.2f, 0.2f, 0.2f}, diffuse{0.8f, 0.8f, 0.8f}, specular{1, 1, 1};
    float shininess = 0;       // 0..1
    float transparency = 0;    // 0 opaque .. 1 invisible
    int shading = 3;           // 1 flat, 2 Gouraud, 3 Phong
    bool two_sided = false;
    bool additive = false;
    std::string texture;       // map file name, as written ("TOP.JPG")
    float texture_amount = 1;
    std::string opacity_map;
    Vec2f uv_scale{1, 1}, uv_offset{0, 0};
};

struct Max3dsFace {
    uint16_t v[3] = {};
    uint16_t flags = 0;
    int material = -1;         // index into Scene::materials
};

struct Max3dsMesh {
    std::string name;
    std::vector<Vec3f> verts;  // world space
    std::vector<Vec2f> uvs;    // one per vertex (may be empty)
    std::vector<Max3dsFace> faces;
    std::vector<uint32_t> smoothing;   // one per face (may be empty)
    Vec3f matrix[4] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};   // the object's frame
    bool hidden = false;
};

struct Max3dsLight {
    std::string name;
    Vec3f pos;
    Vec3f color{1, 1, 1};
    bool spot = false;
    Vec3f target;
    bool off = false;
    float multiplier = 1;
};

struct Max3dsCamera {
    std::string name;
    Vec3f pos, target;
    float roll = 0;            // degrees
    float lens = 50;           // mm
    float fov = 0;             // degrees, from the keyframer (0: none)
};

struct Max3dsScene {
    std::vector<Max3dsMaterial> materials;
    std::vector<Max3dsMesh> meshes;
    std::vector<Max3dsLight> lights;
    std::vector<Max3dsCamera> cameras;
    Vec3f ambient_light{0, 0, 0};
    float master_scale = 1;

    const Max3dsMesh* mesh(std::string_view name) const;
    int material_index(std::string_view name) const;
    // Field of view, degrees, of a camera: the keyframer's when it has one, else from the lens.
    static float camera_fov(const Max3dsCamera& c);
};

bool read_3ds(std::span<const uint8_t> bytes, Max3dsScene& out, std::string* error = nullptr);

}  // namespace ghg::fmt
