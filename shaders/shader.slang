struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float delta_time;
};

struct Particle {
	float2 position;
	float2 velocity;
    float4 color;
};

struct ParticleSSBO {
    Particle particles;
};

[[vk::binding(0)]]
ConstantBuffer<UniformBuffer> ubo;
[[vk::binding(1)]]
Sampler2D texture;
[[vk::binding(2)]]
StructuredBuffer<ParticleSSBO> particles_in;
[[vk::binding(3)]]
RWStructuredBuffer<ParticleSSBO> particles_out;

struct VS_Input {
    float2 in_position;
    float4 in_color;
};

struct VS_Output {
    float4 pos: SV_Position;
    float4 color;
};

[shader("compute")]
[numthreads(256,1,1)]
void comp_main(uint3 thread_id : SV_DispatchThreadID)
{
    uint index = thread_id.x;

    particles_out[index].particles.position = particles_in[index].particles.position +
        particles_in[index].particles.velocity.xy * ubo.delta_time;
    particles_out[index].particles.velocity = particles_in[index].particles.velocity;

    if ((particles_out[index].particles.position.x <= -1.0) || (particles_out[index].particles.position.x >= 1.0)) {
        particles_out[index].particles.velocity.x = -particles_out[index].particles.velocity.x;
    }
    if ((particles_out[index].particles.position.y <= -1.0) || (particles_out[index].particles.position.y >= 1.0)) {
        particles_out[index].particles.velocity.y = -particles_out[index].particles.velocity.y;
    }
}

[shader("vertex")]
VS_Output vert_main(VS_Input input, out float point_size: SV_PointSize) {
    VS_Output output;
    output.pos = float4(input.in_position, 0.0, 1.0);
    output.color = input.in_color;

    point_size = 2.0;

    return output;
}

[shader("fragment")]
float4 frag_main(VS_Output vert_in) : SV_Target {
    return vert_in.color;
}

