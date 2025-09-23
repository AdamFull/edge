struct VSOutput {
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

[shader("vertex")]
VSOutput vs_main(uint sv_vertexID: SV_VertexID) {
    VSOutput output;

    // Calculate texture coordinates (UVs) using bitwise operations on the vertex ID.
    // Slang allows bitwise operations directly on uint.
    // This logic is identical to the GLSL version.
    output.tex_coord = float2((sv_vertexID << 1) & 2, sv_vertexID & 2);

    // Calculate the clip-space position using the generated UVs.
    // The semantic SV_Position replaces gl_Position.
    // The logic remains the same, mapping UVs to a large triangle covering the screen.
    output.position = float4(output.tex_coord * 2.0f - 1.0f, 0.0f, 1.0f);

    return output;
}

struct PSOutput {
    float4 out_color : SV_TARGET0;
};

[shader("fragment")]
float4 ps_main(VSOutput input) : SV_TARGET0 {
    return float4(input.tex_coord, 0.0f, 1.0f);
}