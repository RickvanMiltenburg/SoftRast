#include <d3d11.h>
#include <d3dcompiler.h>
#include <Windows.h>
#include <stdint.h>
#include <assert.h>
#include "ProgressDialog.h"
#include <imgui.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#pragma comment ( lib, "d3d11.lib" )
#pragma comment ( lib, "DXGI.lib" )
#pragma comment ( lib, "d3dcompiler.lib" )

HWND MainWindow;

ID3D11RenderTargetView*	D3DBackBufferRenderTarget;
ID3D11Device*			D3DDevice;
ID3D11DeviceContext*	D3DImmediateContext;
ID3D11Texture2D*        D3DBackBuffer;
IDXGIFactory1*			DXGIFactory;
IDXGISwapChain*			DXGISwapChain;
#ifdef _DEBUG
ID3D11Debug* D3DDebug;
#endif

#include "App.h"
App* app;
bool closed = false;

namespace ImImpl
{
	static ID3D10Blob *             g_pVertexShaderBlob = NULL;
	static ID3D11VertexShader*      g_pVertexShader = NULL;
	static ID3D11InputLayout*       g_pInputLayout = NULL;
	static ID3D11Buffer*            g_pVertexConstantBuffer = NULL;
	static ID3D11Buffer*            g_pVB = NULL;

	static ID3D10Blob *             g_pPixelShaderBlob = NULL;
	static ID3D11PixelShader*       g_pPixelShader = NULL;

	static ID3D11SamplerState*      g_pFontSampler = NULL;
	static ID3D11BlendState*        g_blendState = NULL;
	static ID3D11RasterizerState*   g_rasterizerState = NULL;
	static const uint32_t g_MaxVerts = 10000;
	static bool g_Opened = true;

	struct CUSTOMVERTEX
	{
		float        pos[2];
		float        uv[2];
		unsigned int col;
	};

	struct VERTEX_CONSTANT_BUFFER
	{
		float        mvp[4][4];
	};

	static void RenderDrawLists(ImDrawList** const cmd_lists, int cmd_lists_count)
	{
		// Setup orthographic projection matrix into our constant buffer
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			if (D3DImmediateContext->Map(g_pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource) != S_OK)
				return;

			VERTEX_CONSTANT_BUFFER* pConstantBuffer = (VERTEX_CONSTANT_BUFFER*)mappedResource.pData;
			const float L = 0.0f;
			const float R = ImGui::GetIO().DisplaySize.x;
			const float B = ImGui::GetIO().DisplaySize.y;
			const float T = 0.0f;
			const float mvp[4][4] = 
			{
				{ 2.0f/(R-L),   0.0f,           0.0f,       0.0f},
				{ 0.0f,         2.0f/(T-B),     0.0f,       0.0f,},
				{ 0.0f,         0.0f,           0.5f,       0.0f },
				{ (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
			};
			memcpy(&pConstantBuffer->mvp, mvp, sizeof(mvp));
			D3DImmediateContext->Unmap(g_pVertexConstantBuffer, 0);
		}
    
		// Setup viewport
		{
			D3D11_VIEWPORT vp;
			memset(&vp, 0, sizeof(D3D11_VIEWPORT));
			vp.Width = ImGui::GetIO().DisplaySize.x;
			vp.Height = ImGui::GetIO().DisplaySize.y;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = 0;
			vp.TopLeftY = 0;
			D3DImmediateContext->RSSetViewports(1, &vp);
		}

		// Bind shader and vertex buffers
		unsigned int stride = sizeof(CUSTOMVERTEX);
		unsigned int offset = 0;
		D3DImmediateContext->IASetInputLayout(g_pInputLayout);
		D3DImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
		D3DImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
		D3DImmediateContext->VSSetConstantBuffers(0, 1, &g_pVertexConstantBuffer);

		D3DImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);
		D3DImmediateContext->PSSetSamplers(0, 1, &g_pFontSampler);

		// Setup render state
		const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		D3DImmediateContext->OMSetBlendState(g_blendState, blendFactor, 0xffffffff);

		size_t total_vtx_count = 0;
		for (int n = 0; n < cmd_lists_count; n++)
			total_vtx_count += cmd_lists[n]->vtx_buffer.size();
		if (total_vtx_count == 0)
			return;

		// Copy and convert all vertices into a single contiguous buffer
		
		//for (int n = 0; n < cmd_lists_count; n++)
		//{
		//	
		//}
		

		// Render command lists
		for (int n = 0; n < cmd_lists_count; n++)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			if (D3DImmediateContext->Map(g_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource) != S_OK)
				return;
			CUSTOMVERTEX* vtx_dst = (CUSTOMVERTEX*)mappedResource.pData;

			{
				const ImDrawList* cmd_list = cmd_lists[n];
				const ImDrawVert* vtx_src = &cmd_list->vtx_buffer[0];
				assert ( cmd_list->vtx_buffer.size ( ) < g_MaxVerts );
				for (size_t i = 0; i < cmd_list->vtx_buffer.size(); i++)
				{
					vtx_dst->pos[0] = vtx_src->pos.x;
					vtx_dst->pos[1] = vtx_src->pos.y;
					vtx_dst->uv[0] = vtx_src->uv.x;
					vtx_dst->uv[1] = vtx_src->uv.y;
					vtx_dst->col = vtx_src->col;
					vtx_dst++;
					vtx_src++;
				}
			}
			D3DImmediateContext->Unmap(g_pVB, 0);
			D3DImmediateContext->IASetVertexBuffers(0, 1, &g_pVB, &stride, &offset);

			// Render command list
			int vtx_offset = 0;
			const ImDrawList* cmd_list = cmd_lists[n];
			for (size_t cmd_i = 0; cmd_i < cmd_list->commands.size(); cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->commands[cmd_i];
				const D3D11_RECT r = { (LONG)pcmd->clip_rect.x, (LONG)pcmd->clip_rect.y, (LONG)pcmd->clip_rect.z, (LONG)pcmd->clip_rect.w };
				D3DImmediateContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&pcmd->texture_id);
				D3DImmediateContext->RSSetScissorRects(1, &r); 
				D3DImmediateContext->Draw(pcmd->vtx_count, vtx_offset);
				vtx_offset += pcmd->vtx_count;
			}
		}

		// Restore modified state
		D3DImmediateContext->IASetInputLayout(NULL);
		D3DImmediateContext->PSSetShader(NULL, NULL, 0);
		D3DImmediateContext->VSSetShader(NULL, NULL, 0);
		const D3D11_RECT r = { 0, 0, 0, 0 };
		D3DImmediateContext->RSSetScissorRects(0, &r); 
		D3DImmediateContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
	}

	INT64 ticks_per_second = 0;
	INT64 last_time = 0;

	void UpdateImGui()
	{
		ImGuiIO& io = ImGui::GetIO();

		// Setup time step
		INT64 current_time;
		QueryPerformanceCounter((LARGE_INTEGER *)&current_time); 
		io.DeltaTime = (float)(current_time - last_time) / ticks_per_second;
		last_time = current_time;

		// Setup inputs
		// (we already got mouse position, buttons, wheel from the window message callback)
		BYTE keystate[256];
		GetKeyboardState(keystate);
		for (int i = 0; i < 256; i++)
			io.KeysDown[i] = (keystate[i] & 0x80) != 0;
		io.KeyCtrl = (keystate[VK_CONTROL] & 0x80) != 0;
		io.KeyShift = (keystate[VK_SHIFT] & 0x80) != 0;
		// io.MousePos : filled by WM_MOUSEMOVE event
		// io.MouseDown : filled by WM_*BUTTON* events
		// io.MouseWheel : filled by WM_MOUSEWHEEL events

		// Start the frame
		ImGui::NewFrame();
	}

	void LoadFontsTexture()
	{
		// Load one or more font
		ImGuiIO& io = ImGui::GetIO();
		//ImFont* my_font1 = io.Fonts->AddFontDefault();
		//ImFont* my_font2 = io.Fonts->AddFontFromFileTTF("extra_fonts/Karla-Regular.ttf", 15.0f);
		//ImFont* my_font3 = io.Fonts->AddFontFromFileTTF("extra_fonts/ProggyClean.ttf", 13.0f); my_font3->DisplayOffset.y += 1;
		//ImFont* my_font4 = io.Fonts->AddFontFromFileTTF("extra_fonts/ProggyTiny.ttf", 10.0f); my_font4->DisplayOffset.y += 1;
		//ImFont* my_font5 = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 20.0f, io.Fonts->GetGlyphRangesJapanese());

		// Build
		unsigned char* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		// Create texture
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;

		ID3D11Texture2D *pTexture = NULL;
		D3D11_SUBRESOURCE_DATA subResource;
		subResource.pSysMem = pixels;
		subResource.SysMemPitch = desc.Width * 4;
		subResource.SysMemSlicePitch = 0;
		D3DDevice->CreateTexture2D(&desc, &subResource, &pTexture);

		// Create texture view
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		ID3D11ShaderResourceView* font_texture_view = NULL;
		D3DDevice->CreateShaderResourceView(pTexture, &srvDesc, &font_texture_view);
		pTexture->Release();

		// Store our identifier
		io.Fonts->TexID = (void *)font_texture_view;
	}

	void InitImGui()
	{
		if (!QueryPerformanceFrequency((LARGE_INTEGER *)&ticks_per_second))
			return;
		if (!QueryPerformanceCounter((LARGE_INTEGER *)&last_time))
			return;

		RECT rect;
		GetClientRect(MainWindow, &rect);
		int display_w = (int)(rect.right - rect.left);
		int display_h = (int)(rect.bottom - rect.top);

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2((float)display_w, (float)display_h);    // Display size, in pixels. For clamping windows positions.
		io.DeltaTime = 1.0f/60.0f;                                      // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our time step is variable)
		io.KeyMap[ImGuiKey_Tab] = VK_TAB;                               // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
		io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
		io.KeyMap[ImGuiKey_DownArrow] = VK_UP;
		io.KeyMap[ImGuiKey_Home] = VK_HOME;
		io.KeyMap[ImGuiKey_End] = VK_END;
		io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
		io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
		io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
		io.KeyMap[ImGuiKey_A] = 'A';
		io.KeyMap[ImGuiKey_C] = 'C';
		io.KeyMap[ImGuiKey_V] = 'V';
		io.KeyMap[ImGuiKey_X] = 'X';
		io.KeyMap[ImGuiKey_Y] = 'Y';
		io.KeyMap[ImGuiKey_Z] = 'Z';
		io.RenderDrawListsFn = RenderDrawLists;
		io.IniFilename = nullptr;
		assert ( io.RenderDrawListsFn );

		// Create the vertex buffer
		{
			D3D11_BUFFER_DESC bufferDesc;
			memset(&bufferDesc, 0, sizeof(D3D11_BUFFER_DESC));
			bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			bufferDesc.ByteWidth = g_MaxVerts * sizeof(CUSTOMVERTEX);
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			bufferDesc.MiscFlags = 0;
			if (D3DDevice->CreateBuffer(&bufferDesc, NULL, &g_pVB) < 0)
			{
				IM_ASSERT(0);
				return;
			}
		}

		// Load fonts
		LoadFontsTexture();

		// Create texture sampler
		{
			D3D11_SAMPLER_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.MipLODBias = 0.f;
			desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
			desc.MinLOD = 0.f;
			desc.MaxLOD = 0.f;
			D3DDevice->CreateSamplerState(&desc, &g_pFontSampler);
		}

		// Create the vertex shader
		{
			static const char* vertexShader = 
				"cbuffer vertexBuffer : register(c0) \
				{\
					float4x4 ProjectionMatrix; \
				};\
				struct VS_INPUT\
				{\
					float2 pos : POSITION;\
					float4 col : COLOR0;\
					float2 uv  : TEXCOORD0;\
				};\
				\
				struct PS_INPUT\
				{\
					float4 pos : SV_POSITION;\
					float4 col : COLOR0;\
					float2 uv  : TEXCOORD0;\
				};\
				\
				PS_INPUT main(VS_INPUT input)\
				{\
					PS_INPUT output;\
					output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
					output.col = input.col;\
					output.uv  = input.uv;\
					return output;\
				}";

			D3DCompile(vertexShader, strlen(vertexShader), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &g_pVertexShaderBlob, NULL);
			if (g_pVertexShaderBlob == NULL) // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
				return;// E_FAIL;
			if (D3DDevice->CreateVertexShader((DWORD*)g_pVertexShaderBlob->GetBufferPointer(), g_pVertexShaderBlob->GetBufferSize(), NULL, &g_pVertexShader) != S_OK)
				return;// E_FAIL;

			// Create the input layout
			D3D11_INPUT_ELEMENT_DESC localLayout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (size_t)(&((CUSTOMVERTEX*)0)->pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, (size_t)(&((CUSTOMVERTEX*)0)->col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, (size_t)(&((CUSTOMVERTEX*)0)->uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};

			if (D3DDevice->CreateInputLayout(localLayout, 3, g_pVertexShaderBlob->GetBufferPointer(), g_pVertexShaderBlob->GetBufferSize(), &g_pInputLayout) != S_OK)
				return;// E_FAIL;

			// Create the constant buffer
			{
				D3D11_BUFFER_DESC cbDesc;
				cbDesc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER);
				cbDesc.Usage = D3D11_USAGE_DYNAMIC;
				cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				cbDesc.MiscFlags = 0;
				D3DDevice->CreateBuffer(&cbDesc, NULL, &g_pVertexConstantBuffer);
			}
		}

		// Create the pixel shader
		{
			static const char* pixelShader = 
				"struct PS_INPUT\
				{\
					float4 pos : SV_POSITION;\
					float4 col : COLOR0;\
					float2 uv  : TEXCOORD0;\
				};\
				sampler sampler0;\
				Texture2D texture0;\
				\
				float4 main(PS_INPUT input) : SV_Target\
				{\
					float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
					return out_col; \
				}";

			D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &g_pPixelShaderBlob, NULL);
			if (g_pPixelShaderBlob == NULL)  // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
				return;// E_FAIL;
			if (D3DDevice->CreatePixelShader((DWORD*)g_pPixelShaderBlob->GetBufferPointer(), g_pPixelShaderBlob->GetBufferSize(), NULL, &g_pPixelShader) != S_OK)
				return;// E_FAIL;
		}

		// Create the blending setup
		{
			D3D11_BLEND_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.AlphaToCoverageEnable = false;
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			D3DDevice->CreateBlendState(&desc, &g_blendState);
		}

		{
			D3D11_RASTERIZER_DESC rasterizerDesc = {};
			rasterizerDesc.CullMode              = D3D11_CULL_BACK;
			rasterizerDesc.FillMode              = D3D11_FILL_SOLID;
			rasterizerDesc.FrontCounterClockwise = false;
			D3DDevice->CreateRasterizerState ( &rasterizerDesc, &g_rasterizerState );
		}
	}

	void Render ( )
	{
		if ( g_Opened )
		{
			D3DImmediateContext->RSSetState ( g_rasterizerState );
			D3DImmediateContext->OMSetRenderTargets ( 1, &D3DBackBufferRenderTarget, nullptr );
			ImGui::Render ( );
		}
	}
};

LRESULT CALLBACK WndProc ( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
	ImGuiIO& io = ImGui::GetIO ( );

	switch ( message )
	{
	case WM_KEYUP:
		if ( wParam == VK_F12 )
			ImImpl::g_Opened = !ImImpl::g_Opened;
		return true;
	case WM_LBUTTONDOWN:
		io.MouseDown[0] = true;
		return true;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false; 
		return true;
	case WM_RBUTTONDOWN:
		io.MouseDown[1] = true; 
		return true;
	case WM_RBUTTONUP:
		io.MouseDown[1] = false; 
		return true;
	case WM_MOUSEWHEEL:
		io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
		return true;
	case WM_MOUSEMOVE:
		// Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
		io.MousePos.x = (signed short)(lParam);
		io.MousePos.y = (signed short)(lParam >> 16); 
		return true;
	case WM_CHAR:
		// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (wParam > 0 && wParam < 0x10000)
			io.AddInputCharacter((unsigned short)wParam);
		return true;

	case WM_DESTROY:
		{
			//assert ( renderWindow );
			//if ( renderWindow->eventListener )
			//	renderWindow->eventListener ( RenderWindow ( renderWindow ), RenderWindow::Event::Closed );
					
			PostQuitMessage ( 0 );
			closed = true;
		}
		return 0;

	case WM_SYSCOMMAND:
		{
			//assert ( renderWindow );
			switch ( wParam & 0xFFF0 )	// The last byte is used for internal purposes, it seems
			{
			case SC_MINIMIZE:
				//if ( renderWindow->eventListener )
				//	renderWindow->eventListener ( RenderWindow ( renderWindow ), RenderWindow::Event::Suspended );
				break;

			case SC_RESTORE:
				//if ( renderWindow->eventListener )
				//	renderWindow->eventListener ( RenderWindow ( renderWindow ), RenderWindow::Event::Awakened );
				break;
			}
		}
		return DefWindowProc ( window, message, wParam, lParam );

	case WM_SIZE:
		{
			LRESULT result = DefWindowProc ( window, message, wParam, lParam );
			
			RECT rect;
			GetClientRect ( window, &rect );
			uint32_t width  = rect.right  - rect.left;
			uint32_t height = rect.bottom - rect.top;

			ImGui::GetIO ( ).DisplaySize = ImVec2 ( (float)width, (float)height );

			if ( DXGISwapChain )
			{
				D3DBackBuffer->Release ( );
				if ( D3DBackBufferRenderTarget )
					D3DBackBufferRenderTarget->Release ( );

				HRESULT hr = DXGISwapChain->ResizeBuffers ( 1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0 );

				hr = DXGISwapChain->GetBuffer ( 0, __uuidof ( ID3D11Texture2D ), (void**)&D3DBackBuffer );
				assert ( SUCCEEDED ( hr ) );

				hr = D3DDevice->CreateRenderTargetView ( D3DBackBuffer, nullptr, &D3DBackBufferRenderTarget );
				assert ( SUCCEEDED ( hr ) );

				app->OnResize ( width, height );
			}

			return result;
		}

	default:
		return DefWindowProc ( window, message, wParam, lParam );
	}
}

static HRESULT QueryRefreshRate( UINT screenWidth, UINT screenHeight, BOOL vsync, DXGI_RATIONAL* outRate )
{
	DXGI_RATIONAL refreshRate = { 0, 1 };
	if ( vsync )
	{
		IDXGIFactory* factory;
		IDXGIAdapter* adapter;
		IDXGIOutput* adapterOutput;
		DXGI_MODE_DESC* displayModeList;

		// Create a DirectX graphics interface factory.
		HRESULT hr = CreateDXGIFactory( __uuidof(IDXGIFactory), (void**)&factory );
		if ( FAILED(hr) )
		{
			//GRAPHICS::LogError( __func__ ": Could not create DXGIFactory instance." );

			return -1;
		}

		hr = factory->EnumAdapters(0, &adapter );
		if ( FAILED(hr) )
		{
			//GRAPHICS::LogError( __func__ ": Failed to enumerate adapters." );

			return -1;
		}

		hr = adapter->EnumOutputs( 0, &adapterOutput );
		if ( FAILED(hr) )
		{
			//GRAPHICS::LogError( __func__ ": Failed to enumerate adapter outputs." );

			return -1;
		}

		UINT numDisplayModes;
		hr = adapterOutput->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numDisplayModes, nullptr );
		if ( FAILED(hr) )
		{
			//GRAPHICS::LogError( __func__ ": Failed to query display mode list." );

			return -1;
		}

		displayModeList = new DXGI_MODE_DESC[numDisplayModes];
		assert( displayModeList );

		hr = adapterOutput->GetDisplayModeList( DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numDisplayModes, displayModeList );
		if ( FAILED(hr) )
		{
			//GRAPHICS::LogError( __func__ ": Failed to query display mode list." );

			return -1;
		}

		// Now store the refresh rate of the monitor that matches the width and height of the requested screen.
		for ( UINT i = 0; i < numDisplayModes; ++i )
		{
			if ( displayModeList[i].Width == screenWidth && displayModeList[i].Height == screenHeight )
			{
				refreshRate = displayModeList[i].RefreshRate;
			}
		}

		delete [] displayModeList;
		adapterOutput->Release ( );
		adapter->Release ( );
		factory->Release ( );
	}

	*outRate = refreshRate;
	return 0;
}

static HWND _CreateMainWindow ( uint32_t width, uint32_t height )
{
	const char* className = "SoftRast";

	WNDCLASSEX windowClass;
	windowClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowClass.lpfnWndProc   = WndProc;
	windowClass.cbClsExtra    = 0;
	windowClass.cbWndExtra    = 0;
	windowClass.hInstance     = GetModuleHandle ( NULL );
	windowClass.hIcon         = LoadIcon ( NULL, IDI_WINLOGO );
	windowClass.hIconSm       = windowClass.hIcon;
	windowClass.hCursor       = LoadCursor ( NULL, IDC_ARROW );
	windowClass.hbrBackground = (HBRUSH)GetStockObject ( BLACK_BRUSH );
	windowClass.lpszMenuName  = NULL;
	windowClass.lpszClassName = className;
	windowClass.cbSize        = sizeof ( WNDCLASSEX );

	RECT windowRect = { 0, 0, width, height };
	AdjustWindowRect ( &windowRect, WS_OVERLAPPEDWINDOW, FALSE );

	RegisterClassEx ( &windowClass );

	HWND window;
	window = CreateWindowEx ( WS_EX_APPWINDOW, className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, NULL, NULL, GetModuleHandle ( NULL ), nullptr );

	ShowWindow ( window, SW_SHOW );
	SetForegroundWindow ( window );
	SetFocus ( window );
			
	return window;
}

static bool _InitD3D ( uint32_t width, uint32_t height, HWND window )
{
	HRESULT result;
	result = CreateDXGIFactory1 ( __uuidof ( IDXGIFactory1 ), (void**)&DXGIFactory );
	assert ( SUCCEEDED ( result ) );

	uint32_t deviceFlags = 0;
//#ifdef _DEBUG
	//deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
//#endif
			
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };	// Sorry people who have Vista or XP, or a very outdated graphics card!
	result = D3D11CreateDevice ( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, featureLevels, sizeof ( featureLevels ) / sizeof ( featureLevels[0] ), D3D11_SDK_VERSION, &D3DDevice, nullptr, &D3DImmediateContext );
	assert ( SUCCEEDED ( result ) );

	DXGI_SWAP_CHAIN_DESC swapDesc;
	ZeroMemory ( &swapDesc, sizeof ( DXGI_SWAP_CHAIN_DESC ) );

	//////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////
			
	swapDesc.BufferCount       = 1;
	swapDesc.BufferDesc.Width  = width;
	swapDesc.BufferDesc.Height = height;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;	// TODO: Don't hardcode this, doofus
	result                     = QueryRefreshRate( swapDesc.BufferDesc.Width, swapDesc.BufferDesc.Height, FALSE, &swapDesc.BufferDesc.RefreshRate );
	swapDesc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow      = window;
	swapDesc.SampleDesc.Count  = 1;
	swapDesc.SwapEffect        = DXGI_SWAP_EFFECT_SEQUENTIAL;
	swapDesc.Windowed          = TRUE;

	//renderWindow->format = swapDesc.BufferDesc.Forma;t

	result = DXGIFactory->CreateSwapChain ( D3DDevice, &swapDesc, &DXGISwapChain );
	assert ( SUCCEEDED ( result ) );

	result = DXGISwapChain->GetBuffer ( 0, __uuidof ( ID3D11Texture2D ), (void**)&D3DBackBuffer );
	assert ( SUCCEEDED ( result ) );

	//////////////////////////////////////
	// Create render target
	//////////////////////////////////////
	result = D3DDevice->CreateRenderTargetView ( D3DBackBuffer, nullptr, &D3DBackBufferRenderTarget );
	assert ( SUCCEEDED ( result ) );

#ifdef _DEBUG
	D3DDevice->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&D3DDebug));
#endif

	return true;
}

int main ( int argc, char* argv[] )
{
	MainWindow   = _CreateMainWindow ( SCREEN_WIDTH, SCREEN_HEIGHT );
	bool success = _InitD3D ( SCREEN_WIDTH, SCREEN_HEIGHT, MainWindow );
	assert ( success );

	ImImpl::InitImGui ( );

	ProgressDialog::SetDialogParameters ( true, 1.0f, "Initializing..." );
	app = new App ( D3DDevice, D3DImmediateContext, &D3DBackBufferRenderTarget, &D3DBackBuffer );
	app->OnResize ( SCREEN_WIDTH, SCREEN_HEIGHT );

	LARGE_INTEGER prevCounter, counterFrequency;
	QueryPerformanceFrequency ( &counterFrequency );
	QueryPerformanceCounter ( &prevCounter );

	float deltaTime = 0.0f;

	ProgressDialog::SetDialogParameters ( false, 0.0f, nullptr );
	while ( !closed )
	{
		MSG msg;
		ZeroMemory ( &msg, sizeof ( MSG ) );

		while ( PeekMessage ( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage ( &msg );
			DispatchMessage ( &msg );
		}
		ImImpl::UpdateImGui ( );

		app->Tick ( deltaTime, 0.0f );

		ImImpl::Render ( );

		HRESULT result = DXGISwapChain->Present ( 0, 0 );
		if ( FAILED ( result ) )
		{
			__debugbreak ( );
			//switch ( result )
			//{
			//case DXGI_ERROR_DEVICE_REMOVED:
			//case DXGI_ERROR_DEVICE_RESET:
			//case DXGI_ERROR_DEVICE_HUNG:
			//case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			//
			//
			//}
		}

		LARGE_INTEGER curCounter;
		QueryPerformanceCounter ( &curCounter );

		deltaTime   = float ( double ( curCounter.QuadPart - prevCounter.QuadPart ) / double ( counterFrequency.QuadPart ) );
		prevCounter = curCounter;
	}

	ImGui::Shutdown ( );

	delete app;

#ifdef _DEBUG
	if ( D3DDebug )
		D3DDebug->ReportLiveDeviceObjects( D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL );
#endif

	return EXIT_SUCCESS;
}