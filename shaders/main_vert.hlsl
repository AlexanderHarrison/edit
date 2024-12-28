#include "common.hlsl"

static const float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
};

static const float4 colours[3] = {
    float4(1.0, 0.0, 0.0, 1.0),
    float4(0.0, 1.0, 0.0, 1.0),
    float4(0.0, 0.0, 1.0, 1.0)
};

FragInput main(uint vert_id: SV_VERTEXID) {
    FragInput output;
    output.position = float4(positions[vert_id], 0.0, 1.0);
    output.colour = colours[vert_id];
    return output;
}
