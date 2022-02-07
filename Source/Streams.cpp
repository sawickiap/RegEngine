#include "BaseUtils.hpp"
#include "Streams.hpp"

void FileReadStream::Init(const wstr_view& path, uint32_t flags)
{
    ERR_TRY

    LogInfoF(L"Reading file \"{}\"...", path);

    DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    if((flags & FILE_STREAM_FLAG_SEQUENTIAL) != 0)
        flagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;

	HANDLE handle = CreateFile(
		path.c_str(), // lpFileName
		GENERIC_READ, // dwDesiredAccess
		0, // dwShareMode
		NULL, // lpSecurityAttributes
		OPEN_EXISTING, // dwCreationDisposition
		flagsAndAttributes,
		NULL); // hTemplateFile
	CHECK_BOOL_WINAPI(handle != INVALID_HANDLE_VALUE);
	m_Handle.reset(handle);

    ERR_CATCH_MSG(std::format(L"Cannot open file \"{}\" for reading.", path));
}

FileReadStream::~FileReadStream()
{

}

size_t FileReadStream::ReadSomeData(void* data, size_t len)
{
    if(len == 0)
        return 0;
    const DWORD bytesToRead = (DWORD)len;
    DWORD bytesRead;
    CHECK_BOOL_WINAPI(ReadFile(m_Handle.get(), data, bytesToRead, &bytesRead, NULL));
    return bytesRead;
}

void FileReadStream::ReadData(std::span<char> data)
{
    CHECK_BOOL(ReadSomeData(data.data(), data.size()) == data.size());
}

void FileWriteStream::Init(const wstr_view& path, uint32_t flags)
{
    ERR_TRY

    LogInfoF(L"Writing file \"{}\"...", path);

    DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    if((flags & FILE_STREAM_FLAG_SEQUENTIAL) != 0)
        flagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;

	HANDLE handle = CreateFile(
		path.c_str(), // lpFileName
		GENERIC_WRITE, // dwDesiredAccess
		0, // dwShareMode
		NULL, // lpSecurityAttributes
		CREATE_ALWAYS, // dwCreationDisposition
		flagsAndAttributes,
		NULL); // hTemplateFile
	CHECK_BOOL_WINAPI(handle != INVALID_HANDLE_VALUE);
	m_Handle.reset(handle);

    ERR_CATCH_MSG(std::format(L"Cannot open file \"{}\" for writing.", path));
}

FileWriteStream::~FileWriteStream()
{

}

void FileWriteStream::WriteData(std::span<const char> data)
{
    if(data.empty())
        return;
    DWORD numberOfBytesToWrite = (DWORD)data.size();
    DWORD numberOfBytesWritten;
    CHECK_BOOL_WINAPI(WriteFile(m_Handle.get(), data.data(), numberOfBytesToWrite, &numberOfBytesWritten, NULL));
    CHECK_BOOL(numberOfBytesWritten == numberOfBytesToWrite);
}
