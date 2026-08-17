/*==============================================================================

   XInput ゲームパッド入力ラッパー [input_xinput.cpp]
                                                         Author : Akin Keskinbas
                                                         Date   : 2026/7/1
--------------------------------------------------------------------------------

==============================================================================*/
#include "input_xinput.h"
#include <cstring>
#include <algorithm>

// XInputライブラリのリンク指定
#pragma comment(lib, "Xinput.lib")

// 最大コントローラー数 (XInputの仕様で最大4台)
static const int PAD_MAX = 4;
// デジタルボタン数 (16ビットのフラグ)
static const int BUTTON_MAX = 16;

// コントローラーの接続状態と状態データ
static bool gIsConnected[PAD_MAX] = {};
static XINPUT_STATE gCurrState[PAD_MAX] = {};
static XINPUT_STATE gPrevState[PAD_MAX] = {};

// ボタンリピート用のタイマーと発火回数
static float gRepeatTimer[PAD_MAX][BUTTON_MAX] = {};
static int gRepeatTriggerCount[PAD_MAX][BUTTON_MAX] = {};

// 各コントローラーの振動設定値（Update の最後に適用）
static XINPUT_VIBRATION gVibration[PAD_MAX] = {};


// ビットフラグからインデックス (0 ～ 15) への変換
static int ButtonBitToIndex(WORD buttonBitMask)
{
    for (int i = 0; i < BUTTON_MAX; ++i)
    {
        if (buttonBitMask & (1 << i))
        {
            return i;
        }
    }
    return -1;
}

// ========== 初期化 / 更新 ==========

void InputXInput_Initialize(void)
{
    std::memset(gIsConnected, 0, sizeof(gIsConnected));
    std::memset(gCurrState, 0, sizeof(gCurrState));
    std::memset(gPrevState, 0, sizeof(gPrevState));
    std::memset(gRepeatTimer, 0, sizeof(gRepeatTimer));
    std::memset(gRepeatTriggerCount, 0, sizeof(gRepeatTriggerCount));
    std::memset(gVibration, 0, sizeof(gVibration));
}

void InputXInput_Update(float delta_time)
{
    for (int pad = 0; pad < PAD_MAX; ++pad)
    {
        gPrevState[pad] = gCurrState[pad];

        // 状態取得
        XINPUT_STATE state = {};
        DWORD result = XInputGetState(pad, &state);

        if (result == ERROR_SUCCESS)
        {
            gIsConnected[pad] = true;
            gCurrState[pad] = state;

            // ボタンリピートタイマーの更新
            for (int btn = 0; btn < BUTTON_MAX; ++btn)
            {
                WORD mask = static_cast<WORD>(1 << btn);
                if (state.Gamepad.wButtons & mask)
                {
                    gRepeatTimer[pad][btn] += delta_time;
                }
                else
                {
                    gRepeatTimer[pad][btn] = 0.0f;
                    gRepeatTriggerCount[pad][btn] = 0;
                }
            }
        }
        else
        {
            // 切断された場合はクリア
            gIsConnected[pad] = false;
            std::memset(&gCurrState[pad], 0, sizeof(XINPUT_STATE));
            std::memset(gRepeatTimer[pad], 0, sizeof(gRepeatTimer[pad]));
            std::memset(gRepeatTriggerCount[pad], 0, sizeof(gRepeatTriggerCount[pad]));
            std::memset(&gVibration[pad], 0, sizeof(XINPUT_VIBRATION));
        }

        // 振動を適用
        XInputSetState(pad, &gVibration[pad]);
    }
}


// ========== 接続確認 / ボタン判定 ==========

bool InputXInput_IsConnected(int userIndex)
{
    if (userIndex < 0 || userIndex >= PAD_MAX) return false;
    return gIsConnected[userIndex];
}

bool InputXInput_IsPress(int userIndex, WORD buttonBitMask)
{
    if (!InputXInput_IsConnected(userIndex)) return false;
    return (gCurrState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
}

bool InputXInput_IsTrigger(int userIndex, WORD buttonBitMask)
{
    if (!InputXInput_IsConnected(userIndex)) return false;
    bool prev = (gPrevState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
    bool curr = (gCurrState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
    return (!prev && curr);
}

bool InputXInput_IsRelease(int userIndex, WORD buttonBitMask)
{
    if (!InputXInput_IsConnected(userIndex)) return false;
    bool prev = (gPrevState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
    bool curr = (gCurrState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
    return (prev && !curr);
}

bool InputXInput_IsRepeat(int userIndex, WORD buttonBitMask, float delaySeconds, float intervalSeconds)
{
    if (!InputXInput_IsConnected(userIndex)) return false;
    int btnIdx = ButtonBitToIndex(buttonBitMask);
    if (btnIdx < 0 || btnIdx >= BUTTON_MAX) return false;

    float elapsed = gRepeatTimer[userIndex][btnIdx];
    if (elapsed <= 0.0f) return false;

    // 押した瞬間
    if (gRepeatTriggerCount[userIndex][btnIdx] == 0)
    {
        gRepeatTriggerCount[userIndex][btnIdx] = 1;
        return true;
    }

    // ディレイ時間超過後のインターバル判定
    if (elapsed >= delaySeconds)
    {
        float repeatTime = elapsed - delaySeconds;
        int targetTriggers = 1 + static_cast<int>(repeatTime / intervalSeconds);

        if (targetTriggers > gRepeatTriggerCount[userIndex][btnIdx])
        {
            gRepeatTriggerCount[userIndex][btnIdx] = targetTriggers;
            return true;
        }
    }

    return false;
}


// ========== アナログスティック (スムーズなデッドゾーンスケーリング) ==========

static void ApplyDeadzoneScaling(short rawVal, short deadzone, float& outNormalized)
{
    float raw = static_cast<float>(rawVal);
    if (raw < -32767.0f) raw = -32767.0f;

    float dz = static_cast<float>(deadzone);

    if (raw < -dz)
    {
        outNormalized = (raw + dz) / (32767.0f - dz);
    }
    else if (raw > dz)
    {
        outNormalized = (raw - dz) / (32767.0f - dz);
    }
    else
    {
        outNormalized = 0.0f;
    }
}

float InputXInput_GetLeftStickX(int userIndex)
{
    if (!InputXInput_IsConnected(userIndex)) return 0.0f;
    float val = 0.0f;
    ApplyDeadzoneScaling(gCurrState[userIndex].Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, val);
    return val;
}

float InputXInput_GetLeftStickY(int userIndex)
{
    if (!InputXInput_IsConnected(userIndex)) return 0.0f;
    float val = 0.0f;
    ApplyDeadzoneScaling(gCurrState[userIndex].Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, val);
    return val;
}

float InputXInput_GetRightStickX(int userIndex)
{
    if (!InputXInput_IsConnected(userIndex)) return 0.0f;
    float val = 0.0f;
    ApplyDeadzoneScaling(gCurrState[userIndex].Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, val);
    return val;
}

float InputXInput_GetRightStickY(int userIndex)
{
    if (!InputXInput_IsConnected(userIndex)) return 0.0f;
    float val = 0.0f;
    ApplyDeadzoneScaling(gCurrState[userIndex].Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, val);
    return val;
}


// ========== トリガー入力 ==========

float InputXInput_GetLeftTrigger(int userIndex)
{
    if (!InputXInput_IsConnected(userIndex)) return 0.0f;
    BYTE raw = gCurrState[userIndex].Gamepad.bLeftTrigger;
    if (raw < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return 0.0f;
    return static_cast<float>(raw - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) / (255.0f - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}

float InputXInput_GetRightTrigger(int userIndex)
{
    if (!InputXInput_IsConnected(userIndex)) return 0.0f;
    BYTE raw = gCurrState[userIndex].Gamepad.bRightTrigger;
    if (raw < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return 0.0f;
    return static_cast<float>(raw - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) / (255.0f - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}


// ========== 振動制御 ==========

void InputXInput_SetLeftVibration(int userIndex, float level)
{
    if (userIndex < 0 || userIndex >= PAD_MAX) return;
    float val = std::max(0.0f, std::min(1.0f, level));
    gVibration[userIndex].wLeftMotorSpeed = static_cast<WORD>(val * 65535.0f);
}

void InputXInput_SetRightVibration(int userIndex, float level)
{
    if (userIndex < 0 || userIndex >= PAD_MAX) return;
    float val = std::max(0.0f, std::min(1.0f, level));
    gVibration[userIndex].wRightMotorSpeed = static_cast<WORD>(val * 65535.0f);
}