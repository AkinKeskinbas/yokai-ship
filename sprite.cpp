/*==============================================================================
    Sprite Rendering [sprite.cpp]
==============================================================================*/
#include "sprite.h"

#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;

#include "configuration.h"
#include "debug_ostream.h"
#include "direct3d.h"
#include "shader.h"
#include "texture.h"
#include <cmath>

static ID3D11Buffer* g_pVertexBuffer{ nullptr };
static ID3D11SamplerState* g_pSamplerState{ nullptr };
static ID3D11BlendState* g_pBlendState{ nullptr };
static ID3D11DepthStencilState* g_pDepthStencilState{ nullptr };
static ID3D11Buffer* g_pVSConstantBuffer1{ nullptr };
static ID3D11Buffer* g_pPSConstantBuffer0{ nullptr };
static ID3D11ShaderResourceView* g_pWhiteSRV{ nullptr };

// 頂点構造体
struct Vertex {
    XMFLOAT3 position; // 頂点座標
    XMFLOAT4 color;    // 頂点カラー
    XMFLOAT2 texcoord; // テクスチャ座標
};

static constexpr int NUM_VERTEX{ 4 };

bool Sprite_Initialize()
{
    // 頂点バッファ生成
    D3D11_BUFFER_DESC bd{
        .ByteWidth = sizeof(Vertex) * NUM_VERTEX,
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
    };

    // 頂点バッファへ送るデータの作成
    Vertex v[NUM_VERTEX]{};

    // 頂点情報を書き込み
    v[0].position = { -0.5f, -0.5f, 0.0f }; // 左上
    v[0].color    = { 1.0f, 1.0f, 1.0f, 1.0f };
    v[0].texcoord = { 0.0f, 0.0f };

    v[1].position = {  0.5f, -0.5f, 0.0f }; // 右上
    v[1].color    = { 1.0f, 1.0f, 1.0f, 1.0f };
    v[1].texcoord = { 1.0f, 0.0f };

    v[2].position = { -0.5f,  0.5f, 0.0f }; // 左下
    v[2].color    = { 1.0f, 1.0f, 1.0f, 1.0f };
    v[2].texcoord = { 0.0f, 1.0f };

    v[3].position = {  0.5f,  0.5f, 0.0f }; // 右下
    v[3].color    = { 1.0f, 1.0f, 1.0f, 1.0f };
    v[3].texcoord = { 1.0f, 1.0f };

    D3D11_SUBRESOURCE_DATA sd{}; 
    sd.pSysMem = v;

    HRESULT hr = Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
    if (FAILED(hr)) {
        hal::dout << "Sprite.cpp:頂点バッファの生成に失敗しました。\n";
        return false;
    }

    // 定数バッファUV用マトリクスバッファの作成
    D3D11_BUFFER_DESC cb_desc{};
    cb_desc.ByteWidth = sizeof(XMFLOAT4X4);
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = Direct3D_GetDevice()->CreateBuffer(&cb_desc, nullptr, &g_pVSConstantBuffer1);
    if (FAILED(hr)) {
        hal::dout << "Sprite.cpp:UV用行列の定数バッファの生成に失敗しました。\n";
        return false;
    }

    // 定数バッファカラーバッファの作成
    cb_desc.ByteWidth = sizeof(XMFLOAT4);
    hr = Direct3D_GetDevice()->CreateBuffer(&cb_desc, nullptr, &g_pPSConstantBuffer0);
    if (FAILED(hr)) {
        hal::dout << "Sprite.cpp:カラー用定数バッファの生成に失敗しました。\n";
        return false;
    }

    // サンプラーステートの作成
    D3D11_SAMPLER_DESC sampler_desc{
        .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .BorderColor = { 1.0f, 1.0f, 0.0f, 1.0f },
        .MinLOD = 0,
        .MaxLOD = D3D11_FLOAT32_MAX
    };

    Direct3D_GetDevice()->CreateSamplerState(&sampler_desc, &g_pSamplerState);

    // ブレンドステートの作成
    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    Direct3D_GetDevice()->CreateBlendState(&blend_desc, &g_pBlendState);

    // デプスステンシルステートの作成
    D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{}; 
    depth_stencil_desc.DepthEnable = FALSE;
    Direct3D_GetDevice()->CreateDepthStencilState(&depth_stencil_desc, &g_pDepthStencilState);

    // 1x1 White Texture 생성
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    uint32_t whitePixel = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA subData{};
    subData.pSysMem = &whitePixel;
    subData.SysMemPitch = sizeof(uint32_t);

    ID3D11Texture2D* pTex = nullptr;
    if (SUCCEEDED(Direct3D_GetDevice()->CreateTexture2D(&texDesc, &subData, &pTex)))
    {
        Direct3D_GetDevice()->CreateShaderResourceView(pTex, nullptr, &g_pWhiteSRV);
        pTex->Release();
    }

    return true;
}

void Sprite_Finalize()
{
    SAFE_RELEASE(g_pWhiteSRV);
    SAFE_RELEASE(g_pDepthStencilState);
    SAFE_RELEASE(g_pBlendState);
    SAFE_RELEASE(g_pSamplerState);
    SAFE_RELEASE(g_pPSConstantBuffer0);
    SAFE_RELEASE(g_pVSConstantBuffer1);
    SAFE_RELEASE(g_pVertexBuffer);
}

void Sprite_Draw(int texture_id, float position_x, float position_y, const DirectX::XMFLOAT4& color)
{
    Sprite_Draw(texture_id, position_x, position_y,
        (float)Texture_GetWidth(texture_id), (float)Texture_GetHeight(texture_id), color);
}

void Sprite_Draw(int texture_id, float position_x, float position_y, float width, float height, const DirectX::XMFLOAT4& color)
{
    Direct3D_GetDeviceContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &color, 0, 0);
    Direct3D_GetDeviceContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);

    Shader_Begin();

    XMMATRIX mtxS = XMMatrixScaling(width, height, 1.0f);
    XMMATRIX mtxT = XMMatrixTranslation(position_x + width * 0.5f, position_y + height * 0.5f, 0.0f);
    XMMATRIX mtxP = XMMatrixOrthographicOffCenterLH(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
    XMMATRIX mtx = mtxS * mtxT * mtxP;

    Shader_SetMatrix(mtx);

    XMMATRIX mtxI = XMMatrixIdentity();
    XMFLOAT4X4 mtxUV;
    XMStoreFloat4x4(&mtxUV, mtxI);

    Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &mtxUV, 0, 0);
    Direct3D_GetDeviceContext()->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer1);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Direct3D_GetDeviceContext()->PSSetSamplers(0, 1, &g_pSamplerState);
    Texture_SetTexture(texture_id);
    Direct3D_GetDeviceContext()->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);
    Direct3D_GetDeviceContext()->OMSetDepthStencilState(g_pDepthStencilState, 0);
    Direct3D_GetDeviceContext()->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(int texture_id, float position_x, float position_y, int texture_x, int texture_y, int texture_width, int texture_height, const DirectX::XMFLOAT4& color)
{
    Sprite_Draw(texture_id, position_x, position_y, (float)texture_width, (float)texture_height, texture_x, texture_y, texture_width, texture_height, color);
}

void Sprite_Draw(int texture_id, float position_x, float position_y, float width, float height, int texture_x, int texture_y, int texture_width, int texture_height, const DirectX::XMFLOAT4& color)
{
    Direct3D_GetDeviceContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &color, 0, 0);
    Direct3D_GetDeviceContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);

    Shader_Begin();

    XMMATRIX mtxS = XMMatrixScaling(width, height, 1.0f);
    XMMATRIX mtxT = XMMatrixTranslation(position_x + width * 0.5f, position_y + height * 0.5f, 0.0f);
    XMMATRIX mtxP = XMMatrixOrthographicOffCenterLH(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
    XMMATRIX mtx = mtxS * mtxT * mtxP;

    Shader_SetMatrix(mtx);

    float tx = texture_x / (float)Texture_GetWidth(texture_id);
    float ty = texture_y / (float)Texture_GetHeight(texture_id);
    float tw = texture_width / (float)Texture_GetWidth(texture_id);
    float th = texture_height / (float)Texture_GetHeight(texture_id);

    mtxS = XMMatrixScaling(tw, th, 1.0f);
    mtxT = XMMatrixTranslation(tx, ty, 0.0f);
    XMFLOAT4X4 mtxUV;
    XMStoreFloat4x4(&mtxUV, XMMatrixTranspose(mtxS * mtxT));

    Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &mtxUV, 0, 0);
    Direct3D_GetDeviceContext()->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer1);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Direct3D_GetDeviceContext()->PSSetSamplers(0, 1, &g_pSamplerState);
    Texture_SetTexture(texture_id);
    Direct3D_GetDeviceContext()->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);
    Direct3D_GetDeviceContext()->OMSetDepthStencilState(g_pDepthStencilState, 0);
    Direct3D_GetDeviceContext()->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(int texture_id, float position_x, float position_y, float width, float height, int texture_x, int texture_y, int texture_width, int texture_height, float angle, const DirectX::XMFLOAT2& scale, const DirectX::XMFLOAT4& color)
{
    Direct3D_GetDeviceContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &color, 0, 0);
    Direct3D_GetDeviceContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);

    Shader_Begin();

    XMMATRIX mtxS = XMMatrixScaling(width * scale.x, height * scale.y, 1.0f);
    XMMATRIX mtxR = XMMatrixRotationZ(angle);
    XMMATRIX mtxT = XMMatrixTranslation(position_x + width * 0.5f, position_y + height * 0.5f, 0.0f);
    XMMATRIX mtxP = XMMatrixOrthographicOffCenterLH(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
    XMMATRIX mtx = mtxS * mtxR * mtxT * mtxP;

    Shader_SetMatrix(mtx);

    float tx = texture_x / (float)Texture_GetWidth(texture_id);
    float ty = texture_y / (float)Texture_GetHeight(texture_id);
    float tw = texture_width / (float)Texture_GetWidth(texture_id);
    float th = texture_height / (float)Texture_GetHeight(texture_id);

    mtxS = XMMatrixScaling(tw, th, 1.0f);
    mtxT = XMMatrixTranslation(tx, ty, 0.0f);
    XMFLOAT4X4 mtxUV;
    XMStoreFloat4x4(&mtxUV, XMMatrixTranspose(mtxS * mtxT));

    Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &mtxUV, 0, 0);
    Direct3D_GetDeviceContext()->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer1);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Direct3D_GetDeviceContext()->PSSetSamplers(0, 1, &g_pSamplerState);
    Texture_SetTexture(texture_id);
    Direct3D_GetDeviceContext()->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);
    Direct3D_GetDeviceContext()->OMSetDepthStencilState(g_pDepthStencilState, 0);
    Direct3D_GetDeviceContext()->Draw(NUM_VERTEX, 0);
}

void Sprite_DrawRect(float x, float y, float width, float height, const DirectX::XMFLOAT4& color)
{
    if (!g_pWhiteSRV) return;
    Direct3D_GetDeviceContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &color, 0, 0);
    Direct3D_GetDeviceContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);

    Shader_Begin();

    XMMATRIX mtxS = XMMatrixScaling(width, height, 1.0f);
    XMMATRIX mtxT = XMMatrixTranslation(x + width * 0.5f, y + height * 0.5f, 0.0f);
    XMMATRIX mtxP = XMMatrixOrthographicOffCenterLH(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
    XMMATRIX mtx = mtxS * mtxT * mtxP;
    Shader_SetMatrix(mtx);

    XMMATRIX mtxI = XMMatrixIdentity();
    XMFLOAT4X4 mtxUV;
    XMStoreFloat4x4(&mtxUV, mtxI);
    Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &mtxUV, 0, 0);
    Direct3D_GetDeviceContext()->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer1);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Direct3D_GetDeviceContext()->PSSetSamplers(0, 1, &g_pSamplerState);
    Direct3D_GetDeviceContext()->PSSetShaderResources(0, 1, &g_pWhiteSRV);
    Direct3D_GetDeviceContext()->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);
    Direct3D_GetDeviceContext()->OMSetDepthStencilState(g_pDepthStencilState, 0);
    Direct3D_GetDeviceContext()->Draw(NUM_VERTEX, 0);
}

void Sprite_DrawRectBorder(float x, float y, float width, float height, float thickness, const DirectX::XMFLOAT4& color)
{
    Sprite_DrawRect(x, y, width, thickness, color); // top
    Sprite_DrawRect(x, y + height - thickness, width, thickness, color); // bottom
    Sprite_DrawRect(x, y + thickness, thickness, height - 2.0f * thickness, color); // left
    Sprite_DrawRect(x + width - thickness, y + thickness, thickness, height - 2.0f * thickness, color); // right
}

void Sprite_DrawLine(float x1, float y1, float x2, float y2, float thickness, const DirectX::XMFLOAT4& color)
{
    if (!g_pWhiteSRV) return;
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float angle = atan2f(dy, dx);
    float midX = (x1 + x2) * 0.5f;
    float midY = (y1 + y2) * 0.5f;

    Direct3D_GetDeviceContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &color, 0, 0);
    Direct3D_GetDeviceContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);

    Shader_Begin();

    XMMATRIX mtxS = XMMatrixScaling(len, thickness, 1.0f);
    XMMATRIX mtxR = XMMatrixRotationZ(angle);
    XMMATRIX mtxT = XMMatrixTranslation(midX, midY, 0.0f);
    XMMATRIX mtxP = XMMatrixOrthographicOffCenterLH(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
    XMMATRIX mtx = mtxS * mtxR * mtxT * mtxP;
    Shader_SetMatrix(mtx);

    XMMATRIX mtxI = XMMatrixIdentity();
    XMFLOAT4X4 mtxUV;
    XMStoreFloat4x4(&mtxUV, mtxI);
    Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &mtxUV, 0, 0);
    Direct3D_GetDeviceContext()->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer1);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    Direct3D_GetDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    Direct3D_GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Direct3D_GetDeviceContext()->PSSetSamplers(0, 1, &g_pSamplerState);
    Direct3D_GetDeviceContext()->PSSetShaderResources(0, 1, &g_pWhiteSRV);
    Direct3D_GetDeviceContext()->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);
    Direct3D_GetDeviceContext()->OMSetDepthStencilState(g_pDepthStencilState, 0);
    Direct3D_GetDeviceContext()->Draw(NUM_VERTEX, 0);
}

void Sprite_DrawCircle(float centerX, float centerY, float radius, float thickness, const DirectX::XMFLOAT4& color, int segments)
{
    if (radius <= 0.0f || segments < 3) return;
    float angleStep = (2.0f * 3.14159265f) / (float)segments;
    for (int i = 0; i < segments; ++i)
    {
        float a1 = (float)i * angleStep;
        float a2 = (float)(i + 1) * angleStep;
        float x1 = centerX + cosf(a1) * radius;
        float y1 = centerY + sinf(a1) * radius;
        float x2 = centerX + cosf(a2) * radius;
        float y2 = centerY + sinf(a2) * radius;
        Sprite_DrawLine(x1, y1, x2, y2, thickness, color);
    }
}

