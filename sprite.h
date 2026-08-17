/*==============================================================================
    Sprite Rendering [sprite.h]
==============================================================================*/
#ifndef SPRITE_H
#define SPRITE_H

#include <DirectXMath.h>

bool Sprite_Initialize();
void Sprite_Finalize();

// Draw texture at position with default size
void Sprite_Draw(int texture_id, float position_x, float position_y, 
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

// Draw texture scaled
void Sprite_Draw(
	int texture_id,
	float position_x, float position_y,
	float width, float height,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

// Draw sub-rectangle of texture
void Sprite_Draw(
	int texture_id,
	float position_x, float position_y,
	int texture_x, int texture_y,
	int texture_width, int texture_height,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

// Draw sub-rectangle of texture scaled
void Sprite_Draw(
	int texture_id,
	float position_x, float position_y,
	float width, float height,
	int texture_x, int texture_y,
	int texture_width, int texture_height,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

// Draw sub-rectangle of texture with rotation and custom scale
void Sprite_Draw(
	int texture_id,
	float position_x, float position_y,
	float width, float height,
	int texture_x, int texture_y,
	int texture_width, int texture_height,
	float angle,
	const DirectX::XMFLOAT2& scale = { 1.0f, 1.0f },
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

// Primitive solid rendering helpers
void Sprite_DrawRect(float x, float y, float width, float height, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
void Sprite_DrawRectBorder(float x, float y, float width, float height, float thickness, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
void Sprite_DrawLine(float x1, float y1, float x2, float y2, float thickness = 1.0f, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
void Sprite_DrawCircle(float centerX, float centerY, float radius, float thickness = 1.5f, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f }, int segments = 36);

#endif // SPRITE_H
