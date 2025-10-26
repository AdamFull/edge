#include "fullscreen.h"

import shader_layout;

[[vk::push_constant]] ConstantBuffer<edge::gfx::fullscreen::PushConstant> pc;

struct VSOutput {
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

[shader("vertex")]
VSOutput vs_main(uint sv_vertexID: SV_VertexID) {
    VSOutput output;
    output.tex_coord = float2((sv_vertexID << 1) & 2, sv_vertexID & 2);
    output.position = float4(output.tex_coord * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}

[shader("fragment")]
float4 ps_main(VSOutput input) : SV_Target0 {
    return g_srv_textures[pc.image_id].Sample(g_samplers[0], input.tex_coord);
}