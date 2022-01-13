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
#include "..\ThirdParty\d3dx12\d3dx12.h"

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

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4x4;

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

wstring GetWinApiErrorMessage();
wstring GetHresultErrorMessage(HRESULT hr);

#define __TFILE__ _T(__FILE__)
#define FAIL(msgStr)  do { \
        throw Exception(__TFILE__, __LINE__, msgStr); \
    } while(false)
#define CHECK_BOOL(expr)  do { if(!(expr)) { \
        throw Exception(__TFILE__, __LINE__, L"(" _T(#expr) L") == false"); \
    } } while(false)
#define CHECK_BOOL_WINAPI(expr)  do { if(!(expr)) { \
        throw Exception(__TFILE__, __LINE__, {GetWinApiErrorMessage(), L"(" _T(#expr) L") == false"}); \
    } } while(false)
#define CHECK_HR(expr)  do { HRESULT hr__ = (expr); if(FAILED(hr__)) { \
        throw Exception(__TFILE__, __LINE__, {GetHresultErrorMessage(hr__), L"FAILED(" #expr L")"}); \
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
        fwprintf(stderr, L"ERROR: %hs\n", ex.what()); \
        extraCatchCode \
    } \
    catch(...) \
    { \
        fwprintf(stderr, L"UNKNOWN ERROR.\n"); \
        extraCatchCode \
    }

// As codePage use e.g. CP_ACP (system ASCII code page), CP_UTF8, 125 for Windows-1250.
string ConvertUnicodeToChars(const wstr_view& str, uint32_t codePage);
wstring ConvertCharsToUnicode(const str_view& str, uint32_t codePage);

string VFormat(const char* format, va_list argList);
wstring VFormat(const wchar_t* format, va_list argList);
string Format(const char* format, ...);
wstring Format(const wchar_t* format, ...);

std::vector<char> LoadFile(const wstr_view& path);
void SetThreadName(DWORD threadId, const str_view& name);
