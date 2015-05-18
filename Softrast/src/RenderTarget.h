#pragma once

#include <d3d11.h>
#include <stdint.h>

class RenderTarget
{
	RenderTarget ( );
public:
	~RenderTarget	( );

	static RenderTarget*	Create	( uint32_t width, uint32_t height );

	uint32_t	GetWidth	( );
	uint32_t	GetHeight	( );

	ID3D11Texture2D*			GetTexture				( );
	//ID3D11RenderTargetView*		GetRenderTargetView		( );
	ID3D11ShaderResourceView*	GetShaderResourceView	( );
	//ID3D11UnorderedAccessView*	GetUnorderedAccessView	( );

	void	Resize	( uint32_t width, uint32_t height );

private:
	uint32_t					m_Width = 0, m_Height = 0;

	ID3D11Texture2D*			m_Tex = nullptr;
	//ID3D11RenderTargetView*		m_RTV = nullptr;
	ID3D11ShaderResourceView*	m_SRV = nullptr;
	//ID3D11UnorderedAccessView*	m_UAV = nullptr;
};

class DepthRenderTarget
{
	DepthRenderTarget ( );
public:
	~DepthRenderTarget	( );

	static DepthRenderTarget*	Create	( uint32_t width, uint32_t height );

	uint32_t	GetWidth	( );
	uint32_t	GetHeight	( );

	ID3D11Texture2D*			GetTexture				( );
	ID3D11DepthStencilView*		GetDepthStencilView		( );
	ID3D11ShaderResourceView*	GetShaderResourceView	( );

	void	Resize	( uint32_t width, uint32_t height );

private:
	uint32_t					m_Width = 0, m_Height = 0;

	ID3D11Texture2D*			m_Tex = nullptr;
	ID3D11DepthStencilView*		m_DSV = nullptr;
	ID3D11ShaderResourceView*	m_SRV = nullptr;
};