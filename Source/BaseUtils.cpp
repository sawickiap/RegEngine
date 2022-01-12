#include "BaseUtils.hpp"

string VFormat(const str_view& format, va_list argList)
{
    const size_t dstLen = (size_t)_vscprintf(format.c_str(), argList);
    if(dstLen)
    {
        std::vector<char> buf(dstLen + 1);
        vsprintf_s(&buf[0], dstLen + 1, format.c_str(), argList);
        return string{buf.data(), buf.data() + dstLen};
    }
    else
        return {};
}
wstring VFormat(const wstr_view& format, va_list argList)
{
    const size_t dstLen = (size_t)_vscwprintf(format.c_str(), argList);
    if(dstLen)
    {
        std::vector<wchar_t> buf(dstLen + 1);
        vswprintf_s(&buf[0], dstLen + 1, format.c_str(), argList);
        return wstring{buf.data(), buf.data() + dstLen};
    }
    else
        return {};
}

string Format(const str_view& format, ...)
{
	auto formatStr = format.c_str();
    va_list argList;
    va_start(argList, formatStr);
    auto result = VFormat(format, argList);
    va_end(argList);
    return result;
}
wstring Format(const wstr_view& format, ...)
{
	auto formatStr = format.c_str();
    va_list argList;
    va_start(argList, formatStr);
    auto result = VFormat(format, argList);
    va_end(argList);
    return result;
}

std::vector<char> LoadFile(const wstr_view& path)
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

// For C++ threads as threadId you can use:
// GetThreadId((HANDLE)t.native_handle());
void SetThreadName(DWORD threadId, const str_view& name)
{
    // Code found in: https://stackoverflow.com/questions/10121560/stdthread-naming-your-thread

    const DWORD MS_VC_EXCEPTION = 0x406D1388;

    #pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
    #pragma pack(pop)

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name.c_str();
    info.dwThreadID = threadId;
    info.dwFlags = 0;

    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
}
