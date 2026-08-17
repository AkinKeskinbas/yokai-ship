cbuffer MatrixBuffer : register(b0)
{
    float4x4 mtx;
};

cbuffer UVMatrixBuffer : register(b1)
{
    float4x4 mtxUV;
};

struct VS_INPUT
{
    float4 posL : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 posH : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.posH = mul(input.posL, mtx);
    output.color = input.color;
    output.uv = mul(float4(input.uv, 0.0f, 1.0f), mtxUV).xy;
    return output;
}