// ============================================================================
// Background Vertex Shader [shader_background_vs.hlsl]
// Fullscreen triangle generation via SV_VertexID
// ============================================================================

struct VS_OUTPUT
{
    float4 posH : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;
    // Generates a triangle covering [-1, -1] to [3, 3] in NDC space
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.posH = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.uv   = uv;
    return output;
}
