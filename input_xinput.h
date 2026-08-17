/*==============================================================================

   XInput ゲームパッド入力ラッパー [input_xinput.h]
                                                         Author : Akin Keskinbas
                                                         Date   : 2026/7/1
--------------------------------------------------------------------------------
   XInput をラップし、ゲームパッドの接続状態、ボタン入力状態、
   およびデッドゾーン処理済みの正規化スティック/トリガー値、振動制御を提供します。
==============================================================================*/
#ifndef INPUT_XINPUT_H
#define INPUT_XINPUT_H

#include <Windows.h>
#include <Xinput.h>

// XInputラッパーの初期化
void InputXInput_Initialize(void);

// XInputラッパーの更新（毎フレーム Application_Update の先頭で呼ぶ。delta_time は秒単位）
void InputXInput_Update(float delta_time);

// コントローラーが接続されているか
bool InputXInput_IsConnected(int userIndex);

// ボタンを押し続けている間 true
// buttonBitMask: XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_DPAD_UP などのマスク値
bool InputXInput_IsPress(int userIndex, WORD buttonBitMask);

// ボタンを押した瞬間だけ true
bool InputXInput_IsTrigger(int userIndex, WORD buttonBitMask);

// ボタンを離した瞬間だけ true
bool InputXInput_IsRelease(int userIndex, WORD buttonBitMask);

// ボタンリピート（時間指定：秒）
bool InputXInput_IsRepeat(int userIndex, WORD buttonBitMask, float delaySeconds = 0.3f, float intervalSeconds = 0.08f);

// 左右アナログスティックの軸指定取得 (デッドゾーン処理済みの -1.0f - 1.0f の正規化値)
float InputXInput_GetLeftStickX(int userIndex);
float InputXInput_GetLeftStickY(int userIndex);
float InputXInput_GetRightStickX(int userIndex);
float InputXInput_GetRightStickY(int userIndex);

// 左トリガーのアナログ値を取得 (0.0f - 1.0f の範囲に正規化)
float InputXInput_GetLeftTrigger(int userIndex);

// 右トリガーのアナログ値を取得 (0.0f - 1.0f の範囲に正規化)
float InputXInput_GetRightTrigger(int userIndex);

// モーター振動の設定 (モーター出力レベル: 0.0f - 1.0f、Update時に自動でコントローラーへ適用)
void InputXInput_SetLeftVibration(int userIndex, float level);
void InputXInput_SetRightVibration(int userIndex, float level);

#endif // INPUT_XINPUT_H