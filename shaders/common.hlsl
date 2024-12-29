struct GlyphLoc {
    uint4 location;
};

struct Glyph {
    float2 position;
    uint glyph_idx;
    uint unused;
};

Texture2D<float>           atlas           : register(t0, space0);
StructuredBuffer<GlyphLoc> glyph_locations : register(b1, space0);
StructuredBuffer<Glyph>    glyphs_to_draw  : register(b2, space0);

struct FragInput {
    float4 position: SV_POSITION;
    float4 colour: COLOR0;
    float2 uv;
    uint glyph_idx;
};
