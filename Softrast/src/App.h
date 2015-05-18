#pragma once

#include <stdint.h>
#include <d3d11.h>

#include "RenderTarget.h"

class App
{
public:
	App		( ID3D11Device* device, ID3D11DeviceContext* deviceContext, ID3D11RenderTargetView** backBuffer, ID3D11Texture2D** backBufferTexture );
	~App	( );

	bool	LoadModel	( const char* path );

	void	OnResize	( uint32_t width, uint32_t height );

	void	GUITick	( float cpuDeltaTimeSeconds, float gpuDeltaTimeSeconds );
	void	Tick	( float cpuDeltaTimeSeconds, float gpuDeltaTimeSeconds );

private:
	ID3D11Device*				m_Device;
	ID3D11DeviceContext*		m_DeviceContext;
	ID3D11RenderTargetView**	m_BackBuffer;
	ID3D11Texture2D**           m_BackBufferTexture;
	ID3D11VertexShader*			m_FinalOutputVertexShader;
	ID3D11PixelShader*			m_FinalOutputPixelShader;

	ID3D11SamplerState*	m_Sampler;
	ID3D11Buffer*		m_IndexBuffer;

	RenderTarget*	m_IntermediateRenderTarget = nullptr;
	DepthRenderTarget*	m_DepthRenderTarget;
	uint32_t m_ScreenWidth, m_ScreenHeight;
};