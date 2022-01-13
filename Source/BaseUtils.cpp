#include "BaseUtils.hpp"
#include "Settings.hpp"

static BoolSetting g_UseThreadNames(SettingCategory::Startup, "UseThreadNames", true);
static BoolSetting g_UseD3d12ObjectNames(SettingCategory::Startup, "UseD3d12ObjectNames", true);

void Exception::Print() const
{
    fwprintf(stderr, L"ERROR:\n");
    for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
    {
        if(it->m_file && *it->m_file)
            fwprintf(stderr, L"%s(%u): %.*s\n", it->m_file, it->m_line, STR_TO_FORMAT(it->m_message));
        else
            fwprintf(stderr, L"%.*s\n", STR_TO_FORMAT(it->m_message));
    }
}

wstring GetWinApiErrorMessage()
{
    const uint32_t err = GetLastError();
    wchar_t* msg;
	uint32_t msgLen = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // dwFlags
		NULL, // lpSource
		err, // dwMessageId
		0, // dwLanguageId
		(LPTSTR)&msg, // lpBuffer (celowe rzutowanie tchar** na tchar* !!!)
		0, // nSize
		NULL); // Arguments

    // Trim trailing end of line.
    while(msgLen && isspace(msg[msgLen-1]))
        --msgLen;

	wstring result = Format(L"GetLastError() = 0x%08X: %.*s", err, (int)msgLen, msg);
	LocalFree(msg);
    return result;
}

wstring GetHresultErrorMessage(HRESULT hr)
{
    const wchar_t* msg = nullptr;
#define KNOWN_ERROR(symbol) case symbol: msg = _T(#symbol); break;
    switch(hr)
    {
    KNOWN_ERROR(E_ABORT)
    KNOWN_ERROR(E_ACCESSDENIED)
    KNOWN_ERROR(E_FAIL)
    KNOWN_ERROR(E_HANDLE)
    KNOWN_ERROR(E_INVALIDARG)
    KNOWN_ERROR(E_NOINTERFACE)
    KNOWN_ERROR(E_NOTIMPL)
    KNOWN_ERROR(E_OUTOFMEMORY)
    KNOWN_ERROR(E_PENDING)
    KNOWN_ERROR(E_POINTER)
    KNOWN_ERROR(E_UNEXPECTED)
    KNOWN_ERROR(S_FALSE)
    KNOWN_ERROR(S_OK)
    }
#undef KNOWN_SYMBOL
    if(msg)
        return Format(L"HRESULT = 0x%08X (%s)", hr, msg);
    else
        return Format(L"HRESULT = 0x08X", hr);
}

string ConvertUnicodeToChars(const wstr_view& str, uint32_t codePage)
{
    if(str.empty())
        return string{};
    const int size = WideCharToMultiByte(codePage, 0, str.data(), (int)str.size(), NULL, 0, NULL, FALSE);
    if(size == 0)
        return string{};
    std::vector<char> buf((size_t)size);
    const int result = WideCharToMultiByte(codePage, 0, str.data(), (int)str.size(), buf.data(), size, NULL, FALSE);
    if(result == 0)
        return string{};
    return string{buf.data(), buf.size()};
}

wstring ConvertCharsToUnicode(const str_view& str, uint32_t codePage)
{
    if(str.empty())
        return wstring{};
    const int size = MultiByteToWideChar(codePage, 0, str.data(), (int)str.size(), NULL, 0);
    if(size == 0)
        return wstring{};
    std::vector<wchar_t> buf((size_t)size);
    const int result = MultiByteToWideChar(codePage, 0, str.data(), (int)str.size(), buf.data(), size);
    if(result == 0)
        return wstring{};
    return wstring{buf.data(), buf.size()};
}

string VFormat(const char* format, va_list argList)
{
    const size_t dstLen = (size_t)_vscprintf(format, argList);
    if(dstLen)
    {
        std::vector<char> buf(dstLen + 1);
        vsprintf_s(&buf[0], dstLen + 1, format, argList);
        return string{buf.data(), buf.data() + dstLen};
    }
    else
        return {};
}
wstring VFormat(const wchar_t* format, va_list argList)
{
    const size_t dstLen = (size_t)_vscwprintf(format, argList);
    if(dstLen)
    {
        std::vector<wchar_t> buf(dstLen + 1);
        vswprintf_s(&buf[0], dstLen + 1, format, argList);
        return wstring{buf.data(), buf.data() + dstLen};
    }
    else
        return {};
}

string Format(const char* format, ...)
{
    va_list argList;
    va_start(argList, format);
    auto result = VFormat(format, argList);
    va_end(argList);
    return result;
}
wstring Format(const wchar_t* format, ...)
{
    va_list argList;
    va_start(argList, format);
    auto result = VFormat(format, argList);
    va_end(argList);
    return result;
}

std::vector<char> LoadFile(const wstr_view& path)
{
    ERR_TRY;

    wprintf(Format(L"Loading file \"%.*s\"...\n", STR_TO_FORMAT(path)).c_str());

	HANDLE handle = CreateFile(
		path.c_str(), // lpFileName
		GENERIC_READ, // dwDesiredAccess
		FILE_SHARE_READ, // dwShareMode
		NULL, // lpSecurityAttributes
		OPEN_EXISTING, // dwCreationDisposition
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, // dwFlagsAndAttributes
		NULL); // hTemplateFile
	CHECK_BOOL_WINAPI(handle != INVALID_HANDLE_VALUE);
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

    ERR_CATCH_MSG(Format(L"Cannot load file \"%s\".", path.c_str()));
}

// For C++ threads as threadId you can use:
// GetThreadId((HANDLE)t.native_handle());
void SetThreadName(DWORD threadId, const str_view& name)
{
    if(!g_UseThreadNames.GetValue())
        return;

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

void SetD3d12ObjectName(ID3D12Object* obj, const wstr_view& name)
{
    if(!g_UseD3d12ObjectNames.GetValue())
        return;
    obj->SetName(name.c_str());
}
