// The tagged binary files of Gearhead Garage (.car .dpk .jpk .mek): `u32 (type << 28 | id)`,
// `u32 size`, then the payload; a container's payload is more tags. Ids are `group * 100 + index`
// in ghg.exe's tag-name table (Docs/Research.md, "Tagged files").
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ghg::fmt {

enum class TagType : uint8_t { Raw = 0, Int = 2, Float = 4, String = 6, List = 8 };

namespace tag {
// Group 0/1: files and their top records.
constexpr uint32_t VERSION = 0, CARFILE = 1, DECALFILE = 2, MECHANICFILE = 3, JOBFILE = 4, UPDATEFILE = 5;
constexpr uint32_t CARDATA = 100, DECALDATA = 101, MECHANIC = 102, JOBDATA = 103, JOBPHASEIMAGE = 104, UPDATEDATA = 105;
// Cars.
constexpr uint32_t CD_ID = 200, CD_NAME = 201, CD_TEXTURE = 202, CD_MATERIAL = 203, CD_AUTOMESH = 204, CD_SPECMESH = 205,
                   CD_PARTDATA = 206, CD_SOUND_NOSTARTER = 207, CD_SOUND_FALSESTART = 208, CD_SOUND_IGNITION = 209,
                   CD_MINSKILL = 210;
constexpr uint32_t PD_ID = 300, PD_NAME = 301, PD_CUSTOM = 302, PD_REGION = 303, PD_MESHNAME = 304, PD_COSTMIN = 305,
                   PD_COSTMAX = 306, PD_ATTACHDEP = 307, PD_REMOVEDEP = 308, PD_MUTEXCSET = 309, PD_AMEA = 310, PD_VIC = 311,
                   PD_SPECIAL = 312, PD_BOLT = 313, PD_MULTIMAT = 314;
constexpr uint32_t TEX_NAME = 400, TEX_FLAGS = 401, TEX_BITMAP = 402;
constexpr uint32_t MMAT_MATERIALINDEX = 500, MMAT_SWITCHPOLYLIST = 501;
constexpr uint32_t MAT_NAME = 600, MAT_COLOR = 601, MAT_BLENDSRC = 602, MAT_BLENDDST = 603, MAT_SHADEALPHA = 604,
                   MAT_SHADECOLOR = 605, MAT_ALPHA = 606, MAT_FLAGS = 607, MAT_TEXINDEX1 = 608, MAT_TEXINDEX2 = 609;
constexpr uint32_t MESH_NAME = 700, MESH_INDEX = 701, MESH_LOC = 702, MESH_MATRIXX = 703, MESH_MATRIXY = 704,
                   MESH_MATRIXZ = 705, MESH_NUMPOLYS = 706, MESH_NUMVERTS = 707, MESH_VERT = 708, MESH_POLY = 709;
constexpr uint32_t POLY_VERTINDEX1 = 800, POLY_VERTINDEX2 = 801, POLY_VERTINDEX3 = 802, POLY_MAP1 = 803, POLY_MAP2 = 804,
                   POLY_MAP3 = 805, POLY_MATINDEX = 806;
constexpr uint32_t VERT_LOC = 900, VERT_NORMAL = 901;
constexpr uint32_t BITMAP_WIDTH = 1000, BITMAP_HEIGHT = 1001, BITMAP_FORMAT = 1002, BITMAP_DATA = 1003, BITMAP_PALETTE = 1004;
// Decals.
constexpr uint32_t DD_ID = 1100, DD_NAME = 1101, DD_COST = 1102, DD_COLORTYPE = 1103, DD_NUMPERBUY = 1104, DD_IMAGE = 1105;
// Mechanics (saves).
constexpr uint32_t PART_ID = 1200, PART_AUTOID = 1201, PART_CONDITION = 1202, PART_VALUEWIGGLE = 1203;
constexpr uint32_t PBANK_AUTOID = 1300, PBANK_INBINPART = 1301, PBANK_INYARDPART = 1302;
constexpr uint32_t MECH_ID = 1400, MECH_NAME = 1401, MECH_SKILLINDEX = 1402, MECH_CASH = 1403, MECH_TOTALCAREERAUTOS = 1404,
                   MECH_TOTALWORKTIME = 1405, MECH_NUMCURAUTOS = 1406, MECH_PARTSBANK = 1407, MECH_AUTO = 1408,
                   MECH_DECAL = 1409, MECH_LASTAUTONUM = 1410, MECH_COMPLETEDJOBS = 1411, MECH_AUTOJOBREQUEST = 1412;
constexpr uint32_t CAR_ID = 1500, CAR_ISTOP = 1501, CAR_REPAIRTIME = 1502, CAR_REPAIRCOST = 1503, CAR_PURCHASECOST = 1504,
                   CAR_CAREERNUM = 1505, CAR_PART = 1506, CAR_PAINTONTEX = 1507;
constexpr uint32_t DECAL_ID = 1600, DECAL_COPIES = 1601;
// Jobs.
constexpr uint32_t JOB_ID = 1700, JOB_CARID = 1701, JOB_ANXIETY = 1702, JOB_FEE = 1703, JOB_CARCOLOR = 1704,
                   JOB_CARBITMAP = 1705, JOB_LEGALMODES = 1706, JOB_PHASE = 1707, JOB_STARTSTAT = 1708,
                   JOB_COMPLETESTAT = 1709, JOB_ALLOWCUSTOMPARTS = 1710, JOB_ILLEGALPART = 1711;
constexpr uint32_t JP_TYPE = 1800, JP_TEXT = 1801, JP_IMAGEINDEX = 1802;
constexpr uint32_t JS_TYPE = 1900, JS_SUBJECT = 1901, JS_CONDITION = 1902;
constexpr uint32_t FL_IR = 2000, FL_NAME = 2001, FL_DATA = 2002;
constexpr uint32_t IS_PARTID = 2100, IS_WAVFILE = 2101;
}  // namespace tag

// The name ghg.exe gives a tag id ("CD_NAME"), or its number.
std::string tag_name(uint32_t id);

struct TagNode {
    uint32_t id = 0;
    TagType type = TagType::Raw;
    std::vector<uint8_t> data;     // leaf payload
    std::vector<TagNode> kids;     // a list's contents

    const TagNode* find(uint32_t tag) const;
    std::vector<const TagNode*> all(uint32_t tag) const;
    int as_int(int fallback = 0) const;
    float as_float(float fallback = 0) const;
    std::string as_string() const;
    std::vector<int32_t> as_ints() const;
    std::vector<float> as_floats() const;

    // A child's value (fallback when the child is missing).
    int get_int(uint32_t tag, int fallback = 0) const;
    float get_float(uint32_t tag, float fallback = 0) const;
    std::string get_string(uint32_t tag, std::string_view fallback = {}) const;
    std::vector<int32_t> get_ints(uint32_t tag) const;
    std::vector<float> get_floats(uint32_t tag) const;
    std::span<const uint8_t> get_raw(uint32_t tag) const;

    // Building (writing saves).
    TagNode& add_list(uint32_t tag);
    void add_int(uint32_t tag, int v);
    void add_float(uint32_t tag, float v);
    void add_string(uint32_t tag, std::string_view s);
    void add_raw(uint32_t tag, std::span<const uint8_t> bytes);
    void add_ints(uint32_t tag, std::span<const int32_t> v);
    void add_floats(uint32_t tag, std::span<const float> v);
};

// Parses a whole file into its top-level tags. False (with a reason) on a malformed tree.
bool parse_tags(std::span<const uint8_t> bytes, std::vector<TagNode>& out, std::string* error = nullptr);
std::optional<std::vector<TagNode>> load_tags(const std::filesystem::path& file, std::string* error = nullptr);
std::vector<uint8_t> write_tags(std::span<const TagNode> nodes);

}  // namespace ghg::fmt
