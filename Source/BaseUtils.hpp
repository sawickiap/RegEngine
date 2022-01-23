#pragma once

#include "../ThirdParty/str_view/str_view.hpp"

#define GLM_FORCE_CXX17
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_LEFT_HANDED // The coordinate system we decide to use.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // The convention Direct3D uses.
#define GLM_FORCE_SILENT_WARNINGS
#include "../ThirdParty/glm/glm/glm.hpp"
#include "../ThirdParty/glm/glm/gtc/type_ptr.hpp"
#include "../ThirdParty/glm/glm/gtc/constants.hpp"
#include "../ThirdParty/glm/glm/gtc/matrix_transform.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>
#include <wrl/client.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include "../ThirdParty/d3dx12/d3dx12.h"

#include "../ThirdParty/D3D12MemoryAllocator/include/D3D12MemAlloc.h"

#include <memory>
#include <exception>
#include <stdexcept>
#include <array>
#include <limits>
#include <vector>
#include <thread>
#include <initializer_list>
#include <limits>

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <tchar.h>

using std::unique_ptr;
using std::make_unique;
using std::wstring;
using std::string;

using Microsoft::WRL::ComPtr;

using glm::vec2;  using glm::vec3;  using glm::vec4;
using glm::uvec2; using glm::uvec3; using glm::uvec4;
using glm::ivec2; using glm::ivec3; using glm::ivec4;
using glm::bvec2; using glm::bvec3; using glm::bvec4;
using glm::mat4;

typedef glm::vec<2, float, glm::packed_highp> packed_vec2;
typedef glm::vec<3, float, glm::packed_highp> packed_vec3;
typedef glm::vec<4, float, glm::packed_highp> packed_vec4;

static const uint32_t MAX_FRAME_COUNT = 10;

/*
Returns true if given number is a power of two.
T must be unsigned integer number or signed integer but always nonnegative.
For 0 returns true.
*/
template <typename T>
inline bool IsPow2(T x)
{
    return (x & (x-1)) == 0;
}

// Aligns given value up to nearest multiply of align value. For example: AlignUp(11, 8) = 16.
// Use types like UINT, uint64_t as T.
// alignment must be power of 2.
template <typename T>
static inline T AlignUp(T val, T alignment)
{
	return (val + alignment - 1) & ~(alignment - 1);
}
// Aligns given value down to nearest multiply of align value. For example: AlignUp(11, 8) = 8.
// Use types like UINT, uint64_t as T.
// alignment must be power of 2.
template <typename T>
static inline T AlignDown(T val, T alignment)
{
    return val & ~(alignment - 1);
}

// Division with mathematical rounding to nearest number.
template <typename T>
static inline T RoundDiv(T x, T y)
{
	return (x + (y / (T)2)) / y;
}
template <typename T>
static inline T DivideRoudingUp(T x, T y)
{
    return (x + y - 1) / y;
}

struct CloseHandleDeleter
{
    typedef HANDLE pointer;
    void operator()(HANDLE handle) const { CloseHandle(handle); }
};

struct Exception
{
    struct Entry
    {
        const wchar_t* m_file; // Can be null.
        uint32_t m_line;
        wstring m_message;
    };
    std::vector<Entry> m_entries;

    Exception() { }
    Exception(const wchar_t* file, uint32_t line, const wstr_view& msg)
    {
        Push(file, line, msg);
    }
    Exception(const wchar_t* file, uint32_t line, std::initializer_list<wstr_view> messages)
    {
        for(const auto& msg : messages)
            Push(file, line, msg);
    }

    void Push(const wchar_t* file, uint32_t line, const wstr_view& msg)
    {
        if(!msg.empty())
        {
            const wstr_view fileView{file};
            const size_t lastSlashPos = fileView.find_last_of(L"/\\");
            m_entries.push_back({file + lastSlashPos + 1, line, wstring(msg.data(), msg.size())});
        }
    }

    void Print() const;
};

wstring GetWinAPIErrorMessage();
wstring GetHRESULTErrorMessage(HRESULT hr);

// Use this macro to pass the 2 parameters to formatting function matching formatting string like "%.*s", "%.*hs" etc.
// for s of type like std::string, std::wstring, str_view, wstr_view.
#define STR_TO_FORMAT(s) (int)(s).size(), (s).data()

#define __TFILE__ _T(__FILE__)
#define FAIL(msgStr)  do { \
        throw Exception(__TFILE__, __LINE__, msgStr); \
    } while(false)
#define CHECK_BOOL(expr)  do { if(!(expr)) { \
        throw Exception(__TFILE__, __LINE__, L"(" _T(#expr) L") == false"); \
    } } while(false)
#define CHECK_BOOL_WINAPI(expr)  do { if(!(expr)) { \
        throw Exception(__TFILE__, __LINE__, {GetWinAPIErrorMessage(), L"(" _T(#expr) L") == false"}); \
    } } while(false)
#define CHECK_HR(expr)  do { HRESULT hr__ = (expr); if(FAILED(hr__)) { \
        throw Exception(__TFILE__, __LINE__, {GetHRESULTErrorMessage(hr__), L"FAILED(" #expr L")"}); \
    } } while(false)

#define ERR_TRY try {
#define ERR_CATCH_MSG(msgStr) } catch(Exception& ex) { ex.Push(__TFILE__, __LINE__, (msgStr)); throw; }
#define ERR_CATCH_FUNC } catch(Exception& ex) { ex.Push(__TFILE__, __LINE__, _T(__FUNCTION__)); throw; }

#define CATCH_PRINT_ERROR(extraCatchCode) \
    catch(const Exception& ex) \
    { \
        ex.Print(); \
        extraCatchCode \
    } \
    catch(const std::exception& ex) \
    { \
        LogErrorF(L"ERROR: %hs", ex.what()); \
        extraCatchCode \
    } \
    catch(...) \
    { \
        LogError(L"UNKNOWN ERROR."); \
        extraCatchCode \
    }

constexpr D3D12_RANGE* D3D12_RANGE_ALL = nullptr;
extern const D3D12_RANGE* D3D12_RANGE_NONE;

extern const D3D12_HEAP_PROPERTIES D3D12_HEAP_PROPERTIES_DEFAULT;
extern const D3D12_HEAP_PROPERTIES D3D12_HEAP_PROPERTIES_UPLOAD;
extern const D3D12_HEAP_PROPERTIES D3D12_HEAP_PROPERTIES_READBACK;

template<typename CharT>
void StringOffsetToRowCol(uint32_t& outRow, uint32_t& outCol, const str_view_template<CharT>& str, size_t offset)
{
    outRow = outCol = 1;
    for(size_t i = 0; i < offset; ++i)
    {
        if(str[i] == (CharT)'\n')
            ++outRow, outCol = 1;
        else
            ++outCol;
    }
}

// As codePage use e.g. CP_ACP (system ASCII code page), CP_UTF8, 125 for Windows-1250.
string ConvertUnicodeToChars(const wstr_view& str, uint32_t codePage);
wstring ConvertCharsToUnicode(const str_view& str, uint32_t codePage);

string VFormat(const char* format, va_list argList);
wstring VFormat(const wchar_t* format, va_list argList);
string Format(const char* format, ...);
wstring Format(const wchar_t* format, ...);

enum class LogLevel { Info, Message, Warning, Error, Count };
void Log(LogLevel level, const wstr_view& msg);
void LogV(LogLevel level, const wchar_t* format, va_list argList);
inline void LogF(LogLevel level, const wchar_t* format, ...) { va_list argList; va_start(argList, format); LogV(level, format, argList); va_end(argList); }

inline void LogInfo(const wstr_view& msg) { Log(LogLevel::Info, msg); }
inline void LogInfoV(const wchar_t* format, va_list argList) { LogV(LogLevel::Info, format, argList); }
inline void LogInfoF(const wchar_t* format, ...) { va_list argList; va_start(argList, format); LogV(LogLevel::Info, format, argList); va_end(argList); }
inline void LogMessage(const wstr_view& msg) { Log(LogLevel::Message, msg); }
inline void LogMessageV(const wchar_t* format, va_list argList) { LogV(LogLevel::Message, format, argList); }
inline void LogMessageF(const wchar_t* format, ...) { va_list argList; va_start(argList, format); LogV(LogLevel::Message, format, argList); va_end(argList); }
inline void LogWarning(const wstr_view& msg) { Log(LogLevel::Warning, msg); }
inline void LogWarningV(const wchar_t* format, va_list argList) { LogV(LogLevel::Warning, format, argList); }
inline void LogWarningF(const wchar_t* format, ...) { va_list argList; va_start(argList, format); LogV(LogLevel::Warning, format, argList); va_end(argList); }
inline void LogError(const wstr_view& msg) { Log(LogLevel::Error, msg); }
inline void LogErrorV(const wchar_t* format, va_list argList) { LogV(LogLevel::Error, format, argList); }
inline void LogErrorF(const wchar_t* format, ...) { va_list argList; va_start(argList, format); LogV(LogLevel::Error, format, argList); va_end(argList); }

std::vector<char> LoadFile(const wstr_view& path);
void SetThreadName(DWORD threadId, const str_view& name);
void SetD3D12ObjectName(ID3D12Object* obj, const wstr_view& name);
inline void SetD3D12ObjectName(const ComPtr<ID3D12Object>& obj, const wstr_view& name) { SetD3D12ObjectName(obj.Get(), name); }
// On error returns 0.
uint8_t DXGIFormatToBitsPerPixel(DXGI_FORMAT format);
