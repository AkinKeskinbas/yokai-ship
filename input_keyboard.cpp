/*==============================================================================

   キーボード入力ラッパー [input_keyboard.cpp]
                                                         Author : Akin Keskinbas
                                                         Date   : 2026/7/1
--------------------------------------------------------------------------------

==============================================================================*/
#include "input_keyboard.h"
#include <cstring>

// キーリピート管理用の最大キー数
static const int KEY_MAX = 0xFF;

// 現在フレームのキーボード状態
static Keyboard_State gCurrKeyState = {};

// 前フレームのキーボード状態
static Keyboard_State gPrevKeyState = {};

// 各キーの押下時間（秒、リピート用）
static float gKeyRepeatTimer[KEY_MAX] = {};

// リピートトリガーが発火した回数（インターバル制御用）
static int gKeyRepeatTriggerCount[KEY_MAX] = {};


// ========== 初期化 / 更新 ==========

void InputKeyboard_Initialize(void)
{
    Keyboard_Reset();
    std::memset(&gCurrKeyState, 0, sizeof(gCurrKeyState));
    std::memset(&gPrevKeyState, 0, sizeof(gPrevKeyState));
    std::memset(gKeyRepeatTimer, 0, sizeof(gKeyRepeatTimer));
    std::memset(gKeyRepeatTriggerCount, 0, sizeof(gKeyRepeatTriggerCount));
}

void InputKeyboard_Update(float delta_time)
{
    // 前フレームの状態を保存
    gPrevKeyState = gCurrKeyState;

    // 現在の状態を取得
    gCurrKeyState = *Keyboard_GetState();

    // --- キーリピートタイマーの更新 ---
    for (int i = 0; i < KEY_MAX; ++i)
    {
        Keyboard_Keys k = static_cast<Keyboard_Keys>(i);
        if (Keyboard_IsKeyDown(k, &gCurrKeyState))
        {
            gKeyRepeatTimer[i] += delta_time;
        }
        else
        {
            gKeyRepeatTimer[i] = 0.0f;
            gKeyRepeatTriggerCount[i] = 0;
        }
    }
}


// ========== 判定処理 ==========

bool InputKeyboard_IsPress(Keyboard_Keys key)
{
    return Keyboard_IsKeyDown(key, &gCurrKeyState);
}

bool InputKeyboard_IsTrigger(Keyboard_Keys key)
{
    bool prev = Keyboard_IsKeyDown(key, &gPrevKeyState);
    bool curr = Keyboard_IsKeyDown(key, &gCurrKeyState);
    return (!prev && curr);
}

bool InputKeyboard_IsRelease(Keyboard_Keys key)
{
    bool prev = Keyboard_IsKeyDown(key, &gPrevKeyState);
    bool curr = Keyboard_IsKeyDown(key, &gCurrKeyState);
    return (prev && !curr);
}

bool InputKeyboard_IsRepeat(Keyboard_Keys key, float delaySeconds, float intervalSeconds)
{
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= KEY_MAX) { return false; }

    float elapsed = gKeyRepeatTimer[idx];
    if (elapsed <= 0.0f) { return false; }

    // 押した瞬間 (タイマーの最初の更新)
    if (gKeyRepeatTriggerCount[idx] == 0)
    {
        gKeyRepeatTriggerCount[idx] = 1;
        return true;
    }

    // ディレイ時間を超えているか
    if (elapsed >= delaySeconds)
    {
        float repeatTime = elapsed - delaySeconds;
        int targetTriggers = 1 + static_cast<int>(repeatTime / intervalSeconds);

        if (targetTriggers > gKeyRepeatTriggerCount[idx])
        {
            gKeyRepeatTriggerCount[idx] = targetTriggers;
            return true;
        }
    }

    return false;
}