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
#include <span>
#include <filesystem>
#include <format>
#include <functional>

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cmath>
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
typedef glm::mat<4, 4, float, glm::packed_highp> packed_mat4;

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

/*
Multiply m * v, using only 3x3 submatrix of m.
In other words, extend v to homogeneous coordinates as (x, y, z, 0).
This disregards 4th column of m, which contains translation.

Good for:
- transforming vectors that represent direction not position, so they should not have translation applied,
- transforming vectors by matrices that contain only rotation and scaling, not translation.

The name of this function is a tradition from D3DX and DirectX Math libraries.
*/
template<typename T, enum glm::qualifier Q>
glm::vec<3, T, Q> TransformNormal(const glm::mat<4, 4, T, Q>& m, const glm::vec<3, T, Q>& v)
{
    auto result = m * glm::vec<4, T, Q>(v.x, v.y, v.z, (T)0.f);
    return {result.x, result.y, result.z};
}
/*
Multiply m * v, extending v to homogeneous coordinates as (x, y, z, 1).
Resulting vector w component is discarded.

Good for: transforming vectors that represent position by a matrix that may represent
any affine transformation (rotation, scaling, translation), but not perspective projection.

The name of this function is a tradition from D3DX and DirectX Math libraries.
*/
template<typename T, enum glm::qualifier Q>
glm::vec<3, T, Q> Transform(const glm::mat<4, 4, T, Q>& m, const glm::vec<3, T, Q>& v)
{
    auto result = m * glm::vec<4, T, Q>(v.x, v.y, v.z, (T)1.f);
    return {result.x, result.y, result.z};
}
/*
Multiply m * v, extending v to homogeneous coordinates as (x, y, z, 1).
Resulting vector will be (x/w, y/w, z/w).

Good for: transforming vectors that represent position by a matrix that may represent
any transformation, including perspective projection.

The name of this function is a tradition from D3DX and DirectX Math libraries.
*/
template<typename T, enum glm::qualifier Q>
glm::vec<3, T, Q> TransformCoord(const glm::mat<4, 4, T, Q>& m, const glm::vec<3, T, Q>& v)
{
    auto result = m * glm::vec<4, T, Q>(v.x, v.y, v.z, (T)1.f);
    T wInv = (T)1.f / result.w;
    return {result.x * wInv, result.y * wInv, result.z * wInv};
}

// These two are based on article: "A close look at the sRGB formula"
// https://entropymine.com/imageworsener/srgbformula/
inline float LinearToSRGB(float v)
{
    return v <= 0.00313066844250063f ?
        v * 12.92f :
        pow(v, 0.41666666666666666666666666666667f) * 1.055f - 0.055f;
}
inline float SRGBToLinear(float v)
{
    return v <= 0.0404482362771082f ?
        v * 0.07739938080495356037151702786378f :
        pow((v + 0.055f) * 0.94786729857819905213270142180095f, 2.4f);
}

// Custom deleter for STL smart pointers that uses HANDLE and CloseFile().
struct CloseHandleDeleter
{
    typedef HANDLE pointer;
    void operator()(HANDLE handle) const
    {
        if(handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
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
        LogErrorF(L"ERROR: {}", str_view(ex.what())); \
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

/*
Based on boost::hash_combine, as quoted on page:
https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
*/
inline size_t CombineHash(size_t lhs, size_t rhs)
{
    return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2));
}

template<>
struct std::hash<str_view>
{
    size_t operator()(const str_view& str) const
    {
        return std::hash<std::string_view>()(std::string_view(str.data(), str.length()));
    }
};
template<>
struct std::hash<wstr_view>
{
    size_t operator()(const wstr_view& str) const
    {
        return std::hash<std::wstring_view>()(std::wstring_view(str.data(), str.length()));
    }
};

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

void ToUpperCase(wstring& inoutStr);

std::filesystem::path StrToPath(const wstr_view& str);

enum class LogLevel { Info, Message, Warning, Error, Count };
void Log(LogLevel level, const wstr_view& msg);

template<typename... Args>
void LogF(LogLevel level, const wchar_t* format, Args&&... args) { Log(level, std::format(format, args...)); }

inline void LogInfo(const wstr_view& msg) { Log(LogLevel::Info, msg); }
inline void LogMessage(const wstr_view& msg) { Log(LogLevel::Message, msg); }
inline void LogWarning(const wstr_view& msg) { Log(LogLevel::Warning, msg); }
inline void LogError(const wstr_view& msg) { Log(LogLevel::Error, msg); }

template<typename... Args>
void LogInfoF(const wchar_t* format, Args&&... args) { LogF(LogLevel::Info, format, args...); }
template<typename... Args>
void LogMessageF(const wchar_t* format, Args&&... args) { LogF(LogLevel::Message, format, args...); }
template<typename... Args>
void LogWarningF(const wchar_t* format, Args&&... args) { LogF(LogLevel::Warning, format, args...); }
template<typename... Args>
void LogErrorF(const wchar_t* format, Args&&... args) { LogF(LogLevel::Error, format, args...); }

void SetThreadName(DWORD threadId, const str_view& name);
void SetD3D12ObjectName(ID3D12Object* obj, const wstr_view& name);
inline void SetD3D12ObjectName(const ComPtr<ID3D12Object>& obj, const wstr_view& name) { SetD3D12ObjectName(obj.Get(), name); }
// On error returns 0.
uint8_t DXGIFormatToBitsPerPixel(DXGI_FORMAT format);

void FillShaderResourceViewDesc_Texture2D(D3D12_SHADER_RESOURCE_VIEW_DESC& dst,
    DXGI_FORMAT format);
void FillRasterizerDesc(D3D12_RASTERIZER_DESC& dst,
    D3D12_CULL_MODE cullMode, BOOL frontCounterClockwise);
void FillRasterizerDesc_NoCulling(D3D12_RASTERIZER_DESC& dst);
void FillBlendDesc_BlendRT0(D3D12_BLEND_DESC& dst,
    D3D12_BLEND srcBlend, D3D12_BLEND destBlend, D3D12_BLEND_OP blendOp,
    D3D12_BLEND srcBlendAlpha, D3D12_BLEND destBlendAlpha, D3D12_BLEND_OP blendOpAlpha);
void FillBlendDesc_NoBlending(D3D12_BLEND_DESC& dst);
void FillDepthStencilDesc_NoTests(D3D12_DEPTH_STENCIL_DESC& dst);
void FillDepthStencilDesc_DepthTest(D3D12_DEPTH_STENCIL_DESC& dst,
    D3D12_DEPTH_WRITE_MASK depthWriteMask, D3D12_COMPARISON_FUNC depthFunc);
void FillRootParameter_DescriptorTable(D3D12_ROOT_PARAMETER& dst,
    const D3D12_DESCRIPTOR_RANGE* descriptorRange, D3D12_SHADER_VISIBILITY shaderVisibility);

// Returns true if file exists and is a file not directory or something else. Doesn't throw exceptions.
bool FileExists(const std::filesystem::path& path);
bool GetFileLastWriteTime(std::filesystem::file_time_type& outTime, const std::filesystem::path& path);

#include "Formatters.inl"
