#version 450

layout(set = 0, binding = 0, r8) uniform readonly image2D atlas;

layout(location = 0) in vec4 frag_colour;
layout(location = 1) in vec2 glyph_uv;

layout(location = 0) out vec4 colour;

void main() {
    float alpha;
    if (glyph_uv != vec2(0.0)) {
        alpha = imageLoad(atlas, ivec2(glyph_uv)).r;
    } else {
        alpha = 1.0;
    }

    colour = frag_colour * alpha;
}
