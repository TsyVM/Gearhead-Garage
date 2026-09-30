#include "Formats/CarFile.hpp"

#include "Formats/TagFile.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace ghg::fmt {

namespace {

// A tag seen in place: its id, type and payload.
struct Tag {
    uint32_t id = 0;
    TagType type = TagType::Raw;
    const uint8_t* data = nullptr;
    uint32_t size = 0;
};

// Walks the tags of one container's payload.
class Walker {
public:
    Walker(const uint8_t* p, size_t n) : p_(p), end_(p + n) {}
    bool next(Tag& t) {
        if (size_t(end_ - p_) < 8) return false;
        uint32_t head, len;
        std::memcpy(&head, p_, 4);
        std::memcpy(&len, p_ + 4, 4);
        if (len > size_t(end_ - p_) - 8) {
            bad_ = true;
            return false;
        }
        t.id = head & 0x0FFFFFFF;
        t.type = TagType(head >> 28);
        t.data = p_ + 8;
        t.size = len;
        p_ += 8 + size_t(len);
        return true;
    }
    bool bad() const { return bad_; }

private:
    const uint8_t* p_;
    const uint8_t* end_;
    bool bad_ = false;
};

int32_t i32(const Tag& t) {
    int32_t v = 0;
    if (t.size >= 4) std::memcpy(&v, t.data, 4);
    return v;
}
float f32(const Tag& t) {
    float v = 0;
    if (t.size >= 4) std::memcpy(&v, t.data, 4);
    return v;
}
std::string str(const Tag& t) {
    std::string s(reinterpret_cast<const char*>(t.data), t.size);
    if (const auto z = s.find('\0'); z != std::string::npos) s.resize(z);
    return s;
}
Vec3f v3(const Tag& t) {
    Vec3f v;
    if (t.size >= 12) std::memcpy(&v, t.data, 12);
    return v;
}
Vec2f v2(const Tag& t) {
    Vec2f v;
    if (t.size >= 8) std::memcpy(&v, t.data, 8);
    return v;
}
std::vector<int32_t> ints(const Tag& t) {
    std::vector<int32_t> v(t.size / 4);
    if (!v.empty()) std::memcpy(v.data(), t.data, v.size() * 4);
    return v;
}

bool read_texture(const Tag& c, CarTexture& tex) {
    Walker w(c.data, c.size);
    Tag t;
    while (w.next(t)) {
        if (t.id == tag::TEX_NAME) tex.name = str(t);
        else if (t.id == tag::TEX_FLAGS) tex.flags = i32(t);
        else if (t.id == tag::TEX_BITMAP) {
            Walker b(t.data, t.size);
            Tag u;
            std::span<const uint8_t> data, palette;
            while (b.next(u)) {
                if (u.id == tag::BITMAP_WIDTH) tex.width = i32(u);
                else if (u.id == tag::BITMAP_HEIGHT) tex.height = i32(u);
                else if (u.id == tag::BITMAP_FORMAT) tex.format = i32(u);
                else if (u.id == tag::BITMAP_DATA) data = {u.data, u.size};
                else if (u.id == tag::BITMAP_PALETTE) palette = {u.data, u.size};
            }
            if (!decode_bitmap(tex.width, tex.height, tex.format, data, palette, tex.rgba, &tex.has_alpha)) return false;
        }
    }
    return !w.bad();
}

void read_material(const Tag& c, CarMaterial& m) {
    Walker w(c.data, c.size);
    Tag t;
    while (w.next(t)) {
        switch (t.id) {
            case tag::MAT_NAME: m.name = str(t); break;
            case tag::MAT_COLOR: m.color = v3(t); break;
            case tag::MAT_BLENDSRC: m.blend_src = i32(t); break;
            case tag::MAT_BLENDDST: m.blend_dst = i32(t); break;
            case tag::MAT_SHADEALPHA: m.shade_alpha = i32(t); break;
            case tag::MAT_SHADECOLOR: m.shade_color = i32(t); break;
            case tag::MAT_ALPHA: m.alpha = f32(t); break;
            case tag::MAT_FLAGS: m.flags = i32(t); break;
            case tag::MAT_TEXINDEX1: m.tex1 = i32(t); break;
            case tag::MAT_TEXINDEX2: m.tex2 = i32(t); break;
            default: break;
        }
    }
}

bool read_mesh(const Tag& c, CarMesh& m) {
    Walker w(c.data, c.size);
    Tag t;
    while (w.next(t)) {
        switch (t.id) {
            case tag::MESH_NAME: m.name = str(t); break;
            case tag::MESH_INDEX: m.index = i32(t); break;
            case tag::MESH_LOC: m.loc = v3(t); break;
            case tag::MESH_MATRIXX: m.axis_x = v3(t); break;
            case tag::MESH_MATRIXY: m.axis_y = v3(t); break;
            case tag::MESH_MATRIXZ: m.axis_z = v3(t); break;
            case tag::MESH_NUMPOLYS: m.polys.reserve(size_t(std::max(0, i32(t)))); break;
            case tag::MESH_NUMVERTS: m.verts.reserve(size_t(std::max(0, i32(t)))); break;
            case tag::MESH_VERT: {
                CarVertex v;
                Walker vw(t.data, t.size);
                Tag u;
                while (vw.next(u)) {
                    if (u.id == tag::VERT_LOC) v.pos = v3(u);
                    else if (u.id == tag::VERT_NORMAL) v.normal = v3(u);
                }
                m.verts.push_back(v);
                break;
            }
            case tag::MESH_POLY: {
                CarPoly p;
                Walker pw(t.data, t.size);
                Tag u;
                while (pw.next(u)) {
                    switch (u.id) {
                        case tag::POLY_VERTINDEX1: p.v[0] = i32(u); break;
                        case tag::POLY_VERTINDEX2: p.v[1] = i32(u); break;
                        case tag::POLY_VERTINDEX3: p.v[2] = i32(u); break;
                        case tag::POLY_MAP1: p.uv[0] = v2(u); break;
                        case tag::POLY_MAP2: p.uv[1] = v2(u); break;
                        case tag::POLY_MAP3: p.uv[2] = v2(u); break;
                        case tag::POLY_MATINDEX: p.material = i32(u); break;
                        default: break;
                    }
                }
                m.polys.push_back(p);
                break;
            }
            default: break;
        }
    }
    if (w.bad()) return false;
    // Indices out of range would read past the vertex list: clamp them to the first vertex.
    for (CarPoly& p : m.polys)
        for (int32_t& v : p.v)
            if (v < 0 || size_t(v) >= m.verts.size()) v = 0;
    return true;
}

void read_part(const Tag& c, CarPart& p) {
    Walker w(c.data, c.size);
    Tag t;
    while (w.next(t)) {
        switch (t.id) {
            case tag::PD_ID: p.id = i32(t); break;
            case tag::PD_NAME: p.name = str(t); break;
            case tag::PD_CUSTOM: p.custom = i32(t) != 0; break;
            case tag::PD_REGION: p.region = i32(t); break;
            case tag::PD_MESHNAME: p.mesh = str(t); break;
            case tag::PD_COSTMIN: p.cost_min = f32(t); break;
            case tag::PD_COSTMAX: p.cost_max = f32(t); break;
            case tag::PD_ATTACHDEP: p.attach_dep = ints(t); break;
            case tag::PD_REMOVEDEP: p.remove_dep = ints(t); break;
            case tag::PD_MUTEXCSET: p.mutex_set = i32(t); break;
            case tag::PD_AMEA: p.amea = i32(t); break;
            case tag::PD_VIC: p.vic = i32(t); break;
            case tag::PD_SPECIAL: p.special = i32(t); break;
            case tag::PD_BOLT: p.bolts.push_back(v3(t)); break;
            case tag::PD_MULTIMAT: {
                CarMultiMat mm;
                Walker mw(t.data, t.size);
                Tag u;
                while (mw.next(u)) {
                    if (u.id == tag::MMAT_MATERIALINDEX) mm.materials.push_back(i32(u));
                    else if (u.id == tag::MMAT_SWITCHPOLYLIST) mm.switch_polys = ints(u);
                }
                p.multimat.push_back(std::move(mm));
                break;
            }
            default: break;
        }
    }
}

}  // namespace

const CarPart* CarData::part(int pid) const {
    for (const CarPart& p : parts)
        if (p.id == pid) return &p;
    return nullptr;
}

const CarMesh* CarData::mesh(std::string_view n) const {
    for (const CarMesh& m : meshes)
        if (m.name == n) return &m;
    return nullptr;
}

bool decode_bitmap(int width, int height, int format, std::span<const uint8_t> data, std::span<const uint8_t> palette,
                   std::vector<uint8_t>& rgba, bool* has_alpha) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
    const size_t n = size_t(width) * size_t(height);
    rgba.assign(n * 4, 255);
    bool alpha = false;
    if (format == 16) {
        if (data.size() < n * 2) return false;
        for (size_t i = 0; i < n; ++i) {
            const uint16_t v = uint16_t(data[i * 2] | data[i * 2 + 1] << 8);
            const uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
            rgba[i * 4 + 0] = uint8_t((r << 3) | (r >> 2));
            rgba[i * 4 + 1] = uint8_t((g << 2) | (g >> 4));
            rgba[i * 4 + 2] = uint8_t((b << 3) | (b >> 2));
        }
    } else if (format == 32) {
        if (data.size() < n * 4) return false;
        for (size_t i = 0; i < n; ++i) {
            rgba[i * 4 + 0] = data[i * 4 + 2];
            rgba[i * 4 + 1] = data[i * 4 + 1];
            rgba[i * 4 + 2] = data[i * 4 + 0];
            rgba[i * 4 + 3] = data[i * 4 + 3];
            if (data[i * 4 + 3] != 255) alpha = true;
        }
    } else if (format == 8) {
        if (data.size() < n) return false;
        for (size_t i = 0; i < n; ++i) {
            const size_t e = size_t(data[i]) * 3;
            if (e + 2 < palette.size()) {
                rgba[i * 4 + 0] = palette[e + 0];
                rgba[i * 4 + 1] = palette[e + 1];
                rgba[i * 4 + 2] = palette[e + 2];
            }
        }
    } else if (format == 24) {
        if (data.size() < n * 3) return false;
        for (size_t i = 0; i < n; ++i) {
            rgba[i * 4 + 0] = data[i * 3 + 2];
            rgba[i * 4 + 1] = data[i * 3 + 1];
            rgba[i * 4 + 2] = data[i * 3 + 0];
        }
    } else {
        return false;
    }
    if (has_alpha) *has_alpha = alpha;
    return true;
}

bool read_car(std::span<const uint8_t> bytes, CarData& out, bool header_only, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    out = CarData{};
    Walker top(bytes.data(), bytes.size());
    Tag file;
    if (!top.next(file) || file.id != tag::CARFILE || file.type != TagType::List) return fail("not a CARFILE");
    Walker fw(file.data, file.size);
    Tag t;
    const Tag* data = nullptr;
    Tag cardata;
    while (fw.next(t)) {
        if (t.id == tag::VERSION) out.version = i32(t);
        else if (t.id == tag::CARDATA) {
            cardata = t;
            data = &cardata;
        }
    }
    if (!data) return fail("no CARDATA");
    Walker cw(data->data, data->size);
    while (cw.next(t)) {
        switch (t.id) {
            case tag::CD_ID: out.id = i32(t); break;
            case tag::CD_NAME: out.name = str(t); break;
            case tag::CD_MINSKILL: out.min_skill = i32(t); break;
            case tag::CD_PARTDATA: read_part(t, out.parts.emplace_back()); break;
            case tag::CD_SOUND_IGNITION: {
                CarIgnitionSound s;
                Walker sw(t.data, t.size);
                Tag u;
                while (sw.next(u)) {
                    if (u.id == tag::IS_PARTID) s.part_id = i32(u);
                    else if (u.id == tag::IS_WAVFILE) s.wav.assign(u.data, u.data + u.size);
                }
                out.ignition.push_back(std::move(s));
                break;
            }
            case tag::CD_TEXTURE:
                if (!header_only && !read_texture(t, out.textures.emplace_back())) return fail("bad texture " + std::to_string(out.textures.size() - 1));
                break;
            case tag::CD_MATERIAL:
                if (!header_only) read_material(t, out.materials.emplace_back());
                break;
            case tag::CD_AUTOMESH:
                if (!header_only && !read_mesh(t, out.meshes.emplace_back())) return fail("bad mesh");
                break;
            case tag::CD_SPECMESH:
                if (!header_only && !read_mesh(t, out.spec_meshes.emplace_back())) return fail("bad spec mesh");
                break;
            default: break;
        }
    }
    if (cw.bad()) return fail("truncated CARDATA");
    out.full = !header_only;
    return true;
}

// The identity and parts only, skipping the meshes and textures (a car's parts come after its
// geometry, so this saves reading most of the file).
std::optional<CarData> load_car_header(const ReadAt& read, uint64_t file_size, std::string* error) {
    auto fail = [&](const char* why) {
        if (error) *error = why;
        return std::optional<CarData>();
    };
    CarData out;
    uint64_t pos = 0;
    auto head = [&](uint32_t& id, TagType& type, uint32_t& size) {
        uint32_t h[2];
        if (read(pos, h, 8) != 8) return false;
        pos += 8;
        id = h[0] & 0x0FFFFFFF;
        type = TagType(h[0] >> 28);
        size = h[1];
        return true;
    };
    uint32_t id, size;
    TagType type;
    if (!head(id, type, size) || id != tag::CARFILE || type != TagType::List) return fail("not a CARFILE");
    const uint64_t file_end = std::min<uint64_t>(8 + uint64_t(size), file_size);
    std::vector<uint8_t> buf;
    while (pos + 8 <= file_end) {
        if (!head(id, type, size)) break;
        const uint64_t body = pos;
        if (body + size > file_end) return fail("truncated");
        if (id == tag::VERSION) {
            int32_t v = 0;
            if (read(body, &v, 4) == 4) out.version = v;
        } else if (id == tag::CARDATA) {
            const uint64_t end = body + size;
            while (pos + 8 <= end) {
                uint32_t cid, csize;
                TagType ctype;
                if (!head(cid, ctype, csize)) break;
                const uint64_t cbody = pos;
                if (cbody + csize > end) return fail("truncated CARDATA");
                pos = cbody + csize;
                if (cid == tag::CD_TEXTURE || cid == tag::CD_MATERIAL || cid == tag::CD_AUTOMESH || cid == tag::CD_SPECMESH) continue;
                buf.resize(csize);
                if (csize && read(cbody, buf.data(), csize) != csize) return fail("read error");
                Tag t{cid, ctype, buf.data(), csize};
                switch (cid) {
                    case tag::CD_ID: out.id = i32(t); break;
                    case tag::CD_NAME: out.name = str(t); break;
                    case tag::CD_MINSKILL: out.min_skill = i32(t); break;
                    case tag::CD_PARTDATA: read_part(t, out.parts.emplace_back()); break;
                    case tag::CD_SOUND_IGNITION: {
                        CarIgnitionSound s;
                        Walker sw(t.data, t.size);
                        Tag u;
                        while (sw.next(u)) {
                            if (u.id == tag::IS_PARTID) s.part_id = i32(u);
                            else if (u.id == tag::IS_WAVFILE) s.wav.assign(u.data, u.data + u.size);
                        }
                        out.ignition.push_back(std::move(s));
                        break;
                    }
                    default: break;
                }
            }
        }
        pos = body + size;
    }
    return out;
}

std::optional<CarData> load_car(const std::filesystem::path& file, bool header_only, std::string* error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open";
        return std::nullopt;
    }
    in.seekg(0, std::ios::end);
    const uint64_t size = uint64_t(in.tellg());
    in.seekg(0);
    if (header_only) {
        const ReadAt read = [&in](uint64_t offset, void* out, size_t bytes) -> size_t {
            in.clear();
            in.seekg(std::streamoff(offset));
            in.read(static_cast<char*>(out), std::streamsize(bytes));
            return size_t(in.gcount());
        };
        return load_car_header(read, size, error);
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (size && !in.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(size))) {
        if (error) *error = "read error";
        return std::nullopt;
    }
    CarData car;
    if (!read_car(bytes, car, false, error)) return std::nullopt;
    return car;
}

}  // namespace ghg::fmt
