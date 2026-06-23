struct VS_Input {
    float2 in_position;
    float3 in_color;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

struct VS_Output {
    float4 pos: SV_Position;
    float3 color;
};

[shader("vertex")]
VS_Output vert_main(VS_Input input) {
    VS_Output output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.in_position, 0.0, 1.0))));
    output.color = input.in_color;
    return output;
}

[shader("fragment")]
float4 frag_main(VS_Output vert_in) : SV_Target {
    return float4(vert_in.color, 1.0);
}
