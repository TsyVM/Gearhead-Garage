// The 3D views: the cars on their stands and the places (the auction yard, the car lot, the
// junkyard areas). One directional light and ambient, as the original's 3DGE lit its scenes;
// materials smooth or flat (MAT_SHADECOLOR 1 = one normal per face), textured or plain, opaque,
// blended or added; the shine pass (CD_SPECMESH) adds a highlight over the painted body.
cbuffer Frame : register(b0) {
    row_major float4x4 ViewProj;
    float4 CameraPos;     // xyz
    float4 LightDir;      // xyz: the way the light travels
    float4 LightColour;
    float4 Ambient;
};
cbuffer Object : register(b1) {
    row_major float4x4 World;
    float4 Colour;        // the material's colour (rgb) and alpha
    float4 Params;        // x: 1 textured, y: 1 flat, z: 1 shine pass, w: alpha test (<0 off)
    float4 Extra;         // x: 1 unlit, y: shine power, z: condition tint strength, w: unused
    float4 Tint;          // Show Condition's colour (rgb)
};
Texture2D Tex : register(t0);
SamplerState Samp : register(s0);

struct VSIn {
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};
struct VSOut {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float3 wpos : TEXCOORD1;
    float3 nrm : TEXCOORD2;
};

VSOut vs_scene(VSIn i) {
    VSOut o;
    float4 wp = mul(float4(i.pos, 1), World);
    o.pos = mul(wp, ViewProj);
    o.wpos = wp.xyz;
    o.nrm = mul(float4(i.nrm, 0), World).xyz;
    o.uv = i.uv;
    return o;
}

float4 ps_scene(VSOut i) : SV_Target0 {
    float3 n = normalize(i.nrm);
    if (Params.y > 0.5) {
        float3 fn = cross(ddy(i.wpos), ddx(i.wpos));
        if (dot(fn, fn) > 1e-12) n = normalize(fn);
    }
    float3 v = normalize(CameraPos.xyz - i.wpos);
    if (dot(n, v) < 0) n = -n;   // the cars' polygons are drawn from both sides
    float3 l = -normalize(LightDir.xyz);
    float4 base = Colour;
    if (Params.x > 0.5) base *= Tex.Sample(Samp, i.uv);
    if (Params.w >= 0 && base.a <= Params.w) discard;
    if (Params.z > 0.5) {
        // The shine: a highlight added over the body, no colour of its own.
        float3 h = normalize(l + v);
        float s = pow(saturate(dot(n, h)), max(Extra.y, 1.0));
        return float4(LightColour.rgb * s * Colour.rgb * Colour.a, 1);
    }
    float3 lit = Extra.x > 0.5 ? float3(1, 1, 1) : Ambient.rgb + LightColour.rgb * saturate(dot(n, l));
    float3 c = base.rgb * lit;
    c = lerp(c, Tint.rgb * (0.35 + 0.65 * saturate(dot(n, l) + 0.3)), Extra.z);
    return float4(c, base.a);
}

// A picture drawn over the whole target (the auction's sky behind the yard).
struct VQuad {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
VQuad vs_backdrop(uint id : SV_VertexID) {
    VQuad o;
    float2 p = float2((id << 1) & 2, id & 2);
    o.uv = p;
    o.pos = float4(p * float2(2, -2) + float2(-1, 1), 1, 1);
    return o;
}
float4 ps_backdrop(VQuad i) : SV_Target0 { return Tex.Sample(Samp, i.uv) * Colour; }
