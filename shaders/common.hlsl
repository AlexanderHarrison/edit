struct GlyphLoc {
    uvec2 offset;
    uvec2 size;
};

struct Glyph {
    vec2 position;
    uint glyph_idx;
    uint unused;
};

layout(set = 0, binding = 0, r8) uniform readonly image2D inputImage;
layout(set = 0, binding = 1) buffer glyph_locations { GlyphLoc locations[]; };
layout(set = 0, binding = 2) buffer glyphs_to_draw { Glyph glyphs[]; };

struct VertOutput {
    float4 position: SV_POSITION;
    float4 colour: COLOR0;
    float2 uv;
    uint glyph_idx;
};

struct FragInput {
    float4 position: SV_POSITION;
    float4 colour: COLOR0;
    float2 uv;
    uint glyph_idx;
};
