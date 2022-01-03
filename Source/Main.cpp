#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <dxgi1_4.h>
#include <d3d12.h>
#include "..\ThirdParty\d3dx12\d3dx12.h"

#include <exception>
#include <stdexcept>

#include <cstdio>
#include <cassert>

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

enum EXIT_CODE
{
	EXIT_CODE_SUCCESS = 0,
	EXIT_CODE_RUNTIME_ERROR = -1,
};

static const wchar_t* const CLASS_NAME = L"REG_ENGINE_1";
static const wchar_t* const WINDOW_TITLE = L"RegEngine";
static const int SIZE_X = 1024;
static const int SIZE_Y = 576;

static HINSTANCE g_Instance;
static HWND g_Wnd;

static ComPtr<IDXGIAdapter1> SelectAdapter(IDXGIFactory4* dxgiFactory)
{
    ComPtr<IDXGIAdapter1> tmpAdapter;
    for(UINT i = 0; dxgiFactory->EnumAdapters1(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        tmpAdapter->GetDesc1(&desc);
        if((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
            return tmpAdapter;
    }
    FAIL("Couldn't find suitable DXGI adapter.");
}

static void OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case VK_ESCAPE:
        PostMessage(g_Wnd, WM_CLOSE, 0, 0);
        break;
    }
}

static LRESULT WINAPI WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_DESTROY:
        try
        {
            //Cleanup();
        }
        CATCH_PRINT_ERROR(;)
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        try
        {
            OnKeyDown(wParam);
        }
        CATCH_PRINT_ERROR(DestroyWindow(wnd);)
        return 0;
    }

    return DefWindowProc(wnd, msg, wParam, lParam);
}

static void RegisterWindowClass()
{
    WNDCLASSEX wndClass = {};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    wndClass.hbrBackground = NULL;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hInstance = g_Instance;
    wndClass.lpfnWndProc = &WndProc;
    wndClass.lpszClassName = CLASS_NAME;

    ATOM classR = RegisterClassEx(&wndClass);
    assert(classR);
}

static void CreateWindow_()
{
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    constexpr DWORD exStyle = 0;
    RECT rect = { 0, 0, SIZE_X, SIZE_Y };
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    g_Wnd = CreateWindowEx(
        exStyle,
        CLASS_NAME,
        WINDOW_TITLE,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL,
        NULL,
        g_Instance,
        0);
    assert(g_Wnd);
}

static int main2()
{
    g_Instance = (HINSTANCE)GetModuleHandle(NULL);
    
    CoInitialize(NULL);
    
    ComPtr<IDXGIFactory4> dxgiFactory;
    CHECK_HR( CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) );

    ComPtr<IDXGIAdapter1> adapter = SelectAdapter(dxgiFactory.Get());

    RegisterWindowClass();
    
    CreateWindow_();

    /*InitD3D();
    g_TimeOffset = GetTickCount64();
    */

    MSG msg;
    for (;;)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            /*const UINT64 newTimeValue = GetTickCount64() - g_TimeOffset;
            g_TimeDelta = (float)(newTimeValue - g_TimeValue) * 0.001f;
            g_TimeValue = newTimeValue;
            g_Time = (float)newTimeValue * 0.001f;

            Update();
            Render();
            */
        }
    }
    return (int)msg.wParam;
}

int wmain(int argc, wchar_t** argv)
{
	try
	{
		return main2();
	}
	CATCH_PRINT_ERROR(return EXIT_CODE_RUNTIME_ERROR;);
}
