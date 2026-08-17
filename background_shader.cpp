#include "background_shader.h"
#include "direct3d.h"
#include "debug_ostream.h"
#include "configuration.h"
#include <fstream>
#include <vector>
#include <algorithm>

// ============================================================================
// BACKGROUND SHADER WRAPPER [background_shader.cpp]
// ============================================================================

BackgroundRenderer::~BackgroundRenderer()
{
    Finalize();
}

bool BackgroundRenderer::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    if (!pDevice || !pContext) return false;

    m_pDevice = pDevice;
    m_pContext = pContext;

    // 1. Load Vertex Shader CSO
    std::ifstream ifs_vs("asset/shader/shader_background_vs.cso", std::ios::binary);
    if (!ifs_vs)
    {
        MessageBox(nullptr, "Background VS load failed:\nasset/shader/shader_background_vs.cso", "Error", MB_OK);
        return false;
    }
    ifs_vs.seekg(0, std::ios::end);
    size_t vsSize = (size_t)ifs_vs.tellg();
    ifs_vs.seekg(0, std::ios::beg);
    std::vector<char> vsBytecode(vsSize);
    ifs_vs.read(vsBytecode.data(), vsSize);
    ifs_vs.close();

    HRESULT hr = m_pDevice->CreateVertexShader(vsBytecode.data(), vsSize, nullptr, &m_pVertexShader);
    if (FAILED(hr))
    {
        hal::dout << "BackgroundRenderer: CreateVertexShader failed!" << std::endl;
        return false;
    }

    // 2. Load Pixel Shader CSO
    std::ifstream ifs_ps("asset/shader/shader_background_ps.cso", std::ios::binary);
    if (!ifs_ps)
    {
        MessageBox(nullptr, "Background PS load failed:\nasset/shader/shader_background_ps.cso", "Error", MB_OK);
        return false;
    }
    ifs_ps.seekg(0, std::ios::end);
    size_t psSize = (size_t)ifs_ps.tellg();
    ifs_ps.seekg(0, std::ios::beg);
    std::vector<char> psBytecode(psSize);
    ifs_ps.read(psBytecode.data(), psSize);
    ifs_ps.close();

    hr = m_pDevice->CreatePixelShader(psBytecode.data(), psSize, nullptr, &m_pPixelShader);
    if (FAILED(hr))
    {
        hal::dout << "BackgroundRenderer: CreatePixelShader failed!" << std::endl;
        return false;
    }

    // 3. Create Constant Buffer
    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(BackgroundConstantBuffer);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = 0;

    hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pConstantBuffer);
    if (FAILED(hr))
    {
        hal::dout << "BackgroundRenderer: CreateBuffer for ConstantBuffer failed!" << std::endl;
        return false;
    }

    // 4. Create Depth-Stencil State (Depth disabled for background pass)
    D3D11_DEPTH_STENCIL_DESC dsDesc{};
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    m_pDevice->CreateDepthStencilState(&dsDesc, &m_pDepthStencilState);

    // 5. Create Blend State (Opaque for background)
    D3D11_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_pDevice->CreateBlendState(&blendDesc, &m_pBlendState);

    // 6. Set initial shader parameters
    m_cbData.time = 0.0f;
    m_cbData.distortionStrength = 0.003f;
    m_cbData.flowSpeed = 0.25f;
    m_cbData.pulseStrength = 0.02f;
    m_cbData.hueShift = 0.0f;
    m_cbData.vignetteStrength = 0.85f;
    m_cbData.bossIntensity = 0.0f;
    m_cbData.screenWidth = (float)SCREEN_WIDTH;
    m_cbData.screenHeight = (float)SCREEN_HEIGHT;
    m_cbData.camOffsetX = 0.0f;
    m_cbData.camOffsetY = 0.0f;
    m_cbData.padding = 0.0f;

    m_targetBossIntensity = 0.0f;

    return true;
}

void BackgroundRenderer::Finalize()
{
    if (m_pBlendState) { m_pBlendState->Release(); m_pBlendState = nullptr; }
    if (m_pDepthStencilState) { m_pDepthStencilState->Release(); m_pDepthStencilState = nullptr; }
    if (m_pConstantBuffer) { m_pConstantBuffer->Release(); m_pConstantBuffer = nullptr; }
    if (m_pPixelShader) { m_pPixelShader->Release(); m_pPixelShader = nullptr; }
    if (m_pVertexShader) { m_pVertexShader->Release(); m_pVertexShader = nullptr; }
    m_pDevice = nullptr;
    m_pContext = nullptr;
}

void BackgroundRenderer::Update(float deltaTime, bool isBossActive)
{
    m_cbData.time += deltaTime;

    // Smooth transition between normal and boss modes
    m_targetBossIntensity = isBossActive ? 1.0f : 0.0f;
    m_cbData.bossIntensity += (m_targetBossIntensity - m_cbData.bossIntensity) * 2.0f * deltaTime;
    m_cbData.bossIntensity = std::clamp(m_cbData.bossIntensity, 0.0f, 1.0f);

    // Dynamic parameter adjustments based on boss encounter state
    m_cbData.distortionStrength = 0.003f + m_cbData.bossIntensity * 0.003f; // 0.003 -> 0.006
    m_cbData.flowSpeed          = 0.25f  + m_cbData.bossIntensity * 0.20f;  // 0.25  -> 0.45
    m_cbData.pulseStrength      = 0.02f  + m_cbData.bossIntensity * 0.02f;  // 0.02  -> 0.04
}

void BackgroundRenderer::SetBossIntensity(float value)
{
    m_cbData.bossIntensity = std::clamp(value, 0.0f, 1.0f);
    m_targetBossIntensity = m_cbData.bossIntensity;
}

void BackgroundRenderer::Render(float camX, float camY)
{
    if (!m_pContext || !m_pVertexShader || !m_pPixelShader || !m_pConstantBuffer) return;

    m_cbData.camOffsetX = camX;
    m_cbData.camOffsetY = camY;

    // Update GPU Constant Buffer
    m_pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &m_cbData, 0, 0);

    // Set Shaders and States
    m_pContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pContext->PSSetShader(m_pPixelShader, nullptr, 0);
    m_pContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

    m_pContext->IASetInputLayout(nullptr);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pContext->OMSetDepthStencilState(m_pDepthStencilState, 0);
    m_pContext->OMSetBlendState(m_pBlendState, nullptr, 0xFFFFFFFF);

    // Draw procedural fullscreen triangle covering the whole screen
    m_pContext->Draw(3, 0);
}
