/*
	SoftRast software rasterizer, by Rick van Miltenburg

	Software rasterizer created for fun and educational purposes.

	---

	Copyright (c) 2015 Rick van Miltenburg

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "softrast.h"
#include "../MemoryMappedFile.h"
#include <OpenGEX.h>
#include <assert.h>
#include <unordered_map>
#include <string>
#include <algorithm>
#include "../ProgressDialog.h"

#define VECTOR_WIDTH 8//BAREBONES_MATH_VECTOR_WIDTH

struct OGEXData
{
	const char* filePath   = nullptr;
	uint32_t baseDirLength = 0;

	uint32_t meshCount    = 0;
	uint32_t subMeshCount = 0;
	uint32_t vertexCount  = 0;
	uint32_t indexCount   = 0;
	std::unordered_map<std::string,softrast_texture*> uniqueTextures;

	float *px, *py, *pz;
	float *ptx, *pty, *ptz, *ptw;
	float *nx, *ny, *nz;
	float *tx, *ty;
	uint32_t* indices;
	softrast_mesh* mesh;
	softrast_submesh* submesh;
};

static std::string _FormatTexturePath ( OGEXData* data, const char* path )
{
	return std::string ( data->filePath, data->baseDirLength ) + path;
}

static void _InsertTexturePath ( OGEXData* data, const char* path )
{
	// TODO: Ensure path is correct and all that
	data->uniqueTextures.insert ( { _FormatTexturePath ( data, path ), nullptr } );
}

static void _ScanOGEXGeometryNode ( OGEXData* data, const OGEX::GeometryNodeStructure* structure )
{
	data->meshCount++;

	const OGEX::GeometryObjectStructure* object = nullptr;
	auto subNode = structure->GetFirstSubnode ( );
	while ( subNode )
	{
		switch ( subNode->GetStructureType ( ) )
		{
		case OGEX::kStructureObjectRef:
			object = static_cast<const OGEX::GeometryObjectStructure*> ( static_cast<const OGEX::ObjectRefStructure*> ( subNode )->GetTargetStructure ( ) );
			break;
		case OGEX::kStructureMaterialRef:
			{
				const OGEX::MaterialStructure* mat = static_cast<const OGEX::MaterialStructure*> ( static_cast<const OGEX::MaterialRefStructure*> ( subNode )->GetTargetStructure ( ) );
				auto subNode2 = mat->GetFirstSubnode ( );
				while ( subNode2 )
				{
					switch ( subNode2->GetStructureType ( ) )
					{
					case OGEX::kStructureTexture:
						{
							auto texture = static_cast<const OGEX::TextureStructure*> ( subNode2 );
							auto attrib  = texture->GetAttribString ( );
							auto texPath = texture->GetTextureName ( );

							if ( attrib == "diffuse" )
							{
								_InsertTexturePath ( data, texPath );
							}
						}
						break;
					}

					subNode2 = subNode2->Next ( );
				}
			}
			break;
		}
		subNode = subNode->Next ( );
	}

	assert ( object );

	subNode = object->GetFirstSubnode ( );
	while ( subNode )
	{
		switch ( subNode->GetStructureType ( ) )
		{
		case OGEX::kStructureMesh:
			{
				auto meshNode    = static_cast<const OGEX::MeshStructure*> ( subNode );
				auto meshSubNode = meshNode->GetFirstSubnode ( );
					
				// Cannot directly request primitive; Let's just assume triangles for now

				uint32_t vertexCount   = 0;
				uint32_t triangleCount = 0;

				while ( meshSubNode )
				{
					switch ( meshSubNode->GetStructureType ( ) )
					{
					case OGEX::kStructureVertexArray:
						{
							auto vertexArray = static_cast<const OGEX::VertexArrayStructure*> ( meshSubNode );
							auto vertexData  = static_cast<const ODDL::DataStructure<ODDL::FloatDataType>*> ( vertexArray->GetFirstSubnode ( ) );

							assert ( vertexCount == 0 || vertexCount == vertexData->GetDataElementCount ( ) / vertexData->GetArraySize ( ) );

							vertexCount = vertexData->GetDataElementCount ( ) / vertexData->GetArraySize ( );
						}
						break;

					case OGEX::kStructureIndexArray:
						{
							auto indexArray = static_cast<const OGEX::IndexArrayStructure*> ( meshSubNode );
							auto indexData  = static_cast<const ODDL::DataStructure<ODDL::UnsignedInt32DataType>*> ( indexArray->GetFirstSubnode ( ) );

							assert ( indexData->GetArraySize ( ) == 3 );

							triangleCount += indexData->GetDataElementCount ( ) / 3;
							data->subMeshCount++;
						}
						break;
					}

					meshSubNode = meshSubNode->Next ( );
				}

				uint32_t alignedVertexCount = (vertexCount + (VECTOR_WIDTH-1)) & ~(VECTOR_WIDTH-1);
					
				data->vertexCount += alignedVertexCount;
				data->indexCount  += triangleCount * 3;
			}
			break;
		}

		subNode = subNode->Next ( );
	}

		
}

static void _LoadOGEXGeometryNode ( OGEXData* data, const OGEX::GeometryNodeStructure* structure )
{
	softrast_mesh* mesh = data->mesh++;
	mesh->submeshes = data->submesh;

	const OGEX::GeometryObjectStructure* object = nullptr;
	auto subNode = structure->GetFirstSubnode ( );
	while ( subNode )
	{
		switch ( subNode->GetStructureType ( ) )
		{
		case OGEX::kStructureObjectRef:
			object = static_cast<const OGEX::GeometryObjectStructure*> ( static_cast<const OGEX::ObjectRefStructure*> ( subNode )->GetTargetStructure ( ) );
			break;
		}
		subNode = subNode->Next ( );
	}

	assert ( object );

	subNode = object->GetFirstSubnode ( );
	while ( subNode )
	{
		switch ( subNode->GetStructureType ( ) )
		{
		case OGEX::kStructureMesh:
			{
				auto meshNode    = static_cast<const OGEX::MeshStructure*> ( subNode );
				auto meshSubNode = meshNode->GetFirstSubnode ( );
					
				// Cannot directly request primitive; Let's just assume triangles for now

				uint32_t vertexCount   = 0;
				uint32_t triangleCount = 0;
					
				mesh->submeshCount = 0;

				while ( meshSubNode )
				{
					switch ( meshSubNode->GetStructureType ( ) )
					{
					case OGEX::kStructureVertexArray:
						{
							auto vertexArray = static_cast<const OGEX::VertexArrayStructure*> ( meshSubNode );
							auto vertexData  = static_cast<const ODDL::DataStructure<ODDL::FloatDataType>*> ( vertexArray->GetFirstSubnode ( ) );

							assert ( vertexCount == 0 || vertexCount == vertexData->GetDataElementCount ( ) / vertexData->GetArraySize ( ) );

							vertexCount = vertexData->GetDataElementCount ( ) / vertexData->GetArraySize ( );
							uint32_t vertexCountAligned = vertexCount + (VECTOR_WIDTH-1) & ~(VECTOR_WIDTH-1);

							auto attrib  = vertexArray->GetArrayAttribute ( );
							if ( attrib == "position" )
							{
								float *px = data->px, *py = data->py, *pz = data->pz;
								bbm_soa_vec3_init ( &mesh->positions, px, py, pz, vertexCount );
								
								for ( uint32_t i = 0; i < vertexCount; i++ )
								{
									const float* d = vertexData->GetArrayDataElement ( i );
									*(px++) = d[0];
									*(py++) = d[1];
									*(pz++) = d[2];
								}

								bbm_soa_vec3_min_xyz ( &mesh->aabbMin, &mesh->positions );
								bbm_soa_vec3_max_xyz ( &mesh->aabbMax, &mesh->positions );

								data->px += vertexCountAligned, data->py += vertexCountAligned, data->pz += vertexCountAligned;

								bbm_soa_vec4_init ( &mesh->transformedPositions, data->ptx, data->pty, data->ptz, data->ptw, vertexCount );

								data->ptx += vertexCountAligned, data->pty += vertexCountAligned, data->ptz += vertexCountAligned, data->ptw += vertexCountAligned;
							}
							else if ( attrib == "normal" )
							{
								float *nx = data->nx, *ny = data->ny, *nz = data->nz;
								bbm_soa_vec3_init ( &mesh->normals, nx, ny, nz, vertexCount );
								
								for ( uint32_t i = 0; i < vertexCount; i++ )
								{
									const float* d = vertexData->GetArrayDataElement ( i );
									*(nx++) = d[0];
									*(ny++) = d[1];
									*(nz++) = d[2];
								}

								data->nx += vertexCountAligned, data->ny += vertexCountAligned, data->nz += vertexCountAligned;
							}
							else if ( attrib == "texcoord" )
							{
								float *tx = data->tx, *ty = data->ty;
								bbm_soa_vec2_init ( &mesh->texcoords, tx, ty, vertexCount );
								
								for ( uint32_t i = 0; i < vertexCount; i++ )
								{
									const float* d = vertexData->GetArrayDataElement ( i );
									*(tx++) = 100.0f + d[0];
									*(ty++) = 100.0f - d[1];
								}

								data->tx += vertexCountAligned, data->ty += vertexCountAligned;
							}
						}
						break;

					case OGEX::kStructureIndexArray:
						{
							auto indexArray = static_cast<const OGEX::IndexArrayStructure*> ( meshSubNode );
							auto matIndex = indexArray->GetMaterialIndex ( );
							auto indexData  = static_cast<const ODDL::DataStructure<ODDL::UnsignedInt32DataType>*> ( indexArray->GetFirstSubnode ( ) );

							assert ( indexData->GetArraySize ( ) == 3 );

							softrast_submesh* submesh = data->submesh++;
							submesh->texture    = nullptr;
							submesh->indexCount = indexData->GetDataElementCount ( );
							submesh->indices    = data->indices;
							memcpy ( submesh->indices, indexData->GetArrayDataElement ( 0 ), submesh->indexCount * sizeof ( uint32_t ) );
							data->indices += submesh->indexCount;
							mesh->submeshCount++;


							auto subNode = structure->GetFirstSubnode ( );
							while ( subNode )
							{
								switch ( subNode->GetStructureType ( ) )
								{
								case OGEX::kStructureMaterialRef:
									auto a = static_cast<const OGEX::MaterialRefStructure*> ( subNode );
									if ( a->GetMaterialIndex ( ) == matIndex )
									{
										auto mat = a->GetTargetStructure ( );
										auto subNode2 = mat->GetFirstSubnode ( );
										while ( subNode2 )
										{
											switch ( subNode2->GetStructureType ( ) )
											{
											case OGEX::kStructureTexture:
												{
													auto texture = static_cast<const OGEX::TextureStructure*> ( subNode2 );
													auto attrib  = texture->GetAttribString ( );
													auto texPath = texture->GetTextureName ( );

													if ( attrib == "diffuse" )
													{
														auto it = data->uniqueTextures.find ( _FormatTexturePath ( data, texPath ) );
														assert ( it != data->uniqueTextures.end ( ) );
														submesh->texture = it->second;
													}
												}
												break;
											}

											subNode2 = subNode2->Next ( );
										}
									}
										
									break;
								}
								subNode = subNode->Next ( );
							}
						}
						break;
					}

					meshSubNode = meshSubNode->Next ( );
				}

				uint32_t alignedVertexCount = (vertexCount + (BAREBONES_MATH_VECTOR_WIDTH-1) / BAREBONES_MATH_VECTOR_WIDTH );
			}
			break;
		}

		subNode = subNode->Next ( );
	}
}

// Uh, yeah, well. I prefer this over having to deal with all returns right now. I am aware it is not consistent.
class ProgressDialogAutoCloser
{
public:
	~ProgressDialogAutoCloser ( ) { ProgressDialog::SetDialogParameters ( false, 0.0f, nullptr ); }
};

extern "C" {
	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	extern buddy_allocator* _softrastAllocator;

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	uint32_t softrast_model_load ( softrast_model* model, const char* path )
	{
		OGEXData data;
		char sprintfBuffer[2048];

		//--------------------------------
		// Create progress dialog, and make sure it closes when the function finishes
		//--------------------------------
		ProgressDialogAutoCloser __ProgressDialogShouldCloseAutomaticallyUponGoingOutOfScope;
		sprintf ( sprintfBuffer, "Loading model %s...\nOpening...", path );
		ProgressDialog::SetDialogParameters ( true, 0.0f, sprintfBuffer );

		//--------------------------------
		// Load file
		//--------------------------------
		MemoryMappedFile file ( path );
		if ( !file.IsValid ( ) )
			return -1; // Could not open file

		//--------------------------------
		// Load as OpenGEX file
		//--------------------------------
		OGEX::OpenGexDataDescription desc;
		if ( desc.ProcessText ( (const char*)file.GetData ( ) ) != ODDL::kDataOkay )
			return -2; // Unable to parse OpenGEX file

		//--------------------------------
		// Determine base path
		//--------------------------------
		const char* lastDirSplit = std::max ( strrchr ( path, '/' ), strrchr ( path, '\\' ) );
		size_t fileFolderLength = strlen ( path );
		if ( lastDirSplit )
			fileFolderLength = lastDirSplit - path + 1;
		data.filePath      = path;
		data.baseDirLength = (uint32_t)fileFolderLength;

		//--------------------------------
		// Scan file
		//--------------------------------
		sprintf ( sprintfBuffer, "Loading model %s...\nScanning file content...", path );
		ProgressDialog::SetDialogParameters ( true, 0.1f, sprintfBuffer );
		const ODDL::Structure *structure = desc.GetRootStructure ( )->GetFirstSubnode ( );
		while ( structure )
		{
			switch ( structure->GetStructureType ( ) )
			{
			case OGEX::kStructureGeometryNode:
				_ScanOGEXGeometryNode ( &data, static_cast<const OGEX::GeometryNodeStructure*> ( structure ) );
				break;
			}

			structure = structure->Next ( );
		}

		//--------------------------------
		// Textures
		//--------------------------------
		uint32_t texturePathBytes = 0;
		for ( auto& uniqueTexture : data.uniqueTextures )
		{
			texturePathBytes += (uint32_t)uniqueTexture.first.size ( ) + 1;
		}

		//--------------------------------
		// Allocate memory
		//--------------------------------
		sprintf ( sprintfBuffer, "Loading model %s...\nAllocating memory...", path );
		ProgressDialog::SetDialogParameters ( true, 0.2f, sprintfBuffer );
		static const uint32_t vertexSize            = (3+4+3+2) * sizeof ( float );
		static const uint64_t vertexAlignmentBuffer = ((VECTOR_WIDTH) * sizeof ( float ))-1;
		data.vertexCount = (data.vertexCount + VECTOR_WIDTH) & (~(VECTOR_WIDTH-1));
		uint32_t vertexDataSize = data.vertexCount * vertexSize + vertexAlignmentBuffer;
		uint32_t size = (uint32_t)(data.meshCount * sizeof ( softrast_mesh ) + data.subMeshCount * sizeof ( softrast_submesh ) + data.uniqueTextures.size ( ) * sizeof ( softrast_texture ) + data.indexCount * sizeof ( uint32_t ) + vertexDataSize + texturePathBytes);
		void* memory = miltyalloc_buddy_allocator_alloc ( _softrastAllocator, size );
		if ( memory == nullptr )
			return -3; // Unable to allocate memory

		//--------------------------------
		// Subdivide memory for as much as we are able to at this point
		//--------------------------------
		uintptr_t ptr = (uintptr_t)memory;
		model->meshes = (softrast_mesh*)ptr;
		model->meshCount = data.meshCount;
		data.mesh = model->meshes;
		ptr += model->meshCount * sizeof ( softrast_mesh );

		data.submesh = (softrast_submesh*)ptr;
		ptr += data.subMeshCount * sizeof ( softrast_submesh );

		model->textures = (softrast_texture*)ptr;
		model->textureCount = (uint32_t)data.uniqueTextures.size ( );
		ptr += data.uniqueTextures.size ( ) * sizeof ( softrast_texture );

		data.indices = (uint32_t*)ptr;
		ptr += data.indexCount * sizeof ( uint32_t );

		float* vertexDataPtr = (float*)((ptr + vertexAlignmentBuffer) & (~vertexAlignmentBuffer));
		ptr += vertexDataSize;

		char* stringDataPtr = (char*)ptr;
		ptr += texturePathBytes;

		//--------------------------------
		// Sanity check
		//--------------------------------
		assert ( (((uintptr_t)vertexDataPtr) & (((VECTOR_WIDTH) * sizeof ( float ))-1)) == 0 );	// Not 16-byte aligned!
		assert ( ptr == (uintptr_t)memory + size );

		//--------------------------------
		// Determine SOA parameter pointers
		//--------------------------------
		data.px  = vertexDataPtr +  0 * data.vertexCount, data.py  = vertexDataPtr +  1 * data.vertexCount, data.pz  = vertexDataPtr +  2 * data.vertexCount;
		data.ptx = vertexDataPtr +  3 * data.vertexCount, data.pty = vertexDataPtr +  4 * data.vertexCount, data.ptz = vertexDataPtr +  5 * data.vertexCount, data.ptw = vertexDataPtr +  6 * data.vertexCount;
		data.nx  = vertexDataPtr +  7 * data.vertexCount, data.ny  = vertexDataPtr +  8 * data.vertexCount, data.nz  = vertexDataPtr +  9 * data.vertexCount;
		data.tx  = vertexDataPtr + 10 * data.vertexCount, data.ty  = vertexDataPtr + 11 * data.vertexCount;

		//--------------------------------
		// Load textures
		//--------------------------------
		softrast_texture* texPtr = model->textures;
		uint32_t loadedTextures = 0, failedTextures = 0;
		for ( auto& uniqueTexture : data.uniqueTextures )
		{
			sprintf ( sprintfBuffer, "Loading model %s...\nLoading texture %s... (%u/%u | %u failed)", path, uniqueTexture.first.c_str ( ), loadedTextures + 1, model->textureCount, failedTextures );
			ProgressDialog::SetDialogParameters ( true, 0.3f + 0.5f * ((float)(loadedTextures++) / model->textureCount), sprintfBuffer );
			uint32_t result = softrast_texture_load ( texPtr, uniqueTexture.first.c_str ( ) );
			if ( result == 0 )
			{
				uniqueTexture.second = texPtr++;
				uniqueTexture.second->path = stringDataPtr;
				strcpy ( stringDataPtr, uniqueTexture.first.c_str ( ) );
				stringDataPtr += uniqueTexture.first.size ( ) + 1;
			}
			else
				failedTextures++;
		}

		//--------------------------------
		// Initialize textures that failed to load to nothingness
		//--------------------------------
		for ( uint32_t i = 0; i < failedTextures; i++ )
		{
			auto tex = texPtr++;
			tex->mipData   = nullptr;
			tex->path      = nullptr;
			tex->mipLevels = 0;
			tex->width     = 0;
			tex->height    = 0;
			model->textureCount--;
		}

		//--------------------------------
		// Process file
		//--------------------------------
		sprintf ( sprintfBuffer, "Loading model %s...\nLoading geometry...", path );
		ProgressDialog::SetDialogParameters ( true, 0.3f + 0.5f * ((float)loadedTextures / model->textureCount), sprintfBuffer );
		structure = desc.GetRootStructure ( )->GetFirstSubnode ( );
		while ( structure )
		{
			switch ( structure->GetStructureType ( ) )
			{
			case OGEX::kStructureGeometryNode:
				_LoadOGEXGeometryNode ( &data, static_cast<const OGEX::GeometryNodeStructure*> ( structure ) );
				break;
			}

			structure = structure->Next ( );
		}

		//--------------------------------
		// Check sanity
		//--------------------------------

		//--------------------------------
		// Return success
		//--------------------------------
		return 0;
	}

	uint32_t softrast_model_free ( softrast_model* model )
	{
		uint32_t ret = 0;
		for ( uint32_t i = 0; i < model->textureCount; i++ )
		{
			uint32_t res = softrast_texture_free ( model->textures + i );
			if ( res )
				ret = -1;
		}
		uint32_t res = miltyalloc_buddy_allocator_free ( _softrastAllocator, model->meshes );
		if ( ret == 0 && res )
			ret = -2;
		return ret;
	}
};