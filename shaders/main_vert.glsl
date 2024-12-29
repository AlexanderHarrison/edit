#version 450

vec4 colours[4] = vec4[](
    vec4(1.0, 0.0, 0.0, 1.0),
    vec4(0.0, 1.0, 0.0, 1.0),
    vec4(0.0, 0.0, 1.0, 1.0),
    vec4(0.0, 0.7, 0.7, 1.0)
);

struct GlyphLoc {
    uvec2 offset;
    uvec2 size;
};

struct Glyph {
    vec2 position;
    uint glyph_idx;
    uint unused;
};

layout(set = 0, binding = 1) readonly buffer glyph_locations_SSBO { GlyphLoc glyph_locations[]; };
layout(set = 0, binding = 2) readonly buffer glyphs_to_draw_SSBO { Glyph glyphs_to_draw[]; };

layout(location = 0) out vec4 frag_colour;
layout(location = 1) out vec2 glyph_uv;

void main() {
    Glyph glyph = glyphs_to_draw[gl_InstanceIndex];
    GlyphLoc glyph_loc = glyph_locations[glyph.glyph_idx];

    vec2 uv = vec2((~(gl_VertexIndex >> 1) & 1), gl_VertexIndex & 1);
    glyph_uv = uv * vec2(glyph_loc.size) + vec2(glyph_loc.offset);

    vec2 dims = vec2(glyph_loc.size) / 800.0;
    gl_Position = vec4(uv * dims + glyph.position, 0.0, 1.0);
    frag_colour = colours[gl_VertexIndex];
}
