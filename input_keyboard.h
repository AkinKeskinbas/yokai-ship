/*==============================================================================

   キーボード入力ラッパー [input_keyboard.h]
                                                         Author : Akin Keskinbas
                                                         Date   : 2026/7/1
--------------------------------------------------------------------------------
   keyboard.h をラップし、以下の入力状態を提供します。
     - Press   : キーを押し続けている間 true
     - Trigger : キーを押した瞬間だけ true
     - Release : キーを離した瞬間だけ true
     - Repeat  : 押し続けると delaySeconds 秒後から intervalSeconds 秒ごとに true
==============================================================================*/
#ifndef INPUT_KEYBOARD_H
#define INPUT_KEYBOARD_H

#include "keyboard.h"

// キーボードラッパーの初期化
void InputKeyboard_Initialize(void);

// キーボードラッパーの更新（毎フレーム Application_Update で呼ぶ。delta_time は秒単位）
void InputKeyboard_Update(float delta_time);

// キーを押し続けている間 true
bool InputKeyboard_IsPress(Keyboard_Keys key);

// キーを押した瞬間だけ true
bool InputKeyboard_IsTrigger(Keyboard_Keys key);

// キーを離した瞬間だけ true
bool InputKeyboard_IsRelease(Keyboard_Keys key);

// キーリピート（時間指定：秒）
//   最初に delaySeconds 秒押し続けると反応し、
//   その後 intervalSeconds 秒ごとに true を返す
bool InputKeyboard_IsRepeat(Keyboard_Keys key, float delaySeconds = 0.3f, float intervalSeconds = 0.08f);

#endif // INPUT_KEYBOARD_H