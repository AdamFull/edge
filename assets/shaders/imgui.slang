#include "imgui.h"

struct VSOutput {
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 position : SV_Position;
};

[[vk::push_constant]] ConstantBuffer<edge::gfx::imgui::PushConstant> pc;

[shader("vertex")]
VSOutput vs_main(uint32_t vertex_index: SV_VertexID) {
    edge::gfx::imgui::Vertex vert = pc.vertices[vertex_index];

    VSOutput output;
    output.uv = vert.uv;
    output.color = edge::gfx::decode_color(vert.col);
    output.position = float4(vert.pos * pc.scale + pc.translate, 0.0, 1.0);

    return output;
}

[shader("fragment")]
float4 ps_main(VSOutput input) : SV_Target0 {
    //return input.color * texture0.Sample(sampler0, input.uv);
    return input.color;
}