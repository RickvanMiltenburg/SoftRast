#include "App.h"
#include <stdio.h>
#include <chrono>
#include <imgui.h>

#include "SoftwareRasterizer\BarebonesMath\include\bbm.h"
#include "SoftwareRasterizer\softrast.h"
#include "movement\CameraMovement.h"
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <algorithm>
#include <vector>
#include <memory>

buddy_allocator alloc;
softrast_model model;
bool modelLoaded = false;

CameraMovement Movement;
double totalDTSeconds;
uint32_t totalFrameCount;

struct blah
{
	//--------------------------------
	// Limits
	//--------------------------------
	const float NEAR_CLIP_MIN            = 0.1f;
	const float NEAR_CLIP_MAX            = 18000.0f;
	
	const float FAR_CLIP_MIN             = 1.0f;
	const float FAR_CLIP_MAX             = 20000.0f;
	const float FAR_CLIP_MIN_NEAR_FACTOR = 1.1f;

	const float FOV_MIN = 10.0f;
	const float FOV_MAX = 170.0f;

	const float MIN_MOVEMENT_SPEED = 0.1f;
	const float MAX_MOVEMENT_SPEED = 50.0f;

	const float MIN_ROT_SPEED = 45.0f;
	const float MAX_ROT_SPEED = 360.0f;

	//--------------------------------
	// Variables
	//--------------------------------
	glm::vec3 position = glm::vec3 ( 0.0f, 0.0f, 100.0f );
	float pitch = 0.0f, yaw = 0.0f;	// Degrees
	float fov = 90.0f;	// Degrees
	float nearClip = 0.1f, farClip = 2000.0f;
	float movSpeed = 20.0f, rotSpeed = 180.0f;
} Camera;

extern "C" {
	extern DEBUG_SETTINGS Debug;
};

static const std::string SceneRoot = "assets/";
std::vector<std::string> Scenes;
static int SceneIndex = -1;
struct
{
	uint32_t totalVertCount;
	uint32_t totalTriCount;
	std::vector<ID3D11ShaderResourceView*> textures;
} SceneData;

App::App ( ID3D11Device* device, ID3D11DeviceContext* deviceContext, ID3D11RenderTargetView** backBuffer, ID3D11Texture2D** backBufferTexture )
	: m_Device ( device ), m_DeviceContext ( deviceContext ), m_BackBuffer ( backBuffer ), m_BackBufferTexture ( backBufferTexture )
{
	//--------------------------------
	// Initialize allocator
	//--------------------------------
	static const uint32_t bufferSize = 512*1024*1024;
	static const uint32_t poolSize   = 64*1024;
	uint8_t* buffer            = new uint8_t[bufferSize];
	uint8_t* bookkeepingBuffer = new uint8_t[miltyalloc_buddy_allocator_calculate_bookkeeping_size ( bufferSize, poolSize )];
	miltyalloc_buddy_allocator_initialize ( &alloc, buffer, bufferSize, poolSize, bookkeepingBuffer );

	//--------------------------------
	// Initialize softrast
	//--------------------------------
	softrast_initialize ( &alloc );

	//--------------------------------
	// DEBUG: DEFAULT SETTINGS
	//--------------------------------
	Debug.flags                 = FLAG_BACKFACE_CULLING_ENABLED | FLAG_DEPTH_TESTING | FLAG_CLIP_W | FLAG_CLIP_FRUSTUM | FLAG_ENABLE_QUAD_RASTERIZATION | FLAG_FILTER_LUT | /*FLAG_AABB_FRUSTUM_CHECK |*/ FLAG_FILL_OUTLINES | FLAG_RASTERIZE;
	Debug.renderMode            = RENDER_MODE_TEXTURED;
	Debug.textureAddressingMode = TEXTURE_ADDRESSING_LINEAR;
	Debug.textureFilteringMode  = TEXTURE_FILTERING_BILINEAR;
	Debug.textureMipmapMode     = TEXTURE_MIPMAP_LINEAR;
	Debug.lodBias               = 0.0f;
	Debug.lodScale              = 0.75f;
	Debug.clipBorderDist        = 1.0f;

	////--------------------------------
	//// DEBUG: Load model by default
	////--------------------------------
	//LoadModel ( "assets/sponza.ogex" );
	//LoadModel ( "assets/texcoob.ogex" );

	//--------------------------------
	// Fill array of scenes
	//--------------------------------
	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFileExA ( (SceneRoot + "*.ogex").c_str ( ), FindExInfoStandard, &findData, FindExSearchNameMatch, nullptr, 0 );
	while ( hFind != INVALID_HANDLE_VALUE )
	{
		Scenes.push_back ( findData.cFileName );

		if ( !FindNextFileA ( hFind, &findData ) )
		{
			FindClose ( hFind );
			hFind = INVALID_HANDLE_VALUE;
		}
	}
}

App::~App ( )
{
	delete m_IntermediateRenderTarget;
}

bool App::LoadModel ( const char* path )
{
	if ( modelLoaded )
	{
		softrast_model_free ( &model );
		modelLoaded = false;
	}
	
	uint32_t res = softrast_model_load ( &model, path );
	assert ( res == 0 );

	if ( res != 0 )
		return false;

	SceneData.totalVertCount = 0;
	SceneData.totalTriCount  = 0;
	for ( uint32_t i = 0; i < model.meshCount; i++ )
	{
		SceneData.totalVertCount += model.meshes[i].positions.vectorCount;

		for ( uint32_t j = 0; j < model.meshes[i].submeshCount; j++ )
		{
			SceneData.totalTriCount += model.meshes[i].submeshes[j].indexCount / 3;
		}
	}

	for ( uint32_t i = 0; i < model.textureCount; i++ )
	{
		// Create texture
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = model.textures[i].width;
		desc.Height = model.textures[i].height;
		desc.MipLevels = model.textures[i].mipLevels;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;

		std::unique_ptr<D3D11_SUBRESOURCE_DATA> mipLevels ( new D3D11_SUBRESOURCE_DATA[model.textures[i].mipLevels] );
		D3D11_SUBRESOURCE_DATA* mipPtr = mipLevels.get ( );
		for ( uint32_t j = 0; j < model.textures[i].mipLevels; j++ )
		{
			mipPtr[j].pSysMem          = model.textures[i].mipData[j];
			mipPtr[j].SysMemPitch      = (desc.Width * 4) >> j;
			mipPtr[j].SysMemSlicePitch = 0;
		}

		ID3D11Texture2D *pTexture = NULL;
		//D3D11_SUBRESOURCE_DATA subResource;
		//subResource.pSysMem = model.textures[i].mipData[0];
		//subResource.SysMemPitch = desc.Width * 4;
		//subResource.SysMemSlicePitch = 0;
		m_Device->CreateTexture2D(&desc, mipPtr, &pTexture);

		// Create texture view
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		ID3D11ShaderResourceView* font_texture_view = NULL;
		m_Device->CreateShaderResourceView(pTexture, &srvDesc, &font_texture_view);
		pTexture->Release();

		SceneData.textures.push_back ( font_texture_view );
	}

	modelLoaded = true;
	totalDTSeconds = 0.0f;
	totalFrameCount = 0;

	return true;
}

void UpdateProjectionMatrix ( uint32_t width, uint32_t height )
{
	glm::mat4 projMat = glm::perspective ( glm::radians ( Camera.fov ), (float)width / (float)height, Camera.nearClip, Camera.farClip );
	bbm_aos_mat4 bbmProjMat;
	memcpy ( bbmProjMat.cells, glm::value_ptr ( projMat ), sizeof ( bbmProjMat.cells ) );
	softrast_set_projection_matrix ( &bbmProjMat );
}

void App::OnResize ( uint32_t width, uint32_t height )
{
	//--------------------------------
	// Create new render target
	//--------------------------------
	delete m_IntermediateRenderTarget;
	m_IntermediateRenderTarget = RenderTarget::Create ( width, height );
	m_ScreenWidth = width, m_ScreenHeight = height;

	//--------------------------------
	// Set projection matrix
	//--------------------------------
	UpdateProjectionMatrix ( width, height );
}


void App::GUITick ( float cpuDeltaTimeSeconds, float gpuDeltaTimeSeconds )
{
	totalDTSeconds += cpuDeltaTimeSeconds;
	totalFrameCount++;

	bool updateProjMat = false;

	ImGui::SetNextWindowPos ( ImVec2 ( 10, 10 ) );
	ImGui::Begin ( "Stuff", nullptr, ImVec2 ( 0, 0 ), 0.8f, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize );
		if ( ImGui::CollapsingHeader ( "Statistics", nullptr, true, true ) )
		{
			ImGui::TreePush ( "Statistics" );
				static const int offset = 170;
				ImGui::Text ( "FPS:" );
				ImGui::SameLine ( offset );
				ImGui::Text ( "%.02f", 1.0f/cpuDeltaTimeSeconds );
				ImGui::Text ( "Frame time:" );
				ImGui::SameLine ( offset );
				ImGui::Text ( "%.02f ms", cpuDeltaTimeSeconds * 1000.0f, (totalDTSeconds / totalFrameCount) * 1000.0, totalFrameCount );
				ImGui::Text ( "Avg frame time:" );
				ImGui::SameLine ( offset );
				ImGui::Text ( "%.02f ms", (totalDTSeconds / totalFrameCount) * 1000.0 );
				ImGui::Text ( "Avg frame count:" );
				ImGui::SameLine ( offset );
				ImGui::Text ( "%u", totalFrameCount );
				if ( ImGui::Button ( "Reset averages" ) )
					totalDTSeconds = 0.0f, totalFrameCount = 0;
			ImGui::TreePop ( );
		}
		if ( ImGui::CollapsingHeader ( "Debug", nullptr, true, false ) )
		{
			ImGui::Indent ( );
				ImGui::CheckboxFlags ( "AABB frustum culling",      &Debug.flags, FLAG_AABB_FRUSTUM_CHECK        );
				ImGui::CheckboxFlags ( "Fill outlines",             &Debug.flags, FLAG_FILL_OUTLINES        );

				if ( Debug.flags & FLAG_FILL_OUTLINES )
				{
					ImGui::Indent ( );
					ImGui::CheckboxFlags ( "Backface culling enabled",  &Debug.flags, FLAG_BACKFACE_CULLING_ENABLED  );
					if ( Debug.flags & FLAG_BACKFACE_CULLING_ENABLED )
					{
						ImGui::Indent ( );
						ImGui::CheckboxFlags ( "Backface culling inverted", &Debug.flags, FLAG_BACKFACE_CULLING_INVERTED );
						ImGui::Unindent ( );
					}
					ImGui::CheckboxFlags ( "Depth buffer enabled",      &Debug.flags, FLAG_DEPTH_TESTING             );
				
					ImGui::CheckboxFlags ( "W clip enabled",            &Debug.flags, FLAG_CLIP_W                    );
					ImGui::CheckboxFlags ( "Frustum clip enabled",      &Debug.flags, FLAG_CLIP_FRUSTUM              );
					if ( Debug.flags & FLAG_CLIP_FRUSTUM )
					{
						ImGui::Indent ( );
						ImGui::SliderFloat ( "Border", &Debug.clipBorderDist, 1.0f, 30.0f, "%.2f px" );
						ImGui::Unindent ( );
					}
					ImGui::CheckboxFlags ( "Rasterize",      &Debug.flags, FLAG_RASTERIZE              );
					if ( Debug.flags & FLAG_RASTERIZE )
					{
						ImGui::Indent ( );
						ImGui::CheckboxFlags ( "Enable quad rasterization", &Debug.flags, FLAG_ENABLE_QUAD_RASTERIZATION );
						if ( Debug.flags & FLAG_ENABLE_QUAD_RASTERIZATION )
						{
							ImGui::Indent ( );
							ImGui::CheckboxFlags ( "SSE", &Debug.flags, FLAG_QUAD_RASTERIZATION_SIMD );
							ImGui::Unindent ( );
						}

						//ImGui::CheckboxFlags ( "Temp derp", &Debug.flags, FLAG_DERP );
						//ImGui::CheckboxFlags ( "Temp derp 2", &Debug.flags, FLAG_DERP2 );
						//ImGui::CheckboxFlags ( "Temp derp 3", &Debug.flags, FLAG_DERP3 );

						ImGui::Combo ( "Render mode", &Debug.renderMode, RenderModes, sizeof ( RenderModes ) / sizeof ( RenderModes[0] ) );
						if ( Debug.renderMode == RENDER_MODE_TEXTURED || Debug.renderMode == RENDER_MODE_MIPMAP )
						{
							ImGui::Indent ( );
							ImGui::CheckboxFlags ( "Enable dithering", &Debug.flags, FLAG_TEXTURE_DITHERING );
					
							int prevTextureMode = Debug.textureAddressingMode;
							if ( ImGui::Combo ( "Texture addressing", &Debug.textureAddressingMode, TextureAddressingModes, sizeof ( TextureAddressingModes ) / sizeof ( TextureAddressingModes[0] ) ) )
							{
								LoadModel ( (SceneRoot + Scenes[SceneIndex]).c_str ( ) );
							}
							if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_SWIZZLED )
							{
								ImGui::Indent ( );
								ImGui::CheckboxFlags ( "Use lookup table (LUT)", &Debug.flags, FLAG_FILTER_LUT );
								ImGui::Unindent ( );
							}

							ImGui::Combo ( "Texture filtering", &Debug.textureFilteringMode, TextureFilteringModes, sizeof ( TextureFilteringModes ) / sizeof ( TextureFilteringModes[0] ) );

							if ( Debug.flags & FLAG_ENABLE_QUAD_RASTERIZATION )
							{
								ImGui::Combo ( "Texture mipmap", &Debug.textureMipmapMode, TextureMipmapModes, sizeof ( TextureMipmapModes ) / sizeof ( TextureMipmapModes[0] ) );
								if ( Debug.textureMipmapMode != TEXTURE_MIPMAP_NONE )
								{
									ImGui::Indent ( );
									ImGui::InputFloat ( "LOD bias", &Debug.lodBias );
									ImGui::InputFloat ( "LOD scale", &Debug.lodScale );
									ImGui::Unindent ( );
								}
							}
							ImGui::Unindent ( );
						}
						ImGui::Unindent ( );
					}
					ImGui::Unindent ( );
				}
			ImGui::Unindent ( );
		}
		if ( ImGui::CollapsingHeader ( "Scene", nullptr, true, true ) )
		{
			ImGui::TreePush ( "Scene" );
				if ( ImGui::Combo ( "Scene", &SceneIndex, [&] ( void* data, int idx, const char** out_text ) -> bool { if ( idx < 0 || idx >= (int)Scenes.size ( ) ) return false; *out_text = Scenes[idx].c_str ( ); return true; }, nullptr, (int)Scenes.size ( ) ) )
				{
					LoadModel ( (SceneRoot + Scenes[SceneIndex]).c_str ( ) );
				}
			
				ImGui::Separator ( );

				static const int offset = 120;
				ImGui::Text ( "Meshes:" );
				ImGui::SameLine ( offset );
				ImGui::Text ( "%u", model.meshCount );
				ImGui::Text ( "Vertices:" );
				ImGui::SameLine ( offset );
				ImGui::Text ( "%u", SceneData.totalVertCount );
				ImGui::Text ( "Triangles:" );
				ImGui::SameLine ( offset );
				ImGui::Text ( "%u", SceneData.totalTriCount );

				ImGui::BeginChild ( "submeshes", ImVec2 ( 0, 200 ), false );
					if ( ImGui::TreeNode ( (const void*)model.meshes, "Meshes" ) )
					{
						for ( uint32_t i = 0; i < model.meshCount; i++ )
						{
							if ( ImGui::TreeNode ( (const void*)&(model.meshes[i]), "Mesh %u", i ) )
							{
								ImGui::Text ( "Submeshes:" );
								ImGui::SameLine ( offset + 0 );
								ImGui::Text ( "%u", model.meshes[i].submeshCount );
								ImGui::Text ( "Vertices:" );
								ImGui::SameLine ( offset + 0 );
								ImGui::Text ( "%u", model.meshes[i].positions.vectorCount );

								for ( uint32_t j = 0; j < model.meshes[i].submeshCount; j++ )
								{
									if ( ImGui::TreeNode ( (const void*)&(model.meshes[i].submeshes[j]), "Submesh %u", j ) )
									{
										ImGui::Text ( "Triangles:" );
										ImGui::SameLine ( offset + 20 );
										ImGui::Text ( "%u", model.meshes[i].submeshes[j].indexCount / 3 );
										ImGui::Text ( "Texture:" );
										ImGui::SameLine ( offset + 20 );
										ImGui::Text ( model.meshes[i].submeshes[j].texture ? model.meshes[i].submeshes[j].texture->path : "<none>" );

										if ( model.meshes[i].submeshes[j].texture )
										{
											ImGui::Image ( (ImTextureID)SceneData.textures[model.meshes[i].submeshes[j].texture - model.textures], ImVec2 ( 256, 256 ) );
										}
									
										ImGui::TreePop ( );
									}
								}

								ImGui::TreePop ( );
							}
						}
						ImGui::TreePop ( );
					}
					if ( ImGui::TreeNode ( (const void*)model.textures, "Textures" ) )
					{
						for ( uint32_t i = 0; i < model.textureCount; i++ )
						{
							if ( ImGui::TreeNode ( (const void*)&(model.textures[i]), "Texture %u", i ) )
							{
								ImGui::Text ( "Path:" );
								ImGui::SameLine ( offset + 25 );
								ImGui::Text ( "%s", model.textures[i].path );
								ImGui::Text ( "Resolution:" );
								ImGui::SameLine ( offset + 25 );
								ImGui::Text ( "%ux%u", model.textures[i].width, model.textures[i].height );
								ImGui::Text ( "Mipmap count:" );
								ImGui::SameLine ( offset + 25 );
								ImGui::Text ( "%u", model.textures[i].mipLevels );

								ImGui::Image ( (ImTextureID)SceneData.textures[i], ImVec2 ( 1024, 1024 ) );
								ImGui::TreePop ( );
							}
						}

						ImGui::TreePop ( );
					}
				ImGui::EndChild ( );
			ImGui::TreePop ( );
		}
		if ( ImGui::CollapsingHeader ( "Camera", nullptr, true, false ) )
		{
			ImGui::TreePush ( "Camera" );
				ImGui::InputFloat3 ( "Position", glm::value_ptr ( Camera.position ), 2 );
				ImGui::Columns ( 2, nullptr, false );
					ImGui::InputFloat ( "Pitch", &Camera.pitch, 2 );
					ImGui::NextColumn ( );
					ImGui::InputFloat ( "Yaw", &Camera.yaw, 2 );
				ImGui::Columns ( );
				updateProjMat |= ImGui::SliderFloat ( "FOV", &Camera.fov, Camera.FOV_MIN, Camera.FOV_MAX );
			
				ImGui::Columns ( 2, nullptr, false );
					updateProjMat |=  ImGui::SliderFloat ( "Near clip", &Camera.nearClip, Camera.NEAR_CLIP_MIN, Camera.NEAR_CLIP_MAX );
					Camera.farClip = std::max ( Camera.nearClip * Camera.FAR_CLIP_MIN_NEAR_FACTOR, Camera.farClip );
					ImGui::NextColumn ( );
					updateProjMat |=  ImGui::SliderFloat ( "Far clip", &Camera.farClip, Camera.nearClip * Camera.FAR_CLIP_MIN_NEAR_FACTOR, Camera.FAR_CLIP_MAX );
				ImGui::Columns ( );

				ImGui::Columns ( 2, nullptr, false );
					updateProjMat |=  ImGui::SliderFloat ( "Move speed", &Camera.movSpeed, Camera.MIN_MOVEMENT_SPEED, Camera.MAX_MOVEMENT_SPEED );
					ImGui::NextColumn ( );
					updateProjMat |=  ImGui::SliderFloat ( "Rot speed", &Camera.rotSpeed, Camera.MIN_ROT_SPEED, Camera.MAX_ROT_SPEED );
				ImGui::Columns ( );
			ImGui::TreePop ( );
		}
	ImGui::End ( );

	if ( updateProjMat )
		UpdateProjectionMatrix ( m_ScreenWidth, m_ScreenHeight );
}

void App::Tick ( float cpuDeltaTimeSeconds, float gpuDeltaTimeSeconds )
{
	GUITick ( cpuDeltaTimeSeconds, gpuDeltaTimeSeconds );

	///////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////

	Movement.Update ( );
	glm::vec3 movementVector   = Movement.GetInputMovementVector ( ) * Camera.movSpeed * cpuDeltaTimeSeconds;
	glm::vec2 movementPitchYaw = Movement.GetInputPitchYaw ( ) * Camera.rotSpeed * cpuDeltaTimeSeconds;
	bool moved = glm::length ( movementVector ) > 0.0f || glm::length ( movementPitchYaw ) > 0.0f;

	Camera.pitch += movementPitchYaw.x;
	Camera.yaw   += movementPitchYaw.y;
	
	glm::mat4 rotMat = glm::rotate ( glm::mat4 ( ), glm::radians ( Camera.yaw ), glm::vec3 ( 0.0f, 1.0f, 0.0f ) ) * glm::rotate ( glm::mat4 ( ), glm::radians ( Camera.pitch ), glm::vec3 ( 1.0f, 0.0f, 0.0f ) );
	glm::vec3 fwdVec = glm::vec3 ( rotMat * glm::vec4 ( movementVector, 0.0f ) );
	
	Camera.position = Camera.position + fwdVec;

	glm::mat4 viewMat = glm::inverse ( glm::translate ( glm::mat4 ( ), Camera.position ) * rotMat );

	bbm_aos_mat4 bbmViewMat;
	memcpy ( bbmViewMat.cells, glm::value_ptr ( viewMat ), sizeof ( bbmViewMat.cells ) );
	softrast_set_view_matrix ( &bbmViewMat );

	///////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////

	D3D11_MAPPED_SUBRESOURCE msr;
	HRESULT res = m_DeviceContext->Map ( m_IntermediateRenderTarget->GetTexture ( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr );
	assert ( SUCCEEDED ( res ) );
	//assert ( msr.RowPitch == m_ScreenWidth * sizeof ( uint32_t ) );

	softrast_set_render_target ( m_ScreenWidth, m_ScreenHeight, (uint32_t*)msr.pData, msr.RowPitch );
	softrast_clear_render_target ( );
	softrast_clear_depth_render_target ( );
	softrast_render ( &model );

	m_DeviceContext->Unmap ( m_IntermediateRenderTarget->GetTexture ( ), 0 );
	m_DeviceContext->CopyResource ( *m_BackBufferTexture, m_IntermediateRenderTarget->GetTexture ( ) );
}