#include "common.hlsl"

//struct GlyphLookup {
//    uint x;
//    uint y;
//    uint w;
//    uint h;
//};
//
//struct Glyph {
//    [[vk::location(0)]] uint glyph_idx;
//    [[vk::location(1)]] vec2 position;
//    [[vk::location(0)]] uint unused;
//}

float4 main(FragInput ip) : SV_TARGET {
    return ip.colour;
}
