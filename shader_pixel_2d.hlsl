struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

cbuffer ColorBuffer : register(b0)
{
    float4 color;
};

Texture2D major_texture : register(t0);
SamplerState major_sampler : register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
    return major_texture.Sample(major_sampler, input.uv) * input.color * color;
}