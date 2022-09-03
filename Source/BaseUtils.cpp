#include "BaseUtils.hpp"
#include "Settings.hpp"
#include <DirectXTex.h>
#include <cwchar>
#include <ctime>

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

#ifndef _CMD_LINE_PARSER

bool CommandLineParser::ReadNextArg(std::wstring *OutArg)
{
	if (m_argv != NULL)
	{
		if (m_ArgIndex >= (size_t)m_argc) return false;

		*OutArg = m_argv[m_ArgIndex];
		m_ArgIndex++;
		return true;
	}
	else
	{
		if (m_ArgIndex >= m_CmdLineLength) return false;
		
		OutArg->clear();
		bool InsideQuotes = false;
		while (m_ArgIndex < m_CmdLineLength)
		{
			wchar_t Ch = m_CmdLine[m_ArgIndex];
			if (Ch == L'\\')
			{
				bool FollowedByQuote = false;
				size_t BackslashCount = 1;
				size_t TmpIndex = m_ArgIndex + 1;
				while (TmpIndex < m_CmdLineLength)
				{
					wchar_t TmpCh = m_CmdLine[TmpIndex];
					if (TmpCh == L'\\')
					{
						BackslashCount++;
						TmpIndex++;
					}
					else if (TmpCh == L'"')
					{
						FollowedByQuote = true;
						break;
					}
					else
						break;
				}

				if (FollowedByQuote)
				{
					if (BackslashCount % 2 == 0)
					{
						for (size_t i = 0; i < BackslashCount / 2; i++)
							*OutArg += L'\\';
						m_ArgIndex += BackslashCount + 1;
						InsideQuotes = !InsideQuotes;
					}
					else
					{
						for (size_t i = 0; i < BackslashCount / 2; i++)
							*OutArg += L'\\';
						*OutArg += L'"';
						m_ArgIndex += BackslashCount + 1;
					}
				}
				else
				{
					for (size_t i = 0; i < BackslashCount; i++)
						*OutArg += L'\\';
					m_ArgIndex += BackslashCount;
				}
			}
			else if (Ch == L'"')
			{
				InsideQuotes = !InsideQuotes;
				m_ArgIndex++;
			}
			else if (isspace(Ch))
			{
				if (InsideQuotes)
				{
					*OutArg += Ch;
					m_ArgIndex++;
				}
				else
				{
					m_ArgIndex++;
					break;
				}
			}
			else
			{
				*OutArg += Ch;
				m_ArgIndex++;
			}
		}

		while (m_ArgIndex < m_CmdLineLength && isspace(m_CmdLine[m_ArgIndex]))
			m_ArgIndex++;

		return true;
	}
}

CommandLineParser::ShortOption * CommandLineParser::FindShortOpt(wchar_t Opt)
{
	for (size_t i = 0; i < m_ShortOpts.size(); i++)
		if (m_ShortOpts[i].Opt == Opt)
			return &m_ShortOpts[i];
	return NULL;
}

CommandLineParser::LongOption * CommandLineParser::FindLongOpt(const wstr_view &Opt)
{
	for (size_t i = 0; i < m_LongOpts.size(); i++)
		if (m_LongOpts[i].Opt == Opt)
			return &m_LongOpts[i];
	return NULL;
}

CommandLineParser::CommandLineParser(int argc, wchar_t **argv) :
	m_argv(argv),
	m_argc(argc),
	m_ArgIndex(1)
{
	assert(argc > 0);
	assert(argv != NULL);
}

CommandLineParser::CommandLineParser(const wchar_t *cmdLine) :
	m_CmdLine(cmdLine)
{
	assert(cmdLine != NULL);
	m_CmdLineLength = wcslen(m_CmdLine);
	while (m_ArgIndex < m_CmdLineLength && isspace(m_CmdLine[m_ArgIndex]))
		++m_ArgIndex;
}

void CommandLineParser::RegisterOption(uint32_t Id, wchar_t Opt, bool Parameter)
{
	assert(Opt != L'\0');
    m_ShortOpts.push_back(ShortOption{Id, Opt, Parameter});
}

void CommandLineParser::RegisterOption(uint32_t Id, const wstr_view &Opt, bool Parameter)
{
	assert(!Opt.empty());
    m_LongOpts.push_back(LongOption{Id, wstring{Opt.data(), Opt.length()}, Parameter});
}

CommandLineParser::Result CommandLineParser::ReadNext()
{
	if (m_InsideMultioption)
	{
		assert(m_LastArgIndex < m_LastArg.length());
		ShortOption *so = FindShortOpt(m_LastArg[m_LastArgIndex]);
		if (so == NULL)
		{
			m_LastOptId = 0;
			m_LastParameter.clear();
			return Result::Error;
		}
		if (so->Parameter)
		{
			if (m_LastArg.length() == m_LastArgIndex+1)
			{
				if (!ReadNextArg(&m_LastParameter))
				{
					m_LastOptId = 0;
					m_LastParameter.clear();
					return Result::Error;
				}
				m_InsideMultioption = false;
				m_LastOptId = so->Id;
				return Result::Option;
			}
			else if (m_LastArg[m_LastArgIndex+1] == L'=')
			{
				m_InsideMultioption = false;
				m_LastParameter = m_LastArg.substr(m_LastArgIndex+2);
				m_LastOptId = so->Id;
				return Result::Option;
			}
			else
			{
				m_InsideMultioption = false;
				m_LastParameter = m_LastArg.substr(m_LastArgIndex+1);
				m_LastOptId = so->Id;
				return Result::Option;
			}
		}
		else
		{
			if (m_LastArg.length() == m_LastArgIndex+1)
			{
				m_InsideMultioption = false;
				m_LastParameter.clear();
				m_LastOptId = so->Id;
				return Result::Option;
			}
			else
			{
				m_LastArgIndex++;

				m_LastParameter.clear();
				m_LastOptId = so->Id;
				return Result::Option;
			}
		}
	}
	else
	{
		if (!ReadNextArg(&m_LastArg))
		{
			m_LastParameter.clear();
			m_LastOptId = 0;
			return Result::End;
		}
		
		if (!m_LastArg.empty() && m_LastArg[0] == L'-')
		{
			if (m_LastArg.length() > 1 && m_LastArg[1] == L'-')
			{
				size_t EqualIndex = m_LastArg.find(L'=', 2);
				if (EqualIndex != std::wstring::npos)
				{
					LongOption *lo = FindLongOpt(m_LastArg.substr(2, EqualIndex-2));
					if (lo == NULL || lo->Parameter == false)
					{
						m_LastOptId = 0;
						m_LastParameter.clear();
						return Result::Error;
					}
					m_LastParameter = m_LastArg.substr(EqualIndex+1);
					m_LastOptId = lo->Id;
					return Result::Option;
				}
				else
				{
					LongOption *lo = FindLongOpt(m_LastArg.substr(2));
					if (lo == NULL)
					{
						m_LastOptId = 0;
						m_LastParameter.clear();
						return Result::Error;
					}
					if (lo->Parameter)
					{
						if (!ReadNextArg(&m_LastParameter))
						{
							m_LastOptId = 0;
							m_LastParameter.clear();
							return Result::Error;
						}
					}
					else
						m_LastParameter.clear();
					m_LastOptId = lo->Id;
					return Result::Option;
				}
			}
			else
			{
				if (m_LastArg.length() < 2)
				{
					m_LastOptId = 0;
					m_LastParameter.clear();
					return Result::Error;
				}
				ShortOption *so = FindShortOpt(m_LastArg[1]);
				if (so == NULL)
				{
					m_LastOptId = 0;
					m_LastParameter.clear();
					return Result::Error;
				}
				if (so->Parameter)
				{
					if (m_LastArg.length() == 2)
					{
						if (!ReadNextArg(&m_LastParameter))
						{
							m_LastOptId = 0;
							m_LastParameter.clear();
							return Result::Error;
						}
						m_LastOptId = so->Id;
						return Result::Option;
					}
					else if (m_LastArg[2] == L'=')
					{
						m_LastParameter = m_LastArg.substr(3);
						m_LastOptId = so->Id;
						return Result::Option;
					}
					else
					{
						m_LastParameter = m_LastArg.substr(2);
						m_LastOptId = so->Id;
						return Result::Option;
					}
				}
				else
				{
					if (m_LastArg.length() == 2)
					{
						m_LastParameter.clear();
						m_LastOptId = so->Id;
						return Result::Option;
					}
					else
					{
						m_InsideMultioption = true;
						m_LastArgIndex = 2;

						m_LastParameter.clear();
						m_LastOptId = so->Id;
						return Result::Option;
					}
				}
			}
		}
		else if (!m_LastArg.empty() && m_LastArg[0] == L'/')
		{
			size_t EqualIndex = m_LastArg.find('=', 1);
			if (EqualIndex != std::wstring::npos)
			{
				if (EqualIndex == 2)
				{
					ShortOption *so = FindShortOpt(m_LastArg[1]);
					if (so != NULL)
					{
						if (so->Parameter == false)	
						{
							m_LastOptId = 0;
							m_LastParameter.clear();
							return Result::Error;
						}
						m_LastParameter = m_LastArg.substr(EqualIndex+1);
						m_LastOptId = so->Id;
						return Result::Option;
					}
				}
				LongOption *lo = FindLongOpt(m_LastArg.substr(1, EqualIndex-1));
				if (lo == NULL || lo->Parameter == false)
				{
					m_LastOptId = 0;
					m_LastParameter.clear();
					return Result::Error;
				}
				m_LastParameter = m_LastArg.substr(EqualIndex+1);
				m_LastOptId = lo->Id;
				return Result::Option;
			}
			else
			{
				if (m_LastArg.length() == 2)
				{
					ShortOption *so = FindShortOpt(m_LastArg[1]);
					if (so != NULL)
					{
						if (so->Parameter)
						{
							if (!ReadNextArg(&m_LastParameter))
							{
								m_LastOptId = 0;
								m_LastParameter.clear();
								return Result::Error;
							}
						}
						else
							m_LastParameter.clear();
						m_LastOptId = so->Id;
						return Result::Option;
					}
				}
				LongOption *lo = FindLongOpt(m_LastArg.substr(1));
				if (lo == NULL)
				{
					m_LastOptId = 0;
					m_LastParameter.clear();
					return Result::Error;
				}
				if (lo->Parameter)
				{
					if (!ReadNextArg(&m_LastParameter))
					{
						m_LastOptId = 0;
						m_LastParameter.clear();
						return Result::Error;
					}
				}
				else
					m_LastParameter.clear();
				m_LastOptId = lo->Id;
				return Result::Option;
			}
		}
		else
		{
			m_LastOptId = 0;
			m_LastParameter = m_LastArg;
			return Result::Parameter;
		}
	}
}

#endif // _CMD_LINE_PARSER

#ifndef _PUBLIC_FUNCTIONS

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
        return std::format(L"HRESULT = 0x{:08X} ({})", (uint32_t)hr, msg);
    else
        return std::format(L"HRESULT = 0x{:08X}", (uint32_t)hr);
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

void ToUpperCase(wstring& inoutStr)
{
    std::transform(inoutStr.begin(), inoutStr.end(), inoutStr.begin(), ::towupper);
}

wstring CurrentDateTime()
{
    std::time_t t1 = std::time(nullptr);
    tm t2;
    localtime_s(&t2, &t1);
    wchar_t s[24];
    wcsftime(s, _countof(s), L"%Y-%m-%d %H:%M:%S", &t2);
    return s;
}

std::filesystem::path StrToPath(const wstr_view& str)
{
    return std::filesystem::path(str.begin(), str.end(), std::filesystem::path::native_format);
}
std::filesystem::path StrToPath(const str_view& str)
{
    return std::filesystem::path(str.begin(), str.end(), std::filesystem::path::native_format);
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

const wchar_t* DXGIFormatToStr(DXGI_FORMAT format)
{
#define FORMAT_CASE(v) case DXGI_FORMAT_##v: return L"" #v;
    switch(format)
    {
    FORMAT_CASE(UNKNOWN)
    FORMAT_CASE(R32G32B32A32_TYPELESS)
    FORMAT_CASE(R32G32B32A32_FLOAT)
    FORMAT_CASE(R32G32B32A32_UINT)
    FORMAT_CASE(R32G32B32A32_SINT)
    FORMAT_CASE(R32G32B32_TYPELESS)
    FORMAT_CASE(R32G32B32_FLOAT)
    FORMAT_CASE(R32G32B32_UINT)
    FORMAT_CASE(R32G32B32_SINT)
    FORMAT_CASE(R16G16B16A16_TYPELESS)
    FORMAT_CASE(R16G16B16A16_FLOAT)
    FORMAT_CASE(R16G16B16A16_UNORM)
    FORMAT_CASE(R16G16B16A16_UINT)
    FORMAT_CASE(R16G16B16A16_SNORM)
    FORMAT_CASE(R16G16B16A16_SINT)
    FORMAT_CASE(R32G32_TYPELESS)
    FORMAT_CASE(R32G32_FLOAT)
    FORMAT_CASE(R32G32_UINT)
    FORMAT_CASE(R32G32_SINT)
    FORMAT_CASE(R32G8X24_TYPELESS)
    FORMAT_CASE(D32_FLOAT_S8X24_UINT)
    FORMAT_CASE(R32_FLOAT_X8X24_TYPELESS)
    FORMAT_CASE(X32_TYPELESS_G8X24_UINT)
    FORMAT_CASE(R10G10B10A2_TYPELESS)
    FORMAT_CASE(R10G10B10A2_UNORM)
    FORMAT_CASE(R10G10B10A2_UINT)
    FORMAT_CASE(R11G11B10_FLOAT)
    FORMAT_CASE(R8G8B8A8_TYPELESS)
    FORMAT_CASE(R8G8B8A8_UNORM)
    FORMAT_CASE(R8G8B8A8_UNORM_SRGB)
    FORMAT_CASE(R8G8B8A8_UINT)
    FORMAT_CASE(R8G8B8A8_SNORM)
    FORMAT_CASE(R8G8B8A8_SINT)
    FORMAT_CASE(R16G16_TYPELESS)
    FORMAT_CASE(R16G16_FLOAT)
    FORMAT_CASE(R16G16_UNORM)
    FORMAT_CASE(R16G16_UINT)
    FORMAT_CASE(R16G16_SNORM)
    FORMAT_CASE(R16G16_SINT)
    FORMAT_CASE(R32_TYPELESS)
    FORMAT_CASE(D32_FLOAT)
    FORMAT_CASE(R32_FLOAT)
    FORMAT_CASE(R32_UINT)
    FORMAT_CASE(R32_SINT)
    FORMAT_CASE(R24G8_TYPELESS)
    FORMAT_CASE(D24_UNORM_S8_UINT)
    FORMAT_CASE(R24_UNORM_X8_TYPELESS)
    FORMAT_CASE(X24_TYPELESS_G8_UINT)
    FORMAT_CASE(R8G8_TYPELESS)
    FORMAT_CASE(R8G8_UNORM)
    FORMAT_CASE(R8G8_UINT)
    FORMAT_CASE(R8G8_SNORM)
    FORMAT_CASE(R8G8_SINT)
    FORMAT_CASE(R16_TYPELESS)
    FORMAT_CASE(R16_FLOAT)
    FORMAT_CASE(D16_UNORM)
    FORMAT_CASE(R16_UNORM)
    FORMAT_CASE(R16_UINT)
    FORMAT_CASE(R16_SNORM)
    FORMAT_CASE(R16_SINT)
    FORMAT_CASE(R8_TYPELESS)
    FORMAT_CASE(R8_UNORM)
    FORMAT_CASE(R8_UINT)
    FORMAT_CASE(R8_SNORM)
    FORMAT_CASE(R8_SINT)
    FORMAT_CASE(A8_UNORM)
    FORMAT_CASE(R1_UNORM)
    FORMAT_CASE(R9G9B9E5_SHAREDEXP)
    FORMAT_CASE(R8G8_B8G8_UNORM)
    FORMAT_CASE(G8R8_G8B8_UNORM)
    FORMAT_CASE(BC1_TYPELESS)
    FORMAT_CASE(BC1_UNORM)
    FORMAT_CASE(BC1_UNORM_SRGB)
    FORMAT_CASE(BC2_TYPELESS)
    FORMAT_CASE(BC2_UNORM)
    FORMAT_CASE(BC2_UNORM_SRGB)
    FORMAT_CASE(BC3_TYPELESS)
    FORMAT_CASE(BC3_UNORM)
    FORMAT_CASE(BC3_UNORM_SRGB)
    FORMAT_CASE(BC4_TYPELESS)
    FORMAT_CASE(BC4_UNORM)
    FORMAT_CASE(BC4_SNORM)
    FORMAT_CASE(BC5_TYPELESS)
    FORMAT_CASE(BC5_UNORM)
    FORMAT_CASE(BC5_SNORM)
    FORMAT_CASE(B5G6R5_UNORM)
    FORMAT_CASE(B5G5R5A1_UNORM)
    FORMAT_CASE(B8G8R8A8_UNORM)
    FORMAT_CASE(B8G8R8X8_UNORM)
    FORMAT_CASE(R10G10B10_XR_BIAS_A2_UNORM)
    FORMAT_CASE(B8G8R8A8_TYPELESS)
    FORMAT_CASE(B8G8R8A8_UNORM_SRGB)
    FORMAT_CASE(B8G8R8X8_TYPELESS)
    FORMAT_CASE(B8G8R8X8_UNORM_SRGB)
    FORMAT_CASE(BC6H_TYPELESS)
    FORMAT_CASE(BC6H_UF16)
    FORMAT_CASE(BC6H_SF16)
    FORMAT_CASE(BC7_TYPELESS)
    FORMAT_CASE(BC7_UNORM)
    FORMAT_CASE(BC7_UNORM_SRGB)
    FORMAT_CASE(AYUV)
    FORMAT_CASE(Y410)
    FORMAT_CASE(Y416)
    FORMAT_CASE(NV12)
    FORMAT_CASE(P010)
    FORMAT_CASE(P016)
    FORMAT_CASE(420_OPAQUE)
    FORMAT_CASE(YUY2)
    FORMAT_CASE(Y210)
    FORMAT_CASE(Y216)
    FORMAT_CASE(NV11)
    FORMAT_CASE(AI44)
    FORMAT_CASE(IA44)
    FORMAT_CASE(P8)
    FORMAT_CASE(A8P8)
    FORMAT_CASE(B4G4R4A4_UNORM)
    FORMAT_CASE(P208)
    FORMAT_CASE(V208)
    FORMAT_CASE(V408)
    FORMAT_CASE(SAMPLER_FEEDBACK_MIN_MIP_OPAQUE)
    FORMAT_CASE(SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE)
    }
    return L"";
#undef FORMAT_CASE
}

void FillShaderResourceViewDesc_Texture2D(D3D12_SHADER_RESOURCE_VIEW_DESC& dst,
    DXGI_FORMAT format)
{
    dst = {
        .Format = format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3),
        .Texture2D = {
            .MipLevels = UINT_MAX }};
}

void FillRasterizerDesc(D3D12_RASTERIZER_DESC& dst,
    D3D12_CULL_MODE cullMode, BOOL frontCounterClockwise)
{
    dst = {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = cullMode,
        .FrontCounterClockwise = frontCounterClockwise,
        .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
        .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        .MultisampleEnable = FALSE,
        .AntialiasedLineEnable = FALSE,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF};
}

void FillRasterizerDesc_NoCulling(D3D12_RASTERIZER_DESC& dst)
{
    FillRasterizerDesc(dst, D3D12_CULL_MODE_NONE, FALSE);
}

void FillBlendDesc_BlendRT0(D3D12_BLEND_DESC& dst,
    D3D12_BLEND srcBlend, D3D12_BLEND destBlend, D3D12_BLEND_OP blendOp,
    D3D12_BLEND srcBlendAlpha, D3D12_BLEND destBlendAlpha, D3D12_BLEND_OP blendOpAlpha)
{
    dst = {};
    dst.RenderTarget[0] = {
        .BlendEnable = TRUE,
        .LogicOpEnable = FALSE,
        .SrcBlend = srcBlend,
        .DestBlend = destBlend,
        .BlendOp = blendOp,
        .SrcBlendAlpha = srcBlendAlpha,
        .DestBlendAlpha = destBlendAlpha,
        .BlendOpAlpha = blendOpAlpha,
        .LogicOp = D3D12_LOGIC_OP_NOOP,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL};
    const D3D12_RENDER_TARGET_BLEND_DESC otherRTBlendDesc = {
        .BlendEnable = FALSE,
        .LogicOpEnable = FALSE,
        .SrcBlend = D3D12_BLEND_ONE,
        .DestBlend = D3D12_BLEND_ONE,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ONE,
        .DestBlendAlpha = D3D12_BLEND_ONE,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .LogicOp = D3D12_LOGIC_OP_NOOP,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL};
    for (UINT i = 1; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        dst.RenderTarget[i] = otherRTBlendDesc;
}

void FillBlendDesc_NoBlending(D3D12_BLEND_DESC& dst)
{
    dst = {};
    const D3D12_RENDER_TARGET_BLEND_DESC RTBlendDesc = {
        .BlendEnable = FALSE,
        .LogicOpEnable = FALSE,
        .SrcBlend = D3D12_BLEND_ONE,
        .DestBlend = D3D12_BLEND_ONE,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ONE,
        .DestBlendAlpha = D3D12_BLEND_ONE,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .LogicOp = D3D12_LOGIC_OP_NOOP,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL};
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        dst.RenderTarget[i] = RTBlendDesc;
}

void FillDepthStencilDesc_NoTests(D3D12_DEPTH_STENCIL_DESC& dst)
{
    dst = {
        .DepthEnable = FALSE,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
        .DepthFunc = D3D12_COMPARISON_FUNC_GREATER,
        .StencilEnable = FALSE,
        .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
        .FrontFace = {
            .StencilFailOp = D3D12_STENCIL_OP_KEEP,
            .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
            .StencilPassOp = D3D12_STENCIL_OP_KEEP,
            .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS},
        .BackFace = {
            .StencilFailOp = D3D12_STENCIL_OP_KEEP,
            .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
            .StencilPassOp = D3D12_STENCIL_OP_KEEP,
            .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS}};
}

void FillDepthStencilDesc_DepthTest(D3D12_DEPTH_STENCIL_DESC& dst,
    D3D12_DEPTH_WRITE_MASK depthWriteMask, D3D12_COMPARISON_FUNC depthFunc)
{
    dst = {
        .DepthEnable = TRUE,
        .DepthWriteMask = depthWriteMask,
        .DepthFunc = depthFunc,
        .StencilEnable = FALSE,
        .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
        .FrontFace = {
            .StencilFailOp = D3D12_STENCIL_OP_KEEP,
            .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
            .StencilPassOp = D3D12_STENCIL_OP_KEEP,
            .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS},
        .BackFace = {
            .StencilFailOp = D3D12_STENCIL_OP_KEEP,
            .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
            .StencilPassOp = D3D12_STENCIL_OP_KEEP,
            .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS}};
}

void FillRootParameter_DescriptorTable(D3D12_ROOT_PARAMETER& dst,
    const D3D12_DESCRIPTOR_RANGE* descriptorRange, D3D12_SHADER_VISIBILITY shaderVisibility)
{
    dst = {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = {
            .NumDescriptorRanges = 1,
            .pDescriptorRanges = descriptorRange},
        .ShaderVisibility = shaderVisibility};
}

bool FileExists(const std::filesystem::path& path)
{
    std::error_code errorCode;
    return std::filesystem::is_regular_file(path, errorCode);
}

bool GetFileLastWriteTime(std::filesystem::file_time_type& outTime, const std::filesystem::path& path)
{
    std::error_code errorCode;
    outTime = std::filesystem::last_write_time(path, errorCode);
    return !errorCode;
}

#endif // _PUBLIC_FUNCTIONS
