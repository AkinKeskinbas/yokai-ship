#include <Windows.h>
#include "application.h"
#include "direct3d.h"
#include "shader.h"
#include "configuration.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "input_xinput.h"
#include "audio.h"
#include <DirectXMath.h>
#include "game.h"
#include <memory>

static std::unique_ptr<Game> g_pGame = nullptr;

void Application_Initialize(HWND hWnd)
{
    // 各グラフィックスシステムの初期化
    if (!Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext()))
    {
        MessageBox(nullptr, "Shader Initialization failed!", "Fatal Error", MB_OK | MB_ICONERROR);
        return;
    }

    // 入力システムの初期化
    InputKeyboard_Initialize();
    InputMouse_Initialize(hWnd);
    InputXInput_Initialize();
    InitAudio();

    // マウスカーソルを表示に設定
    InputMouse_SetVisible(true);

    // 正射影行列の設定 (画面全体)
    DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicOffCenterLH(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
    Shader_SetMatrix(proj);

    // ゲームの初期化
    g_pGame = std::make_unique<Game>();
    if (!g_pGame->Initialize(hWnd))
    {
        MessageBox(nullptr, "Game Initialization failed!", "Fatal Error", MB_OK | MB_ICONERROR);
    }
}

void Application_Finalize()
{
    if (g_pGame)
    {
        g_pGame->Finalize();
        g_pGame.reset();
    }

    // 各システムの終了処理
    InputMouse_Finalize();
    Shader_Finalize();
    UninitAudio();
}

void Application_Update(float delta_time)
{
    // 入力状態の更新
    InputKeyboard_Update(delta_time);
    InputMouse_Update();
    InputXInput_Update(delta_time);

    if (g_pGame)
    {
        g_pGame->Update(delta_time);
    }
}

void Application_FixedUpdate()
{
}

void Application_Draw()
{
    // シェーダーの開始を設定
    Shader_Begin();

    if (g_pGame)
    {
        g_pGame->Draw();
    }
}
