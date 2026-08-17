/*==============================================================================

   VF[_[ [shader.cpp]
														 Author : Akin Keskinbas
														 Date   : 2026/07/01
--------------------------------------------------------------------------------

==============================================================================*/
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
#include "direct3d.h"
#include "debug_ostream.h"
#include <fstream>


static ID3D11VertexShader* g_pVertexShader = nullptr;
static ID3D11InputLayout* g_pInputLayout = nullptr;
static ID3D11Buffer* g_pVSConstantBuffer = nullptr;
static ID3D11PixelShader* g_pPixelShader = nullptr;

// ���ӁI�������ŊO������ݒ肳����́BRelease�s�v�B
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;


bool Shader_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	HRESULT hr; // �߂�l�i�[�p

	// �f�o�C�X�ƃf�o�C�X�R���e�L�X�g�̃`�F�b�N
	if (!pDevice || !pContext) {
		hal::dout << "Shader_Initialize() : �^����ꂽ�f�o�C�X���R���e�L�X�g���s���ł�" << std::endl;
		return false;
	}

	// �f�o�C�X�ƃf�o�C�X�R���e�L�X�g�̕ۑ�
	g_pDevice = pDevice;
	g_pContext = pContext;


	// ORpCςݒ_VF[_[̓ǂݍ
	std::ifstream ifs_vs("asset/shader/shader_vertex_2d.cso", std::ios::binary);

	if (!ifs_vs) {
		MessageBox(nullptr, "_VF[_[̓ǂݍ݂Ɏs܂\n\nasset/shader/shader_vertex_2d.cso", "G[", MB_OK);
		return false;
	}

	// t@CTCY擾
	ifs_vs.seekg(0, std::ios::end); // t@C|C^𖖔Ɉړ
	std::streamsize filesize = ifs_vs.tellg(); // t@C|C^̈ʒu擾i‚܂t@CTCYj
	ifs_vs.seekg(0, std::ios::beg); // t@C|C^擪ɖ߂

	// oCif[^i[邽߂̃obt@m
	unsigned char* vsbinary_pointer = new unsigned char[filesize];

	ifs_vs.read((char*)vsbinary_pointer, filesize); // oCif[^ǂݍ
	ifs_vs.close(); // t@C‚

	// _VF[_[̍쐬
	hr = g_pDevice->CreateVertexShader(vsbinary_pointer, filesize, nullptr, &g_pVertexShader);

	if (FAILED(hr)) {
		hal::dout << "Shader_Initialize() : _VF[_[̍쐬Ɏs܂" << std::endl;
		delete[] vsbinary_pointer; // [NȂ悤ɃoCif[^̃obt@
		return false;
	}


	// _CAEg̒`
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT num_elements = ARRAYSIZE(layout); // z̗vf擾

	// _CAEg̍쐬
	hr = g_pDevice->CreateInputLayout(layout, num_elements, vsbinary_pointer, filesize, &g_pInputLayout);

	delete[] vsbinary_pointer; // oCif[^̃obt@

	if (FAILED(hr)) {
		hal::dout << "Shader_Initialize() : _CAEg̍쐬Ɏs܂" << std::endl;
		return false;
	}


	// _VF[_[p萔obt@̍쐬
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4X4); // obt@̃TCY
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // oChtO

	HRESULT hr_cb = g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer);
	if (FAILED(hr_cb)) {
		MessageBox(nullptr, "Shader_Initialize: CreateBuffer for VSConstantBuffer failed!", "Error", MB_OK | MB_ICONERROR);
		return false;
	}


	// ORpCς݃sNZVF[_[̓ǂݍ
	std::ifstream ifs_ps("asset/shader/shader_pixel_2d.cso", std::ios::binary);
	if (!ifs_ps) {
		MessageBox(nullptr, "sNZVF[_[̓ǂݍ݂Ɏs܂\n\nasset/shader/shader_pixel_2d.cso", "G[", MB_OK);
		return false;
	}

	ifs_ps.seekg(0, std::ios::end);
	filesize = ifs_ps.tellg();
	ifs_ps.seekg(0, std::ios::beg);

	unsigned char* psbinary_pointer = new unsigned char[filesize];
	ifs_ps.read((char*)psbinary_pointer, filesize);
	ifs_ps.close();

	// �s�N�Z���V�F�[�_�[�̍쐬
	hr = g_pDevice->CreatePixelShader(psbinary_pointer, filesize, nullptr, &g_pPixelShader);

	delete[] psbinary_pointer; // �o�C�i���f�[�^�̃o�b�t�@����

	if (FAILED(hr)) {
		hal::dout << "Shader_Initialize() : �s�N�Z���V�F�[�_�[�̍쐬�Ɏ��s���܂���" << std::endl;
		return false;
	}

	return true;
}

void Shader_Finalize()
{
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pVSConstantBuffer);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pVertexShader);
}

void Shader_SetMatrix(const DirectX::XMMATRIX& matrix)
{
	if (!g_pContext) {
		MessageBox(nullptr, "Shader_SetMatrix: g_pContext is nullptr!", "Fatal Error", MB_OK | MB_ICONERROR);
		return;
	}
	if (!g_pVSConstantBuffer) {
		MessageBox(nullptr, "Shader_SetMatrix: g_pVSConstantBuffer is nullptr!", "Fatal Error", MB_OK | MB_ICONERROR);
		return;
	}

	// 萔obt@i[ps̍\̂`
	XMFLOAT4X4 transpose;

	// s]uĒ萔obt@i[psɕϊ
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));

	// 萔obt@ɍsZbg
	g_pContext->UpdateSubresource(g_pVSConstantBuffer, 0, nullptr, &transpose, 0, 0);
}

void Shader_Begin()
{
	// _VF[_[ƃsNZVF[_[`pCvCɐݒ
	g_pContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pContext->PSSetShader(g_pPixelShader, nullptr, 0);

	// ���_���C�A�E�g��`��p�C�v���C���ɐݒ�
	g_pContext->IASetInputLayout(g_pInputLayout);

	// �萔�o�b�t�@��`��p�C�v���C���ɐݒ�
	g_pContext->VSSetConstantBuffers(0, 1, &g_pVSConstantBuffer);
}
