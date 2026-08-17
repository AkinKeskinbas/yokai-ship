#ifndef BACKGROUND_SHADER_H
#define BACKGROUND_SHADER_H

#include <d3d11.h>
#include <DirectXMath.h>

// ============================================================================
// BACKGROUND SHADER WRAPPER [background_shader.h]
// Procedural Retro Psychedelic Cosmic Background Renderer (DirectX 11 HLSL)
// Renders only to background layer; leaves gameplay sprites 100% sharp & unaffected
// ============================================================================

struct BackgroundConstantBuffer
{
    float time;                 // Elapsed time in seconds
    float distortionStrength;   // Base 0.003, Boss 0.006
    float flowSpeed;            // Base 0.25, Boss 0.45
    float pulseStrength;        // Base 0.02, Boss 0.04
    float hueShift;             // Subtle color shift
    float vignetteStrength;     // Edge darkness (0.85)
    float bossIntensity;        // 0.0 (normal) to 1.0 (boss encounter)
    float screenWidth;          // 1600.0f
    float screenHeight;         // 900.0f
    float camOffsetX;           // Camera shake X
    float camOffsetY;           // Camera shake Y
    float padding;              // 16-byte alignment padding
};

class BackgroundRenderer
{
public:
    BackgroundRenderer() = default;
    ~BackgroundRenderer();

    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    void Finalize();

    void Update(float deltaTime, bool isBossActive);
    void Render(float camX = 0.0f, float camY = 0.0f);

    void SetBossIntensity(float value);
    float GetBossIntensity() const { return m_cbData.bossIntensity; }

private:
    ID3D11Device*           m_pDevice = nullptr;
    ID3D11DeviceContext*    m_pContext = nullptr;

    ID3D11VertexShader*     m_pVertexShader = nullptr;
    ID3D11PixelShader*      m_pPixelShader = nullptr;
    ID3D11Buffer*           m_pConstantBuffer = nullptr;
    ID3D11DepthStencilState* m_pDepthStencilState = nullptr;
    ID3D11BlendState*       m_pBlendState = nullptr;

    BackgroundConstantBuffer m_cbData{};
    float                   m_targetBossIntensity = 0.0f;
};

#endif // BACKGROUND_SHADER_H
