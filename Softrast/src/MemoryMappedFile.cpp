#include "MemoryMappedFile.h"
#include <Windows.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MemoryMappedFile::MemoryMappedFile ( const char* filePath )
	: m_File ( 0 ), m_Mapping ( 0 ), m_DataPtr ( 0 ), m_Size ( 0 )
{
	m_File = CreateFile ( filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );

	if ( m_File == INVALID_HANDLE_VALUE )
		return;

	DWORD fileSizeHigh = 0;
	DWORD fileSizeLow  = GetFileSize ( m_File, &fileSizeHigh );

	m_Size    = (uint64_t ( fileSizeHigh )<<32) | fileSizeLow;
	m_Mapping = CreateFileMapping ( m_File, NULL, PAGE_READONLY, 0, 0, NULL );

	if ( m_Mapping == 0 )
		return;

	m_DataPtr = (uint8_t*)MapViewOfFile ( m_Mapping, FILE_MAP_READ, 0, 0, 0 );

	if ( m_DataPtr == nullptr )
		return;
}

MemoryMappedFile::~MemoryMappedFile ( )
{
	if ( m_DataPtr )
		UnmapViewOfFile ( m_DataPtr );
	if ( m_Mapping )
		CloseHandle ( m_Mapping );
	if ( m_File )
		CloseHandle ( m_File );

#ifdef _DEBUG
	// For debug builds: Clean up the pointers so we get glorious errors if we do messed up stuff!
	m_File    = (void*)0xFEEEFEEEFEEEFEEE;
	m_Mapping = (void*)0xFEEEFEEEFEEEFEEE;
	m_DataPtr = (void*)0xFEEEFEEEFEEEFEEE;
	m_Size    = 0xFEEEFEEEFEEEFEEE;
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryMappedFile::IsValid ( ) const
{
	return m_File && m_Mapping && m_DataPtr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint64_t MemoryMappedFile::GetLastModifiedTime ( ) const
{
	FILETIME t;
	GetFileTime ( m_File, nullptr, nullptr, &t );
	return (uint64_t ( t.dwHighDateTime ) << 32) | uint64_t ( t.dwLowDateTime );
}

uint64_t MemoryMappedFile::GetSize ( ) const
{
	return m_Size;
}

const void* MemoryMappedFile::GetData ( ) const
{
	return m_DataPtr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////