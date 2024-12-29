#include "common.hlsl"

static const float4 colours[4] = {
    float4(1.0, 0.0, 0.0, 1.0),
    float4(0.0, 1.0, 0.0, 1.0),
    float4(0.0, 0.0, 1.0, 1.0),
    float4(0.0, 0.7, 0.7, 1.0)
};

FragInput main(uint vert_id: SV_VERTEXID, uint inst_id: SV_INSTANCEID) {
    FragInput output;

    float2 uv = float2((!(vert_id >> 1) & 1), vert_id & 1);
    Glyph glyph = glyphs_to_draw.Load(inst_id);
    GlyphLoc glyph_loc = glyph_locations.Load(glyph.glyph_idx);

    // TODO: pass viewport width rather than hardcoding
    //float2 dims = float2(glyph_loc.location.zw) / 800.0;
    //float2 pos = uv * dims + glyph.position;
    float2 pos = uv;

    output.position = float4(pos * 2.0 - 1.0, 0.0, 1.0);
    output.colour = colours[vert_id];
    output.uv = uv;
    output.glyph_idx = glyph.glyph_idx;
    return output;
}
