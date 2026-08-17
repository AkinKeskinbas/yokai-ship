/*==============================================================================
        ウィンドウ表示 [main.cpp]

                                            Author : Akin Keskinbas
                                            Date   : 2026/7/1
==============================================================================*/

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "direct3d.h"
#include "application.h"
#include "keyboard.h"
#include "mouse.h"
#include <algorithm>
#include <stdio.h>

#include "configuration.h"

/*------------------------------------------------------------------------------
        ウィンドウ情報
------------------------------------------------------------------------------*/
static constexpr char WINDOW_CLASS[] = "GameWindow"; // メインウィンドウクラス名
static constexpr char TITLE[] = "Yokai Ship";     // タイトルバーのテキスト

/*------------------------------------------------------------------------------
        ウィンドウプロシージャ プロトタイプ宣言
------------------------------------------------------------------------------*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 高精度タイマーによる経過時間計測 (秒単位)
double SystemTimer_GetElapsedTime()
{
    static LARGE_INTEGER frequency;
    static LARGE_INTEGER lastTime;
    static bool initialized = false;
    if (!initialized)
    {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastTime);
        initialized = true;
    }
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    double elapsed = (double)(currentTime.QuadPart - lastTime.QuadPart) / (double)frequency.QuadPart;
    lastTime = currentTime;
    return elapsed;
}

/*------------------------------------------------------------------------------
        メイン
------------------------------------------------------------------------------*/
int APIENTRY WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    /* ウィンドウクラスの登録 */
    WNDCLASSEX wcex{};

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WndProc;

    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr; // メニューは作らない
    wcex.lpszClassName = WINDOW_CLASS;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    RegisterClassEx(&wcex);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);


    // プライマリモニターの画面解像度取得
    const int DESKTOP_WIDTH = GetSystemMetrics(SM_CXSCREEN);
    const int DESKTOP_HEIGHT = GetSystemMetrics(SM_CYSCREEN);
   

    RECT window_rect{
     .left = 0,
     .top = 0,
     .right = SCREEN_WIDTH,
     .bottom = SCREEN_HEIGHT
    };

    constexpr DWORD WINDOW_STYLE =
        WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

    AdjustWindowRect(&window_rect, WINDOW_STYLE, FALSE);

    const int WINDOW_WIDTH = window_rect.right - window_rect.left;
    const int WINDOW_HEIGHT = window_rect.bottom - window_rect.top;

    //ekranin yerini belirliyor ortada gosterir burda
    const int WINDOW_X = std::max((DESKTOP_WIDTH - WINDOW_WIDTH) / 2, 0);
    const int WINDOW_Y = std::max((DESKTOP_HEIGHT - WINDOW_HEIGHT) / 2, 0);

    HWND hWnd = CreateWindow(
        WINDOW_CLASS,
        TITLE,
        WINDOW_STYLE,
        WINDOW_X,
        WINDOW_Y,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!Direct3D_Initialize(hWnd))
    {
        return 0;
    }
    Application_Initialize(hWnd);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    /* メッセージループ */
    MSG msg;

    // 固定時間ステップ（60fps固定）のための変数定義
    static constexpr double TARGET_FPS = 60.0;
    static constexpr double FIXED_DELTA_TIME = 1.0 / TARGET_FPS; // 約0.016666秒
    double fixed_time_accumulator = 0.0;
    double elapsed_time = 0.0;
    double time_accumulator = 0.0;
    int frame_counter = 0;
    double fps = 0.0;

    do
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // ウィンドウメッセージが来ていたら
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // 前フレームからの経過時間を測定
            elapsed_time = SystemTimer_GetElapsedTime();

            // アプリ切り替え等による巨大な時間経過を制限（床抜けバグ防止）
            if (elapsed_time > 0.1) {
                elapsed_time = 0.1;
            }

            time_accumulator += elapsed_time;
            frame_counter++;

            // 1秒経過ごとにFPS値を更新
            if (time_accumulator >= 1.0) {
                fps = frame_counter / time_accumulator;
                frame_counter = 0;
                time_accumulator = 0.0;

                // タイトルバーにFPSを表示
                char titleBuffer[128];
                sprintf_s(titleBuffer, "%s - FPS: %.1f", TITLE, fps);
                SetWindowTextA(hWnd, titleBuffer);
            }

            // アキュムレータに経過時間を蓄積
            fixed_time_accumulator += elapsed_time;

            // アキュムレータが固定時間（1/60秒）を超えている間、FixedUpdateを呼び出す
            // 処理落ちによる無限ループ（スパイラル・オブ・デス）防止のため最大5回に制限
            int update_count = 0;
            while (fixed_time_accumulator >= FIXED_DELTA_TIME && update_count < 5) 
            {
                Application_FixedUpdate();
                fixed_time_accumulator -= FIXED_DELTA_TIME;
                update_count++;
            }

            // 可変フレームレート更新（経過時間を渡し、キャスト適用）
            Application_Update((float)elapsed_time);

            Direct3D_Clear();
            Application_Draw();   
            Direct3D_Present();
        }

    }
    while (msg.message != WM_QUIT);


    Application_Finalize();
    Direct3D_Finalize();

    return 0;
}

/*------------------------------------------------------------------------------
        ウィンドウプロシージャ
------------------------------------------------------------------------------*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ACTIVATEAPP:
        Keyboard_ProcessMessage(uMsg, wParam, lParam);
        Mouse_ProcessMessage(uMsg, wParam, lParam);
        break;

    case WM_KEYDOWN:
        // Klavye mesajlarını işle
        [[fallthrough]];
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        Keyboard_ProcessMessage(uMsg, wParam, lParam);
        break;

    case WM_INPUT:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_MOUSEHOVER:
        Mouse_ProcessMessage(uMsg, wParam, lParam);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
