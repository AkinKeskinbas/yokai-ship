/*==============================================================================
   オーディオ管理 [audio.cpp]
                                                          Author : Akin Keskinbas
                                                          Date   : 2026/7/1
==============================================================================*/
#include <windows.h>
#include <mmsystem.h>
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <assert.h>
#include <vector>
#include "audio.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

static IXAudio2* g_Xaudio{};
static IXAudio2MasteringVoice* g_MasteringVoice{};

void InitAudio()
{
	MFStartup(MF_VERSION);
	// XAudio    
	XAudio2Create(&g_Xaudio, 0);

	//  } X ^     O { C X    
	g_Xaudio->CreateMasteringVoice(&g_MasteringVoice);
}

void UninitAudio()
{
	for (int i = 0; i < 100; i++)
	{
		UnloadAudio(i);
	}
	if (g_MasteringVoice)
	{
		g_MasteringVoice->DestroyVoice();
		g_MasteringVoice = nullptr;
	}
	if (g_Xaudio)
	{
		g_Xaudio->Release();
		g_Xaudio = nullptr;
	}
	MFShutdown();
}

struct AUDIO
{
	IXAudio2SourceVoice*	SourceVoice{};
	BYTE*					SoundData{};

	int						Length{};
	int						PlayLength{};
};

#define AUDIO_MAX 100
static AUDIO g_Audio[AUDIO_MAX]{};

int LoadAudio(const char *FileName)
{
	int index = -1;

	for (int i = 0; i < AUDIO_MAX; i++)
	{
		if (g_Audio[i].SourceVoice == nullptr)
		{
			index = i;
			break;
		}
	}

	if (index == -1)
		return -1;

	wchar_t wPath[MAX_PATH] = { 0 };
	MultiByteToWideChar(CP_ACP, 0, FileName, -1, wPath, MAX_PATH);

	// 1. Try Loading via Windows Media Foundation (Supports WAV, MPEG, MP3, etc.)
	IMFSourceReader* pReader = nullptr;
	HRESULT hr = MFCreateSourceReaderFromURL(wPath, nullptr, &pReader);
	if (SUCCEEDED(hr) && pReader)
	{
		IMFMediaType* pPartialType = nullptr;
		MFCreateMediaType(&pPartialType);
		pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
		pPartialType->Release();

		if (SUCCEEDED(hr))
		{
			IMFMediaType* pUncompressedType = nullptr;
			hr = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pUncompressedType);
			if (SUCCEEDED(hr) && pUncompressedType)
			{
				WAVEFORMATEX* pWfx = nullptr;
				UINT32 cbFormat = 0;
				hr = MFCreateWaveFormatExFromMFMediaType(pUncompressedType, &pWfx, &cbFormat);
				pUncompressedType->Release();

				if (SUCCEEDED(hr) && pWfx)
				{
					std::vector<BYTE> audioData;
					while (true)
					{
						IMFSample* pSample = nullptr;
						DWORD flags = 0;
						hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &pSample);
						if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
						{
							if (pSample) pSample->Release();
							break;
						}
						if (pSample)
						{
							IMFMediaBuffer* pBuffer = nullptr;
							hr = pSample->ConvertToContiguousBuffer(&pBuffer);
							if (SUCCEEDED(hr) && pBuffer)
							{
								BYTE* pAudioBytes = nullptr;
								DWORD cbBuffer = 0;
								hr = pBuffer->Lock(&pAudioBytes, nullptr, &cbBuffer);
								if (SUCCEEDED(hr))
								{
									audioData.insert(audioData.end(), pAudioBytes, pAudioBytes + cbBuffer);
									pBuffer->Unlock();
								}
								pBuffer->Release();
							}
							pSample->Release();
						}
					}
					pReader->Release();

					if (!audioData.empty() && pWfx->nBlockAlign > 0)
					{
						g_Audio[index].Length = (int)audioData.size();
						g_Audio[index].SoundData = new unsigned char[audioData.size()];
						memcpy(g_Audio[index].SoundData, audioData.data(), audioData.size());
						g_Audio[index].PlayLength = (int)audioData.size() / pWfx->nBlockAlign;

						hr = g_Xaudio->CreateSourceVoice(&g_Audio[index].SourceVoice, pWfx);
						CoTaskMemFree(pWfx);

						if (SUCCEEDED(hr))
						{
							return index;
						}
						delete[] g_Audio[index].SoundData;
						g_Audio[index].SoundData = nullptr;
						return -1;
					}
					CoTaskMemFree(pWfx);
				}
			}
		}
		pReader->Release();
	}

	// 2. Fallback to mmio for standard WAV
	WAVEFORMATEX wfx = { 0 };
	{
		HMMIO hmmio = NULL;
		MMIOINFO mmioinfo = { 0 };
		MMCKINFO riffchunkinfo = { 0 };
		MMCKINFO datachunkinfo = { 0 };
		MMCKINFO mmckinfo = { 0 };
		UINT32 buflen;
		LONG readlen;

		hmmio = mmioOpenA((LPSTR)FileName, &mmioinfo, MMIO_READ);
		if (hmmio == NULL)
		{
			return -1;
		}

		riffchunkinfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');
		mmioDescend(hmmio, &riffchunkinfo, NULL, MMIO_FINDRIFF);

		mmckinfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
		mmioDescend(hmmio, &mmckinfo, &riffchunkinfo, MMIO_FINDCHUNK);

		if (mmckinfo.cksize >= sizeof(WAVEFORMATEX))
		{
			mmioRead(hmmio, (HPSTR)&wfx, sizeof(wfx));
		}
		else
		{
			PCMWAVEFORMAT pcmwf = { 0 };
			mmioRead(hmmio, (HPSTR)&pcmwf, sizeof(pcmwf));
			memset(&wfx, 0x00, sizeof(wfx));
			memcpy(&wfx, &pcmwf, sizeof(pcmwf));
			wfx.cbSize = 0;
		}
		mmioAscend(hmmio, &mmckinfo, 0);

		datachunkinfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
		mmioDescend(hmmio, &datachunkinfo, &riffchunkinfo, MMIO_FINDCHUNK);

		buflen = datachunkinfo.cksize;
		g_Audio[index].SoundData = new unsigned char[buflen];
		readlen = mmioRead(hmmio, (HPSTR)g_Audio[index].SoundData, buflen);

		g_Audio[index].Length = readlen;

		if (wfx.nBlockAlign == 0)
		{
			mmioClose(hmmio, 0);
			delete[] g_Audio[index].SoundData;
			g_Audio[index].SoundData = nullptr;
			return -1;
		}

		g_Audio[index].PlayLength = readlen / wfx.nBlockAlign;

		mmioClose(hmmio, 0);
	}

	hr = g_Xaudio->CreateSourceVoice(&g_Audio[index].SourceVoice, &wfx);
	if (FAILED(hr))
	{
		delete[] g_Audio[index].SoundData;
		g_Audio[index].SoundData = nullptr;
		return -1;
	}

	return index;
}

void UnloadAudio(int Index)
{
	if (Index < 0 || Index >= AUDIO_MAX || g_Audio[Index].SourceVoice == nullptr)
		return;

	g_Audio[Index].SourceVoice->Stop();
	g_Audio[Index].SourceVoice->DestroyVoice();

	delete[] g_Audio[Index].SoundData;
	g_Audio[Index].SoundData = nullptr;
	g_Audio[Index].SourceVoice = nullptr;
}

void PlayAudio(int Index, bool Loop)
{
	if (Index < 0 || Index >= AUDIO_MAX || g_Audio[Index].SourceVoice == nullptr)
		return;

	g_Audio[Index].SourceVoice->Stop();
	g_Audio[Index].SourceVoice->FlushSourceBuffers();

	//  o b t @ ݒ 
	XAUDIO2_BUFFER bufinfo;

	memset(&bufinfo, 0x00, sizeof(bufinfo));
	bufinfo.AudioBytes = g_Audio[Index].Length;
	bufinfo.pAudioData = g_Audio[Index].SoundData;
	bufinfo.PlayBegin = 0;
	bufinfo.PlayLength = g_Audio[Index].PlayLength;

	//    [ v ݒ 
	if (Loop)
	{
		bufinfo.LoopBegin = 0;
		bufinfo.LoopLength = g_Audio[Index].PlayLength;
		bufinfo.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	g_Audio[Index].SourceVoice->SubmitSourceBuffer(&bufinfo, NULL);

	//  Đ 
	g_Audio[Index].SourceVoice->Start();
}

void SetAudioVolume(int Index, float Volume)
{
	if (Index < 0 || Index >= AUDIO_MAX || g_Audio[Index].SourceVoice == nullptr)
		return;
	g_Audio[Index].SourceVoice->SetVolume(Volume);
}

void StopAudio(int Index)
{
	if (Index < 0 || Index >= AUDIO_MAX || g_Audio[Index].SourceVoice == nullptr)
		return;
	g_Audio[Index].SourceVoice->Stop();
	g_Audio[Index].SourceVoice->FlushSourceBuffers();
}

