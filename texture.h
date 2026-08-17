#ifndef TEXTURE_H
#define TEXTURE_H

#include <d3d11.h>
#include <DirectXMath.h>

// 初期化と終了処理
void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Texture_Finalize();

// テクスチャの読み込み
// 戻り値：テクスチャID（失敗した場合は TEXTURE_INVALID_ID を返す）
int Texture_Load(const wchar_t* pFileName, bool bMipMap = true);
constexpr int TEXTURE_INVALID_ID = -1;

// テクスチャの解放
void Texture_Release(int texture_id);
void Texture_Release(const int* pTextureIDs, int count);
void Texture_AllRelease();

// 描画時のテクスチャ設定
void Texture_SetTexture(int texture_id);

// テクスチャ情報の取得
DirectX::XMUINT2 Texture_GetSize(int texture_id);
unsigned int Texture_GetWidth(int texture_id);
unsigned int Texture_GetHeight(int texture_id);

#endif // TEXTURE_H
