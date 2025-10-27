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

//[shader("fragment")]
//float4 ps_main(VSOutput input) : SV_Target0 {
//    return g_srv_textures[pc.image_id].Sample(g_samplers[0], input.tex_coord);
//}

// Fit screen shader (for test)
[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target0 {
    uint tex_width, tex_height;
    Texture2D texture = g_srv_textures[pc.image_id];
    texture.GetDimensions(tex_width, tex_height);

    float screen_aspect = float(pc.width) / float(pc.height);
    float texture_aspect = float(tex_width) / float(tex_height);

    float2 scale;
    if (texture_aspect > screen_aspect) {
        scale = float2(1.0f, screen_aspect / texture_aspect);
    } else {
        scale = float2(texture_aspect / screen_aspect, 1.0f);
    }

    float2 uv = (input.tex_coord - 0.5f) / scale + 0.5f;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    return texture.Sample(g_samplers[0], uv);
}