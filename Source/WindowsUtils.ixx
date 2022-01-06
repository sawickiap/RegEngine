module;

#include "WindowsUtils.h"
#include "BaseUtils.hpp"
#include <vector>

export module WindowsUtils;

import BaseUtils;

export std::vector<char> LoadFile(const wstr_view& path)
{
	HANDLE handle = CreateFile(
		path.c_str(), // lpFileName
		GENERIC_READ, // dwDesiredAccess
		FILE_SHARE_READ, // dwShareMode
		NULL, // lpSecurityAttributes
		OPEN_EXISTING, // dwCreationDisposition
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, // dwFlagsAndAttributes
		NULL); // hTemplateFile
	CHECK_BOOL(handle != INVALID_HANDLE_VALUE);
	unique_ptr<HANDLE, CloseHandleDeleter> handlePtr(handle);

	LARGE_INTEGER sizeInt = {};
	CHECK_BOOL(GetFileSizeEx(handle, &sizeInt));
	if(sizeInt.QuadPart == 0)
		return {};
	CHECK_BOOL(sizeInt.QuadPart <= (LONGLONG)UINT32_MAX);
	const DWORD size = sizeInt.LowPart;
	
	std::vector<char> bytes(size);
	DWORD numberOfBytesRead = 0;
	CHECK_BOOL(ReadFile(handle, bytes.data(), size, &numberOfBytesRead, NULL));
	CHECK_BOOL(numberOfBytesRead == numberOfBytesRead);
	
	return bytes;
}
