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

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <cstdarg>

using std::unique_ptr;
using std::make_unique;
using std::wstring;
using std::string;

using Microsoft::WRL::ComPtr;

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4x4;

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define FAIL(msgStr)  do { \
        assert(0 && msgStr); \
        throw std::runtime_error(__FILE__ "(" LINE_STRING "): " msgStr); \
    } while(false)
#define CHECK_BOOL(expr)  do { if(!(expr)) { \
        assert(0 && #expr); \
        throw std::runtime_error(__FILE__ "(" LINE_STRING "): ( " #expr " ) == false"); \
    } } while(false)
#define CHECK_HR(expr)  do { if(FAILED(expr)) { \
        assert(0 && #expr); \
        throw std::runtime_error(__FILE__ "(" LINE_STRING "): FAILED( " #expr " )"); \
    } } while(false)

#define CATCH_PRINT_ERROR(extraCatchCode) \
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

struct CloseHandleDeleter
{
    typedef HANDLE pointer;
    void operator()(HANDLE handle) const { CloseHandle(handle); }
};

string VFormat(const str_view& format, va_list argList);
wstring VFormat(const wstr_view& format, va_list argList);
string Format(const str_view& format, ...);
wstring Format(const wstr_view& format, ...);

std::vector<char> LoadFile(const wstr_view& path);
void SetThreadName(DWORD threadId, const str_view& name);
