/*==============================================================================
   オーディオ管理 [audio.h]
                                                         Author : Akin Keskinbas
                                                         Date   : 2026/7/1
==============================================================================*/
#pragma once

void InitAudio();
void UninitAudio();

int LoadAudio(const char* FileName);
void UnloadAudio(int Index);
void PlayAudio(int Index, bool Loop = false);
void SetAudioVolume(int Index, float Volume);
void StopAudio(int Index);
