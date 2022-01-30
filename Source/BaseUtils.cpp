#include "BaseUtils.hpp"
#include "Settings.hpp"
#include <DirectXTex.h>

// Use this macro to pass the 2 parameters to formatting function matching formatting string like "%.*s", "%.*hs" etc.
// for s of type like std::string, std::wstring, str_view, wstr_view.
#define STR_TO_FORMAT(s) (int)(s).size(), (s).data()

static constexpr D3D12_RANGE D3D12_RANGE_NONE_DATA = {0, 0};
const D3D12_RANGE* D3D12_RANGE_NONE = &D3D12_RANGE_NONE_DATA;

extern const D3D12_HEAP_PROPERTIES D3D12_HEAP_PROPERTIES_DEFAULT = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
extern const D3D12_HEAP_PROPERTIES D3D12_HEAP_PROPERTIES_UPLOAD = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_UPLOAD};
extern const D3D12_HEAP_PROPERTIES D3D12_HEAP_PROPERTIES_READBACK = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_READBACK};


static BoolSetting g_UseThreadNames(SettingCategory::Startup, "UseThreadNames", true);
static BoolSetting g_UseD3d12ObjectNames(SettingCategory::Startup, "UseD3d12ObjectNames", true);

void Exception::Print() const
{
    LogError(L"ERROR:");
    for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
    {
        if(it->m_file && *it->m_file)
            LogErrorF(L"{}({}): {}", it->m_file, it->m_line, it->m_message);
        else
            LogError(it->m_message);
    }
}

wstring GetWinAPIErrorMessage()
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

	wstring result = std::format(L"GetLastError() = 0x{:08X}: {}", err, wstr_view(msg, msgLen));
	LocalFree(msg);
    return result;
}

wstring GetHRESULTErrorMessage(HRESULT hr)
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
        return std::format(L"HRESULT = 0x{:08X} ({})", hr, msg);
    else
        return std::format(L"HRESULT = 0x{:08X}", hr);
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

static void SetConsoleColor(LogLevel level)
{
    WORD attr = 0;
    switch(level)
    {
    case LogLevel::Info:
        attr = FOREGROUND_INTENSITY;
        break;
    case LogLevel::Message:
        attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case LogLevel::Warning:
        attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case LogLevel::Error:
        attr = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    default:
        assert(0);
    }

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(out, attr);
}

void Log(LogLevel level, const wstr_view& msg)
{
    wstr_view finalMsg = msg;
    // Trim trailing end of line.
    if(finalMsg.ends_with(L'\n', true))
        finalMsg = finalMsg.substr(0, finalMsg.length() - 1);
    else if(finalMsg.ends_with(L"\r\n", true))
        finalMsg = finalMsg.substr(0, finalMsg.length() - 2);

    if(level != LogLevel::Message)
        SetConsoleColor(level);
    wprintf(L"%.*s\n", STR_TO_FORMAT(finalMsg));
    if (level != LogLevel::Message)
        SetConsoleColor(LogLevel::Message);
}

std::vector<char> LoadFile(const wstr_view& path)
{
    ERR_TRY;

    LogInfoF(L"Loading file \"{}\"...", path);

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

    ERR_CATCH_MSG(std::format(L"Cannot load file \"{}\".", path));
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

void SetD3D12ObjectName(ID3D12Object* obj, const wstr_view& name)
{
    if(!g_UseD3d12ObjectNames.GetValue())
        return;
    obj->SetName(name.c_str());
}

uint8_t DXGIFormatToBitsPerPixel(DXGI_FORMAT format)
{
    return (uint8_t)DirectX::BitsPerPixel(format);
}
