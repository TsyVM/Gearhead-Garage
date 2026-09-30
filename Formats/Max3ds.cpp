#include "Formats/Max3ds.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace ghg::fmt {

namespace {

class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    size_t size() const { return n_; }
    template <class T>
    T get(size_t at) const {
        T v{};
        if (at + sizeof(T) <= n_) std::memcpy(&v, p_ + at, sizeof(T));
        return v;
    }
    std::string cstr(size_t at, size_t* after = nullptr) const {
        size_t e = at;
        while (e < n_ && p_[e]) ++e;
        if (after) *after = e < n_ ? e + 1 : n_;
        return std::string(reinterpret_cast<const char*>(p_ + at), e - at);
    }

private:
    const uint8_t* p_;
    size_t n_;
};

struct Chunk {
    uint16_t id;
    size_t body, end;
};

template <class F>
void for_chunks(const Reader& r, size_t from, size_t to, F&& f) {
    size_t at = from;
    while (at + 6 <= to) {
        const uint16_t id = r.get<uint16_t>(at);
        const uint32_t len = r.get<uint32_t>(at + 2);
        if (len < 6 || at + len > to) return;
        f(Chunk{id, at + 6, at + len});
        at += len;
    }
}

Vec3f read_color(const Reader& r, const Chunk& c) {
    Vec3f col{0, 0, 0};
    bool linear = false;
    for_chunks(r, c.body, c.end, [&](const Chunk& k) {
        if (k.id == 0x0010 || k.id == 0x0013) {   // float colour (gamma / linear)
            if (!linear) col = {r.get<float>(k.body), r.get<float>(k.body + 4), r.get<float>(k.body + 8)};
        } else if (k.id == 0x0011) {               // 24-bit colour
            if (!linear) col = {r.get<uint8_t>(k.body) / 255.0f, r.get<uint8_t>(k.body + 1) / 255.0f, r.get<uint8_t>(k.body + 2) / 255.0f};
        } else if (k.id == 0x0012) {               // linear 24-bit colour: preferred
            col = {r.get<uint8_t>(k.body) / 255.0f, r.get<uint8_t>(k.body + 1) / 255.0f, r.get<uint8_t>(k.body + 2) / 255.0f};
            linear = true;
        }
    });
    return col;
}

float read_percent(const Reader& r, const Chunk& c) {
    float v = 0;
    for_chunks(r, c.body, c.end, [&](const Chunk& k) {
        if (k.id == 0x0030) v = r.get<uint16_t>(k.body) / 100.0f;
        else if (k.id == 0x0031) v = r.get<float>(k.body) / 100.0f;
    });
    return v;
}

void read_map(const Reader& r, const Chunk& c, std::string& name, float& amount, Vec2f* scale, Vec2f* offset) {
    for_chunks(r, c.body, c.end, [&](const Chunk& k) {
        if (k.id == 0xA300) name = r.cstr(k.body);
        else if (k.id == 0x0030) amount = r.get<uint16_t>(k.body) / 100.0f;
        else if (k.id == 0xA354 && scale) scale->y = r.get<float>(k.body);   // V scale
        else if (k.id == 0xA356 && scale) scale->x = r.get<float>(k.body);   // U scale
        else if (k.id == 0xA358 && offset) offset->x = r.get<float>(k.body);
        else if (k.id == 0xA35A && offset) offset->y = r.get<float>(k.body);
    });
}

void read_material(const Reader& r, const Chunk& c, Max3dsMaterial& m) {
    for_chunks(r, c.body, c.end, [&](const Chunk& k) {
        switch (k.id) {
            case 0xA000: m.name = r.cstr(k.body); break;
            case 0xA010: m.ambient = read_color(r, k); break;
            case 0xA020: m.diffuse = read_color(r, k); break;
            case 0xA030: m.specular = read_color(r, k); break;
            case 0xA040: m.shininess = read_percent(r, k); break;
            case 0xA050: m.transparency = read_percent(r, k); break;
            case 0xA081: m.two_sided = true; break;
            case 0xA083: m.additive = true; break;
            case 0xA100: m.shading = r.get<uint16_t>(k.body); break;
            case 0xA200: read_map(r, k, m.texture, m.texture_amount, &m.uv_scale, &m.uv_offset); break;
            case 0xA210: {
                float a = 1;
                read_map(r, k, m.opacity_map, a, nullptr, nullptr);
                break;
            }
            default: break;
        }
    });
}

}  // namespace

const Max3dsMesh* Max3dsScene::mesh(std::string_view n) const {
    auto same = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(uint8_t(a[i])) != std::tolower(uint8_t(b[i]))) return false;
        return true;
    };
    for (const Max3dsMesh& m : meshes)
        if (same(m.name, n)) return &m;
    return nullptr;
}

int Max3dsScene::material_index(std::string_view n) const {
    for (size_t i = 0; i < materials.size(); ++i)
        if (materials[i].name == n) return int(i);
    return -1;
}

float Max3dsScene::camera_fov(const Max3dsCamera& c) {
    if (c.fov > 0) return c.fov;
    const float lens = c.lens > 1 ? c.lens : 50;
    return 2.0f * std::atan(21.635f / lens) * 57.29578f;
}

bool read_3ds(std::span<const uint8_t> bytes, Max3dsScene& out, std::string* error) {
    out = Max3dsScene{};
    const Reader r(bytes.data(), bytes.size());
    if (r.size() < 6 || r.get<uint16_t>(0) != 0x4D4D) {
        if (error) *error = "not a 3DS file";
        return false;
    }
    const size_t main_end = std::min<size_t>(r.get<uint32_t>(2), r.size());
    for_chunks(r, 6, main_end, [&](const Chunk& top) {
        if (top.id == 0x3D3D) {   // the editor data
            for_chunks(r, top.body, top.end, [&](const Chunk& e) {
                if (e.id == 0xAFFF) read_material(r, e, out.materials.emplace_back());
                else if (e.id == 0x0100) out.master_scale = r.get<float>(e.body);
                else if (e.id == 0x2100) out.ambient_light = read_color(r, e);
                else if (e.id == 0x4000) {
                    size_t after = 0;
                    const std::string name = r.cstr(e.body, &after);
                    for_chunks(r, after, e.end, [&](const Chunk& o) {
                        if (o.id == 0x4100) {
                            Max3dsMesh& m = out.meshes.emplace_back();
                            m.name = name;
                            for_chunks(r, o.body, o.end, [&](const Chunk& k) {
                                if (k.id == 0x4110) {
                                    const uint16_t n = r.get<uint16_t>(k.body);
                                    m.verts.resize(n);
                                    for (uint16_t i = 0; i < n; ++i)
                                        m.verts[i] = {r.get<float>(k.body + 2 + i * 12), r.get<float>(k.body + 6 + i * 12),
                                                      r.get<float>(k.body + 10 + i * 12)};
                                } else if (k.id == 0x4140) {
                                    const uint16_t n = r.get<uint16_t>(k.body);
                                    m.uvs.resize(n);
                                    for (uint16_t i = 0; i < n; ++i)
                                        m.uvs[i] = {r.get<float>(k.body + 2 + i * 8), r.get<float>(k.body + 6 + i * 8)};
                                } else if (k.id == 0x4160) {
                                    for (int i = 0; i < 4; ++i)
                                        m.matrix[i] = {r.get<float>(k.body + i * 12), r.get<float>(k.body + 4 + i * 12),
                                                       r.get<float>(k.body + 8 + i * 12)};
                                } else if (k.id == 0x4120) {
                                    const uint16_t n = r.get<uint16_t>(k.body);
                                    m.faces.resize(n);
                                    for (uint16_t i = 0; i < n; ++i) {
                                        const size_t at = k.body + 2 + size_t(i) * 8;
                                        m.faces[i].v[0] = r.get<uint16_t>(at);
                                        m.faces[i].v[1] = r.get<uint16_t>(at + 2);
                                        m.faces[i].v[2] = r.get<uint16_t>(at + 4);
                                        m.faces[i].flags = r.get<uint16_t>(at + 6);
                                    }
                                    const size_t sub = k.body + 2 + size_t(n) * 8;
                                    for_chunks(r, sub, k.end, [&](const Chunk& f) {
                                        if (f.id == 0x4130) {
                                            size_t a2 = 0;
                                            const std::string mat = r.cstr(f.body, &a2);
                                            const int mi = out.material_index(mat);
                                            const uint16_t cnt = r.get<uint16_t>(a2);
                                            for (uint16_t j = 0; j < cnt; ++j) {
                                                const uint16_t fi = r.get<uint16_t>(a2 + 2 + j * 2);
                                                if (fi < m.faces.size()) m.faces[fi].material = mi;
                                            }
                                        } else if (f.id == 0x4150) {
                                            m.smoothing.resize(m.faces.size());
                                            for (size_t j = 0; j < m.faces.size(); ++j) m.smoothing[j] = r.get<uint32_t>(f.body + j * 4);
                                        }
                                    });
                                }
                            });
                            for (Max3dsFace& f : m.faces)
                                for (uint16_t& v : f.v)
                                    if (v >= m.verts.size()) v = 0;
                        } else if (o.id == 0x4600) {
                            Max3dsLight& l = out.lights.emplace_back();
                            l.name = name;
                            l.pos = {r.get<float>(o.body), r.get<float>(o.body + 4), r.get<float>(o.body + 8)};
                            for_chunks(r, o.body + 12, o.end, [&](const Chunk& k) {
                                if (k.id == 0x0010 || k.id == 0x0011) {
                                    if (k.id == 0x0010) l.color = {r.get<float>(k.body), r.get<float>(k.body + 4), r.get<float>(k.body + 8)};
                                    else l.color = {r.get<uint8_t>(k.body) / 255.0f, r.get<uint8_t>(k.body + 1) / 255.0f, r.get<uint8_t>(k.body + 2) / 255.0f};
                                } else if (k.id == 0x4610) {
                                    l.spot = true;
                                    l.target = {r.get<float>(k.body), r.get<float>(k.body + 4), r.get<float>(k.body + 8)};
                                } else if (k.id == 0x4620) {
                                    l.off = true;
                                } else if (k.id == 0x465B) {
                                    l.multiplier = r.get<float>(k.body);
                                }
                            });
                        } else if (o.id == 0x4700) {
                            Max3dsCamera& c = out.cameras.emplace_back();
                            c.name = name;
                            c.pos = {r.get<float>(o.body), r.get<float>(o.body + 4), r.get<float>(o.body + 8)};
                            c.target = {r.get<float>(o.body + 12), r.get<float>(o.body + 16), r.get<float>(o.body + 20)};
                            c.roll = r.get<float>(o.body + 24);
                            c.lens = r.get<float>(o.body + 28);
                        } else if (o.id == 0x4010) {
                            // hidden object: marked on the mesh read next or before
                            for (auto it = out.meshes.rbegin(); it != out.meshes.rend(); ++it)
                                if (it->name == name) it->hidden = true;
                        }
                    });
                }
            });
        } else if (top.id == 0xB000) {   // the keyframer: only the cameras' field of view is used
            for_chunks(r, top.body, top.end, [&](const Chunk& node) {
                if (node.id != 0xB003) return;
                std::string name;
                float fov = 0;
                for_chunks(r, node.body, node.end, [&](const Chunk& k) {
                    if (k.id == 0xB010) name = r.cstr(k.body);
                    else if (k.id == 0xB023) {
                        const uint32_t keys = r.get<uint32_t>(k.body + 10);
                        if (keys == 0) return;
                        size_t at = k.body + 14;
                        const uint16_t flags = r.get<uint16_t>(at + 4);
                        at += 6;
                        for (int b = 0; b < 5; ++b)
                            if (flags & (1 << b)) at += 4;
                        fov = r.get<float>(at);
                    }
                });
                for (Max3dsCamera& c : out.cameras)
                    if (c.name == name && fov > 0) c.fov = fov;
            });
        }
    });
    return true;
}

}  // namespace ghg::fmt
