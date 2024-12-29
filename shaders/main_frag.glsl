#version 450

layout(set = 0, binding = 0, r8) uniform readonly image2D atlas;

layout(location = 0) in vec4 frag_colour;
layout(location = 1) in vec2 glyph_uv;

layout(location = 0) out vec4 colour;

void main() {
    float alpha = imageLoad(atlas, ivec2(glyph_uv)).r;

    colour = frag_colour * alpha;
}
