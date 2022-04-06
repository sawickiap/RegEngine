#include "BaseUtils.hpp"
#include "Streams.hpp"

size_t BaseStream::GetSize()
{
    assert(0 && "GetSize not implemented for this type of stream.");
    return SIZE_MAX;
}

size_t BaseStream::GetPosition()
{
    assert(0 && "GetPosition not implemented for this type of stream.");
    return SIZE_MAX;
}

size_t BaseStream::SetPosition(ptrdiff_t distance, MoveMethod method)
{
    assert(0 && "SetPosition not implemented for this type of stream.");
    return SIZE_MAX;
}

size_t BaseStream::TryRead(void* outBytes, size_t maxBytesToRead)
{
    assert(0 && "TryReadBytes not implemented for this type of stream.");
    return 0;
}

void BaseStream::Write(const void* bytes, size_t bytesToWrite)
{
    assert(0 && "WriteBytes not implemented for this type of stream.");
}

void BaseStream::Read(void* outBytes, size_t bytesToRead)
{
    if(TryRead(outBytes, bytesToRead) < bytesToRead)
        FAIL(L"Too few bytes read.");
}

FileStream::FileStream(const wstr_view& path, uint32_t flags)
{
    ERR_TRY

    LogInfoF(L"Opening file \"{}\"...", path);

    DWORD desiredAccess = 0;
    if((flags & Flag_Read) != 0)
        desiredAccess |= GENERIC_READ;
    if((flags & Flag_Write) != 0)
        desiredAccess |= GENERIC_WRITE;

    DWORD shareMode = (flags & Flag_Write) != 0 ? 0 : FILE_SHARE_READ;

    DWORD creationDisposition = (flags & Flag_Write) != 0 ? CREATE_ALWAYS : OPEN_EXISTING;

    DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    if((flags & Flag_Sequential) != 0)
        flagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;

	HANDLE handle = CreateFile(
		path.c_str(), // lpFileName
		desiredAccess,
		shareMode,
		NULL, // lpSecurityAttributes
		creationDisposition,
		flagsAndAttributes,
		NULL); // hTemplateFile
	CHECK_BOOL_WINAPI(handle != INVALID_HANDLE_VALUE);
	m_Handle.reset(handle);

    ERR_CATCH_MSG(std::format(L"Cannot open file \"{}\".", path));
}

size_t FileStream::GetSize()
{
    assert(m_Handle.get() != INVALID_HANDLE_VALUE);
    LARGE_INTEGER i;
    CHECK_BOOL_WINAPI(GetFileSizeEx(m_Handle.get(), &i));
    return (size_t)i.QuadPart;    
}

size_t FileStream::GetPosition()
{
    assert(m_Handle.get() != INVALID_HANDLE_VALUE);
    LARGE_INTEGER distanceToMove;
    distanceToMove.QuadPart = 0;
    LARGE_INTEGER newPointer;
    CHECK_BOOL_WINAPI(SetFilePointerEx(m_Handle.get(), distanceToMove, &newPointer, FILE_CURRENT));
    return (size_t)newPointer.QuadPart;
}

size_t FileStream::SetPosition(ptrdiff_t distance, MoveMethod method)
{
    assert(m_Handle.get() != INVALID_HANDLE_VALUE);
    LARGE_INTEGER distanceToMove, newPointer;
    distanceToMove.QuadPart = (LONGLONG)distance;
    CHECK_BOOL_WINAPI(SetFilePointerEx(m_Handle.get(), distanceToMove, &newPointer, (DWORD)method));
    return (size_t)newPointer.QuadPart;
}

size_t FileStream::TryRead(void* outBytes, size_t maxBytesToRead)
{
    assert(m_Handle.get() != INVALID_HANDLE_VALUE);
    if(maxBytesToRead == 0)
        return 0;
    const DWORD bytesToRead = (DWORD)maxBytesToRead;
    DWORD bytesRead;
    CHECK_BOOL_WINAPI(ReadFile(m_Handle.get(), outBytes, bytesToRead, &bytesRead, NULL));
    return (size_t)bytesRead;
}

void FileStream::Write(const void* bytes, size_t bytesToWrite)
{
    assert(m_Handle.get() != INVALID_HANDLE_VALUE);
    if(bytesToWrite == 0)
        return;
    DWORD numberOfBytesToWrite = (DWORD)bytesToWrite;
    DWORD numberOfBytesWritten;
    CHECK_BOOL_WINAPI(WriteFile(m_Handle.get(), bytes, numberOfBytesToWrite, &numberOfBytesWritten, NULL));
    CHECK_BOOL(numberOfBytesWritten == numberOfBytesToWrite);
}

std::vector<char> LoadFile(const wstr_view& path)
{
    ERR_TRY;

    FileStream s(path, FileStream::Flag_Read | FileStream::Flag_Sequential);
    const size_t size = s.GetSize();
    std::vector<char> data(size);
    if(size)
        s.Read(data.data(), size);
	return data;

    ERR_CATCH_MSG(std::format(L"Cannot load file \"{}\".", path));
}

void SaveFile(const wstr_view& path, std::span<const char> bytes)
{
    ERR_TRY;

    FileStream s(path, FileStream::Flag_Write | FileStream::Flag_Sequential);
    s.Write(bytes);

    ERR_CATCH_MSG(std::format(L"Cannot save file \"{}\".", path));
}
