#include "RenderTarget.h"

#include <assert.h>

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

#define SAFE_RELEASE(x) { if ( x ) (x)->Release ( ); (x) = nullptr; }

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

extern ID3D11Device* D3DDevice;

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

RenderTarget::RenderTarget ( )
{

}

RenderTarget::~RenderTarget	( )
{
	//////////////////////////////////////
	// Release all resources (if any)
	//////////////////////////////////////
	SAFE_RELEASE(m_SRV);
	//SAFE_RELEASE(m_UAV);
	//SAFE_RELEASE(m_RTV);
	SAFE_RELEASE(m_Tex);
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

RenderTarget* RenderTarget::Create ( uint32_t width, uint32_t height )
{
	auto rt = new RenderTarget ( );
	rt->Resize ( width, height );
	return rt;
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

uint32_t RenderTarget::GetWidth ( )
{
	return m_Width;
}

uint32_t RenderTarget::GetHeight ( )
{
	return m_Height;
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

ID3D11Texture2D* RenderTarget::GetTexture ( )
{
	return m_Tex;
}

//ID3D11RenderTargetView* RenderTarget::GetRenderTargetView ( )
//{
//	return m_RTV;
//}

ID3D11ShaderResourceView* RenderTarget::GetShaderResourceView ( )
{
	return m_SRV;
}

//ID3D11UnorderedAccessView* RenderTarget::GetUnorderedAccessView ( )
//{
//	return m_UAV;
//}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

void RenderTarget::Resize ( uint32_t width, uint32_t height )
{
	//////////////////////////////////////
	// Release previous resources (if any)
	//////////////////////////////////////
	SAFE_RELEASE(m_SRV);
	//SAFE_RELEASE(m_UAV);
	//SAFE_RELEASE(m_RTV);
	SAFE_RELEASE(m_Tex);

	//////////////////////////////////////
	// Create texture
	//////////////////////////////////////
	D3D11_TEXTURE2D_DESC textureDesc = {};

	textureDesc.Width              = width;
	textureDesc.Height             = height;
	textureDesc.MipLevels          = 1;
	textureDesc.ArraySize          = 1;
	textureDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;//DXGI_FORMAT_R32G32B32A32_FLOAT;
	textureDesc.SampleDesc.Count   = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage              = D3D11_USAGE_DYNAMIC;//D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;//D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	textureDesc.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;//0;
	textureDesc.MiscFlags          = 0;
		
	HRESULT result = D3DDevice->CreateTexture2D ( &textureDesc, nullptr, &m_Tex );
	assert ( SUCCEEDED ( result ) );

	m_Width  = width;
	m_Height = height;

	//////////////////////////////////////
	// Create render target
	//////////////////////////////////////
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;//DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels       = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	//D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	//uavDesc.Format                    = DXGI_FORMAT_R32G32B32A32_FLOAT;
	//uavDesc.ViewDimension             = D3D11_UAV_DIMENSION_TEXTURE2D;
	//uavDesc.Texture2D.MipSlice        = 0;

	result = D3DDevice->CreateShaderResourceView ( m_Tex, &srvDesc, &m_SRV );
	assert ( SUCCEEDED ( result ) );

	//result = D3DDevice->CreateUnorderedAccessView ( m_Tex, &uavDesc, &m_UAV );
	//assert ( SUCCEEDED ( result ) );
	//
	//result = D3DDevice->CreateRenderTargetView ( m_Tex, nullptr, &m_RTV );
	//assert ( SUCCEEDED ( result ) );
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

DepthRenderTarget::DepthRenderTarget ( )
{

}

DepthRenderTarget::~DepthRenderTarget ( )
{
	//////////////////////////////////////
	// Release all resources (if any)
	//////////////////////////////////////
	SAFE_RELEASE(m_SRV);
	SAFE_RELEASE(m_DSV);
	SAFE_RELEASE(m_Tex);
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

DepthRenderTarget* DepthRenderTarget::Create ( uint32_t width, uint32_t height )
{
	auto rt = new DepthRenderTarget ( );
	rt->Resize ( width, height );
	return rt;
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

uint32_t DepthRenderTarget::GetWidth ( )
{
	return m_Width;
}

uint32_t DepthRenderTarget::GetHeight ( )
{
	return m_Height;
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

ID3D11Texture2D* DepthRenderTarget::GetTexture ( )
{
	return m_Tex;
}

ID3D11DepthStencilView* DepthRenderTarget::GetDepthStencilView ( )
{
	return m_DSV;
}

ID3D11ShaderResourceView* DepthRenderTarget::GetShaderResourceView ( )
{
	return m_SRV;
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

void DepthRenderTarget::Resize ( uint32_t width, uint32_t height )
{
	//////////////////////////////////////
	// Release previous resources (if any)
	//////////////////////////////////////
	SAFE_RELEASE(m_SRV);
	SAFE_RELEASE(m_DSV);
	SAFE_RELEASE(m_Tex);

	//////////////////////////////////////
	// Create texture
	//////////////////////////////////////
	D3D11_TEXTURE2D_DESC textureDesc = {};

	textureDesc.Width              = width;
	textureDesc.Height             = height;
	textureDesc.MipLevels          = 1;
	textureDesc.ArraySize          = 1;
	textureDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
	textureDesc.SampleDesc.Count   = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage              = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags     = 0;
	textureDesc.MiscFlags          = 0;
		
	HRESULT result = D3DDevice->CreateTexture2D ( &textureDesc, nullptr, &m_Tex );
	assert ( SUCCEEDED ( result ) );

	m_Width  = width;
	m_Height = height;

	//////////////////////////////////////
	// Create render target
	//////////////////////////////////////
	D3D11_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
	dsDesc.Format        = DXGI_FORMAT_D32_FLOAT;
	dsDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                    = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels       = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	result = D3DDevice->CreateShaderResourceView ( m_Tex, &srvDesc, &m_SRV );
	assert ( SUCCEEDED ( result ) );

	result = D3DDevice->CreateDepthStencilView ( m_Tex, &dsDesc, &m_DSV );
	assert ( SUCCEEDED ( result ) );
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////