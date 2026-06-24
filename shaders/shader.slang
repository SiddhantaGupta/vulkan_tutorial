struct VS_Input {
    float2 in_position;
    float2 in_tex_coord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

Sampler2D texture;

struct VS_Output {
    float4 pos: SV_Position;
    float2 frag_tex_coord;
};

[shader("vertex")]
VS_Output vert_main(VS_Input input) {
    VS_Output output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.in_position, 0.0, 1.0))));
    output.frag_tex_coord = input.in_tex_coord;
    return output;
}

[shader("fragment")]
float4 frag_main(VS_Output vert_in) : SV_Target {
    return texture.Sample(vert_in.frag_tex_coord);
}

