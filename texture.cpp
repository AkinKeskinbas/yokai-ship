#include "texture.h"
#include "direct3d.h"
#include "WICTextureLoader11.h"
#include <string>
#include <DirectXMath.h>

using namespace DirectX;

// 管理できるテクスチャの最大数
static constexpr int TEXTURE_MAX = 1024;

// 1つのテクスチャが持つ情報
struct Texture
{
    std::wstring filename;                    // ファイル名（重複チェック用）
    unsigned int width = 0;                   // 幅
    unsigned int height = 0;                  // 高さ
    ID3D11Resource* pTexture = nullptr;       // テクスチャリソース
    ID3D11ShaderResourceView* pTextureView = nullptr; // シェーダーリソースビュー
    int ref_count = 0;                        // 参照カウンタ（貸し出し回数）
};

// テクスチャ管理用の配列
static Texture g_Textures[TEXTURE_MAX];

// デバイスとコンテキストの保存用ポインタ
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

#include <objbase.h>

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    // COMライブラリの初期化 (WICテクスチャローダーのため)
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // デバイスとデバイスコンテキストを保存
    g_pDevice = pDevice;
    g_pContext = pContext;
}

void Texture_Finalize()
{
    // 全てのテクスチャを強制解放
    Texture_AllRelease();
    CoUninitialize();
}

int Texture_Load(const wchar_t* pFileName, bool bMipMap)
{
    if (!g_pDevice || !g_pContext) {
        return TEXTURE_INVALID_ID;
    }
    if (!pFileName || pFileName[0] == L'\0') {
        return TEXTURE_INVALID_ID;
    }

    // ① すでに読み込まれているファイルかチェック (キャッシュ機能)
    for (int i = 0; i < TEXTURE_MAX; i++) {
        if (!g_Textures[i].pTextureView) continue;

        // 同じファイル名が見つかったら、参照カウンタを増やしてそのIDを返す
        if (g_Textures[i].filename == pFileName) {
            g_Textures[i].ref_count++;
            return i;
        }
    }

    // ② 新しく読み込む処理
    for (int i = 0; i < TEXTURE_MAX; i++) {
        // 空いている場所を探す
        if (g_Textures[i].pTextureView) continue;

        HRESULT hr;
        // ミップマップの有無で読み込み関数を分ける
        if (bMipMap) {
            hr = CreateWICTextureFromFile(g_pDevice, g_pContext, pFileName, 
                &g_Textures[i].pTexture, &g_Textures[i].pTextureView);
        }
        else {
            hr = CreateWICTextureFromFile(g_pDevice, pFileName, 
                &g_Textures[i].pTexture, &g_Textures[i].pTextureView);
        }

        if (FAILED(hr)) {
            MessageBoxW(nullptr, L"テクスチャの読み込みに失敗しました", pFileName, 
                MB_OK | MB_ICONERROR);
            return TEXTURE_INVALID_ID;
        }

        // ③ 画像のサイズ（幅と高さ）を取得して保存
        ID3D11Texture2D* pTexture2D = (ID3D11Texture2D*)g_Textures[i].pTexture;
        D3D11_TEXTURE2D_DESC t2desc;
        pTexture2D->GetDesc(&t2desc);
        g_Textures[i].width = t2desc.Width;
        g_Textures[i].height = t2desc.Height;

        // ファイル名を記録
        g_Textures[i].filename = pFileName;
        g_Textures[i].ref_count = 1;

        // 登録したID（配列のインデックス）を返す
        return i;
    }

    return TEXTURE_INVALID_ID;
}

void Texture_SetTexture(int texture_id)
{
    if (texture_id < 0 || texture_id >= TEXTURE_MAX || !g_pContext) {
        return;
    }
    if (!g_Textures[texture_id].pTextureView) {
        return;
    }

    // 指定されたIDのテクスチャをピクセルシェーダーにセット
    g_pContext->PSSetShaderResources(0, 1, &g_Textures[texture_id].pTextureView);
}

DirectX::XMUINT2 Texture_GetSize(int texture_id)
{
    if (texture_id < 0 || texture_id >= TEXTURE_MAX) {
        return { 0, 0 };
    }
    return { g_Textures[texture_id].width, g_Textures[texture_id].height };
}

unsigned int Texture_GetWidth(int texture_id)
{
    if (texture_id < 0 || texture_id >= TEXTURE_MAX) {
        return 0;
    }
    return g_Textures[texture_id].width;
}

unsigned int Texture_GetHeight(int texture_id)
{
    if (texture_id < 0 || texture_id >= TEXTURE_MAX) {
        return 0;
    }
    return g_Textures[texture_id].height;
}

void Texture_Release(int texture_id)
{
    if (texture_id < 0 || texture_id >= TEXTURE_MAX) {
        return;
    }

    Texture& t = g_Textures[texture_id];
    if (!t.pTextureView) {
        return;
    }

    // 参照カウンタを減らす
    t.ref_count--;

    // 参照カウンタが0以下になったら実際に解放する
    if (t.ref_count <= 0) {
        SAFE_RELEASE(t.pTextureView);
        SAFE_RELEASE(t.pTexture);
        t.filename.clear();
        t.width = 0;
        t.height = 0;
        t.ref_count = 0;
    }
}

void Texture_Release(const int* pTextureIDs, int count)
{
    if (!pTextureIDs || count <= 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        Texture_Release(pTextureIDs[i]);
    }
}

void Texture_AllRelease()
{
    for (int i = 0; i < TEXTURE_MAX; i++) {
        Texture& t = g_Textures[i];
        SAFE_RELEASE(t.pTextureView);
        SAFE_RELEASE(t.pTexture);
        t.filename.clear();
        t.width = 0;
        t.height = 0;
        t.ref_count = 0;
    }
}
