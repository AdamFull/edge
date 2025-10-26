[[vk::binding(0, 0)]] SamplerState g_samplers[65535] : register(s0, space0);
[[vk::binding(1, 0)]] Texture2D g_srv_textures[65535] : register(t0, space0);
[[vk::binding(2, 0)]] WTexture2D<float4> g_uav_textures[65535] : register(u0, space0);