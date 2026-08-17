/*==============================================================================

   マウス入力ラッパー [input_mouse.h]
                                                         Author : Akin Keskinbas
                                                         Date   : 2026/7/1
--------------------------------------------------------------------------------
   mouse.h をラップし、以下の入力状態を提供します。
     - Press   : ボタンを押し続けている間 true
     - Trigger : ボタンを押した瞬間だけ true
     - Release : ボタンを離した瞬間だけ true
==============================================================================*/
#ifndef INPUT_MOUSE_H
#define INPUT_MOUSE_H

#include <Windows.h>
#include "mouse.h" // Mouse_PositionMode 等の型を使用するため

// マウスボタン番号
enum MouseButton
{
    MOUSE_BUTTON_LEFT   = 0,
    MOUSE_BUTTON_RIGHT  = 1,
    MOUSE_BUTTON_MIDDLE = 2,
};

// マウスラッパーの初期化
void InputMouse_Initialize(HWND hWnd);

// マウスラッパーの更新（毎フレーム Application_Update の先頭で呼ぶ）
void InputMouse_Update(void);

// マウスラッパーの終了処理
void InputMouse_Finalize(void);

// ボタンを押し続けている間 true
bool InputMouse_IsPress(MouseButton button);

// ボタンを押した瞬間だけ true
bool InputMouse_IsTrigger(MouseButton button);

// ボタンを離した瞬間だけ true
bool InputMouse_IsRelease(MouseButton button);

// マウスの現在座標（または相対移動量）を取得
int InputMouse_GetX(void);
int InputMouse_GetY(void);

// スクロールホイールの値を取得
int InputMouse_GetScrollWheel(void);

// マウスの動作モードを設定 (絶対 / 相対)
void InputMouse_SetMode(Mouse_PositionMode mode);

// マウスカーソルの表示 / 非表示を設定
void InputMouse_SetVisible(bool visible);

#endif // INPUT_MOUSE_H