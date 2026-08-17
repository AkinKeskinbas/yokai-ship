#ifndef DIRECT3D_H
#define	DIRECT3D_H
#include <Windows.h>

#include <d3d11.h>

#define SAFE_RELEASE(o) if (o) { (o)->Release(); o = NULL; } 

// VSyncを有効にするかどうか (true: VSync有効, false: ティアリングありのフリーループ)
static constexpr bool USE_VSYNC = true;


bool Direct3D_Initialize(HWND window_handle);
void Direct3D_Begin();
void Direct3D_Present();

void Direct3D_Finalize();

ID3D11Device* Direct3D_GetDevice();
ID3D11DeviceContext* Direct3D_GetContext();
ID3D11DeviceContext* Direct3D_GetDeviceContext();
unsigned int Direct3D_GetBackBufferWidth();
unsigned int Direct3D_GetBackBufferHeight();
const D3D11_VIEWPORT* Direct3D_GetViewport(int index);
void Direct3D_Clear();

#endif // DIRECT3D_H

