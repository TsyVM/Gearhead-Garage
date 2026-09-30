#include "Formats/TagFile.hpp"

#include <cstring>
#include <fstream>

namespace ghg::fmt {

namespace {

const char* const kGroups[][15] = {
    {"VERSION", "CARFILE", "DECALFILE", "MECHANICFILE", "JOBFILE", "UPDATEFILE", "EVERBLAZEPLAYERFILE"},
    {"CARDATA", "DECALDATA", "MECHANIC", "JOBDATA", "JOBPHASEIMAGE", "UPDATEDATA", "EVERBLAZEPLAYERDATA"},
    {"CD_ID", "CD_NAME", "CD_TEXTURE", "CD_MATERIAL", "CD_AUTOMESH", "CD_SPECMESH", "CD_PARTDATA", "CD_SOUND_NOSTARTER",
     "CD_SOUND_FALSESTART", "CD_SOUND_IGNITION", "CD_MINSKILL"},
    {"PD_ID", "PD_NAME", "PD_CUSTOM", "PD_REGION", "PD_MESHNAME", "PD_COSTMIN", "PD_COSTMAX", "PD_ATTACHDEP", "PD_REMOVEDEP",
     "PD_MUTEXCSET", "PD_AMEA", "PD_VIC", "PD_SPECIAL", "PD_BOLT", "PD_MULTIMAT"},
    {"TEX_NAME", "TEX_FLAGS", "TEX_BITMAP"},
    {"MMAT_MATERIALINDEX", "MMAT_SWITCHPOLYLIST"},
    {"MAT_NAME", "MAT_COLOR", "MAT_BLENDSRC", "MAT_BLENDDST", "MAT_SHADEALPHA", "MAT_SHADECOLOR", "MAT_ALPHA", "MAT_FLAGS",
     "MAT_TEXINDEX1", "MAT_TEXINDEX2"},
    {"MESH_NAME", "MESH_INDEX", "MESH_LOC", "MESH_MATRIXX", "MESH_MATRIXY", "MESH_MATRIXZ", "MESH_NUMPOLYS", "MESH_NUMVERTS",
     "MESH_VERT", "MESH_POLY"},
    {"POLY_VERTINDEX1", "POLY_VERTINDEX2", "POLY_VERTINDEX3", "POLY_MAP1", "POLY_MAP2", "POLY_MAP3", "POLY_MATINDEX"},
    {"VERT_LOC", "VERT_NORMAL"},
    {"BITMAP_WIDTH", "BITMAP_HEIGHT", "BITMAP_FORMAT", "BITMAP_DATA", "BITMAP_PALETTE"},
    {"DD_ID", "DD_NAME", "DD_COST", "DD_COLORTYPE", "DD_NUMPERBUY", "DD_IMAGE"},
    {"PART_ID", "PART_AUTOID", "PART_CONDITION", "PART_VALUEWIGGLE"},
    {"PBANK_AUTOID", "PBANK_INBINPART", "PBANK_INYARDPART"},
    {"MECH_ID", "MECH_NAME", "MECH_SKILLINDEX", "MECH_CASH", "MECH_TOTALCAREERAUTOS", "MECH_TOTALWORKTIME",
     "MECH_NUMCURAUTOS", "MECH_PARTSBANK", "MECH_AUTO", "MECH_DECAL", "MECH_LASTAUTONUM", "MECH_COMPLETEDJOBS",
     "MECH_AUTOJOBREQUEST"},
    {"CAR_ID", "CAR_ISTOP", "CAR_REPAIRTIME", "CAR_REPAIRCOST", "CAR_PURCHASECOST", "CAR_CAREERNUM", "CAR_PART",
     "CAR_PAINTONTEX"},
    {"DECAL_ID", "DECAL_COPIES"},
    {"JOB_ID", "JOB_CARID", "JOB_ANXIETY", "JOB_FEE", "JOB_CARCOLOR", "JOB_CARBITMAP", "JOB_LEGALMODES", "JOB_PHASE",
     "JOB_STARTSTAT", "JOB_COMPLETESTAT", "JOB_ALLOWCUSTOMPARTS", "JOB_ILLEGALPART"},
    {"JP_TYPE", "JP_TEXT", "JP_IMAGEINDEX"},
    {"JS_TYPE", "JS_SUBJECT", "JS_CONDITION"},
    {"FL_IR", "FL_NAME", "FL_DATA"},
    {"IS_PARTID", "IS_WAVFILE"},
};

bool parse(const uint8_t* p, size_t size, std::vector<TagNode>& out, int depth, std::string* error) {
    if (depth > 32) {
        if (error) *error = "tags nested too deep";
        return false;
    }
    size_t at = 0;
    while (at + 8 <= size) {
        uint32_t head, len;
        std::memcpy(&head, p + at, 4);
        std::memcpy(&len, p + at + 4, 4);
        at += 8;
        if (len > size - at) {
            if (error) *error = "tag " + tag_name(head & 0x0FFFFFFF) + " runs past its parent";
            return false;
        }
        TagNode n;
        n.id = head & 0x0FFFFFFF;
        n.type = TagType(head >> 28);
        if (n.type == TagType::List) {
            if (!parse(p + at, len, n.kids, depth + 1, error)) return false;
        } else {
            n.data.assign(p + at, p + at + len);
        }
        at += len;
        out.push_back(std::move(n));
    }
    return true;
}

void write(const TagNode& n, std::vector<uint8_t>& out) {
    const uint32_t head = (uint32_t(n.type) << 28) | (n.id & 0x0FFFFFFF);
    const size_t at = out.size();
    out.resize(at + 8);
    std::memcpy(out.data() + at, &head, 4);
    if (n.type == TagType::List) {
        for (const TagNode& k : n.kids) write(k, out);
    } else {
        out.insert(out.end(), n.data.begin(), n.data.end());
    }
    const uint32_t len = uint32_t(out.size() - at - 8);
    std::memcpy(out.data() + at + 4, &len, 4);
}

}  // namespace

std::string tag_name(uint32_t id) {
    const uint32_t g = id / 100, i = id % 100;
    if (g < std::size(kGroups) && i < 15 && kGroups[g][i]) return kGroups[g][i];
    return std::to_string(id);
}

const TagNode* TagNode::find(uint32_t tag) const {
    for (const TagNode& k : kids)
        if (k.id == tag) return &k;
    return nullptr;
}

std::vector<const TagNode*> TagNode::all(uint32_t tag) const {
    std::vector<const TagNode*> r;
    for (const TagNode& k : kids)
        if (k.id == tag) r.push_back(&k);
    return r;
}

int TagNode::as_int(int fallback) const {
    if (data.size() < 4) return fallback;
    int32_t v;
    std::memcpy(&v, data.data(), 4);
    return v;
}

float TagNode::as_float(float fallback) const {
    if (data.size() < 4) return fallback;
    float v;
    std::memcpy(&v, data.data(), 4);
    return v;
}

std::string TagNode::as_string() const {
    std::string s(data.begin(), data.end());
    if (const auto z = s.find('\0'); z != std::string::npos) s.resize(z);
    return s;
}

std::vector<int32_t> TagNode::as_ints() const {
    std::vector<int32_t> v(data.size() / 4);
    if (!v.empty()) std::memcpy(v.data(), data.data(), v.size() * 4);
    return v;
}

std::vector<float> TagNode::as_floats() const {
    std::vector<float> v(data.size() / 4);
    if (!v.empty()) std::memcpy(v.data(), data.data(), v.size() * 4);
    return v;
}

int TagNode::get_int(uint32_t tag, int fallback) const {
    const TagNode* k = find(tag);
    return k ? k->as_int(fallback) : fallback;
}
float TagNode::get_float(uint32_t tag, float fallback) const {
    const TagNode* k = find(tag);
    return k ? k->as_float(fallback) : fallback;
}
std::string TagNode::get_string(uint32_t tag, std::string_view fallback) const {
    const TagNode* k = find(tag);
    return k ? k->as_string() : std::string(fallback);
}
std::vector<int32_t> TagNode::get_ints(uint32_t tag) const {
    const TagNode* k = find(tag);
    return k ? k->as_ints() : std::vector<int32_t>{};
}
std::vector<float> TagNode::get_floats(uint32_t tag) const {
    const TagNode* k = find(tag);
    return k ? k->as_floats() : std::vector<float>{};
}
std::span<const uint8_t> TagNode::get_raw(uint32_t tag) const {
    const TagNode* k = find(tag);
    return k ? std::span<const uint8_t>(k->data) : std::span<const uint8_t>{};
}

TagNode& TagNode::add_list(uint32_t tag) {
    TagNode& n = kids.emplace_back();
    n.id = tag;
    n.type = TagType::List;
    return n;
}
void TagNode::add_int(uint32_t tag, int v) {
    TagNode& n = kids.emplace_back();
    n.id = tag;
    n.type = TagType::Int;
    n.data.resize(4);
    std::memcpy(n.data.data(), &v, 4);
}
void TagNode::add_float(uint32_t tag, float v) {
    TagNode& n = kids.emplace_back();
    n.id = tag;
    n.type = TagType::Float;
    n.data.resize(4);
    std::memcpy(n.data.data(), &v, 4);
}
void TagNode::add_string(uint32_t tag, std::string_view s) {
    TagNode& n = kids.emplace_back();
    n.id = tag;
    n.type = TagType::String;
    n.data.assign(s.begin(), s.end());
}
void TagNode::add_raw(uint32_t tag, std::span<const uint8_t> bytes) {
    TagNode& n = kids.emplace_back();
    n.id = tag;
    n.type = TagType::Raw;
    n.data.assign(bytes.begin(), bytes.end());
}
void TagNode::add_ints(uint32_t tag, std::span<const int32_t> v) {
    add_raw(tag, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(v.data()), v.size() * 4));
}
void TagNode::add_floats(uint32_t tag, std::span<const float> v) {
    add_raw(tag, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(v.data()), v.size() * 4));
}

bool parse_tags(std::span<const uint8_t> bytes, std::vector<TagNode>& out, std::string* error) {
    out.clear();
    return parse(bytes.data(), bytes.size(), out, 0, error);
}

std::optional<std::vector<TagNode>> load_tags(const std::filesystem::path& file, std::string* error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open";
        return std::nullopt;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<TagNode> nodes;
    if (!parse_tags(bytes, nodes, error)) return std::nullopt;
    return nodes;
}

std::vector<uint8_t> write_tags(std::span<const TagNode> nodes) {
    std::vector<uint8_t> out;
    for (const TagNode& n : nodes) write(n, out);
    return out;
}

}  // namespace ghg::fmt
