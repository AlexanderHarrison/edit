#version 450

struct GlyphLoc {
    uvec2 offset;
    uvec2 size;
};

struct Glyph {
    vec2 position;
    uint glyph_idx;
    uint colour;
};

layout(set = 0, binding = 1) readonly buffer glyph_locations_SSBO { GlyphLoc glyph_locations[]; };
layout(set = 0, binding = 2) readonly buffer glyphs_to_draw_SSBO { Glyph glyphs_to_draw[]; };

layout(set = 0, binding = 3) readonly uniform StaticData {
    vec2 viewport_size;
} static_data;

layout(location = 0) out vec4 frag_colour;
layout(location = 1) out vec2 glyph_uv;

void main() {
    Glyph glyph = glyphs_to_draw[gl_InstanceIndex];
    uint special = glyph.glyph_idx >> 24;
    vec2 uv = vec2((~(gl_VertexIndex >> 1) & 1), gl_VertexIndex & 1);

    vec2 dims;
    if (special == 0) {
        GlyphLoc glyph_loc = glyph_locations[glyph.glyph_idx];
        glyph_uv = uv * vec2(glyph_loc.size) + vec2(glyph_loc.offset);
        dims = vec2(glyph_loc.size);
    } else if (special == 1) {
        glyph_uv = vec2(0);
        dims = vec2(
            float((glyph.glyph_idx >> 12) & 0xFFFu),
            float(glyph.glyph_idx & 0xFFFu)
        );
    }
    vec2 glyph_position = glyph.position;
    gl_Position = vec4((uv * dims + glyph_position) / static_data.viewport_size * 2.0 - 1.0, 0.0, 1.0);

    uint colour_packed = glyph.colour;
    frag_colour = vec4(
        float(colour_packed & 0xFFu),         // R
        float((colour_packed >> 8) & 0xFFu),  // G
        float((colour_packed >> 16) & 0xFFu), // B
        float((colour_packed >> 24) & 0xFFu)  // A
    ) / 255.0;
}
