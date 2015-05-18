#pragma once

#include <cstdint>

class MemoryMappedFile
{
public:
	MemoryMappedFile ( const char* filePath );
	~MemoryMappedFile ( );

	bool	IsValid	( ) const;

	uint64_t	GetLastModifiedTime ( ) const;
	uint64_t	GetSize				( ) const;
	const void*	GetData				( ) const;

private:
	void* m_File;
	void* m_Mapping;
	void* m_DataPtr;
	uint64_t m_Size;
};