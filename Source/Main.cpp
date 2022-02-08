#include "BaseUtils.hpp"
#include "Game.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"
#include <windowsx.h>

enum EXIT_CODE
{
	EXIT_CODE_SUCCESS = 0,
	EXIT_CODE_RUNTIME_ERROR = -1,
};

static const wchar_t* const CLASS_NAME = L"REG_ENGINE_1";
static const wchar_t* const WINDOW_TITLE = L"RegEngine";

VecSetting<glm::uvec2> g_Size(SettingCategory::Startup, "Size", glm::uvec2(1024, 576));

/*
Represents the main object responsible for application initialization and management
of a Windows window.
*/
class Application
{
public:
    Application();
    void Init();
    int Run();

private:
    HINSTANCE m_Instance = NULL;
    HWND m_Wnd = NULL;
    ComPtr<IDXGIFactory4> m_DXGIFactory;
    ComPtr<IDXGIAdapter1> m_Adapter;
    unique_ptr<Renderer> m_Renderer;
    Game m_Game;

    static LRESULT WINAPI GlobalWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void SelectAdapter();
    void RegisterWindowClass();
    void CreateWindow_();
    LRESULT WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnDestroy();
    void OnKeyDown(WPARAM key);
};

static Application* g_App;

Application::Application()
{
    m_Instance = (HINSTANCE)GetModuleHandle(NULL);
    SetThreadName(GetCurrentThreadId(), "MAIN");
}

void Application::Init()
{
    ERR_TRY;
    LogMessage(L"Application starting.");
    CHECK_HR(CoInitialize(NULL));
    CHECK_HR(CreateDXGIFactory1(IID_PPV_ARGS(&m_DXGIFactory)));
    LoadStartupSettings();
    LoadLoadSettings();
    SelectAdapter();
    assert(m_Adapter);
    RegisterWindowClass();
    CreateWindow_();
    m_Renderer = make_unique<Renderer>(m_DXGIFactory.Get(), m_Adapter.Get(), m_Wnd);
    m_Renderer->Init();
    m_Game.Init();
    ERR_CATCH_MSG(L"Failed to initialize application.");
}

void Application::SelectAdapter()
{
    ComPtr<IDXGIAdapter1> tmpAdapter;
    for(UINT i = 0; m_DXGIFactory->EnumAdapters1(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        tmpAdapter->GetDesc1(&desc);
        if((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
        {
            m_Adapter = std::move(tmpAdapter);
            return;
        }
    }
    FAIL(L"Couldn't find suitable DXGI adapter.");
}

LRESULT WINAPI Application::GlobalWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    assert(g_App);
    return g_App->WndProc(wnd, msg, wParam, lParam);
}

void Application::RegisterWindowClass()
{
    WNDCLASSEX wndClass = {};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    wndClass.hbrBackground = NULL;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hInstance = m_Instance;
    wndClass.lpfnWndProc = &GlobalWndProc;
    wndClass.lpszClassName = CLASS_NAME;

    ATOM classResult = RegisterClassEx(&wndClass);
    assert(classResult);
}

void Application::CreateWindow_()
{
    assert(m_Instance);
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    constexpr DWORD exStyle = 0;
    RECT rect = {0, 0, (int)g_Size.GetValue().x, (int)g_Size.GetValue().y};
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    m_Wnd = CreateWindowEx(
        exStyle,
        CLASS_NAME,
        WINDOW_TITLE,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL,
        NULL,
        m_Instance,
        NULL);
    assert(m_Wnd);
}

static uint32_t WParamToMouseButtonDownFlags(WPARAM wParam)
{
    uint32_t result = 0;
    if((wParam & MK_CONTROL) != 0)
        result |= MOUSE_BUTTON_DOWN_CONTROL;
    if((wParam & MK_LBUTTON) != 0)
        result |= MOUSE_BUTTON_DOWN_LBUTTON;
    if((wParam & MK_MBUTTON) != 0)
        result |= MOUSE_BUTTON_DOWN_MBUTTON;
    if((wParam & MK_RBUTTON) != 0)
        result |= MOUSE_BUTTON_DOWN_RBUTTON;
    if((wParam & MK_SHIFT) != 0)
        result |= MOUSE_BUTTON_DOWN_SHIFT;
    if((wParam & MK_XBUTTON1) != 0)
        result |= MOUSE_BUTTON_DOWN_XBUTTON1;
    if((wParam & MK_XBUTTON2) != 0)
        result |= MOUSE_BUTTON_DOWN_XBUTTON2;
    return result;
}

LRESULT Application::WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if(m_Wnd == NULL)
        m_Wnd = wnd;

    switch(msg)
    {
    case WM_CREATE:
        return 0;

    case WM_DESTROY:
        try
        {
            OnDestroy();
        }
        CATCH_PRINT_ERROR(;)
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        try
        {
            OnKeyDown(wParam);
        }
        CATCH_PRINT_ERROR(DestroyWindow(m_Wnd);)
        return 0;

    case WM_MOUSEMOVE:
    {
        uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(wParam);
        ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        m_Game.OnMouseMove(mouseButtonDownFlag, pos);
        return 0;
    }
    }

    return DefWindowProc(wnd, msg, wParam, lParam);
}

void Application::OnDestroy()
{
    m_Renderer.reset();
    g_Renderer = nullptr;
}

void Application::OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case VK_F5:
        LoadLoadSettings();
        if(g_Renderer)
            g_Renderer->Reload();
        return;

    case VK_ESCAPE:
        PostMessage(m_Wnd, WM_CLOSE, 0, 0);
        return;
    }

    m_Game.OnKeyDown(key);
}

int Application::Run()
{
    LogMessage(L"Application initialized, running...");
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
            */

            if(m_Renderer)
            {
                m_Game.Update();
                m_Renderer->Render();
            }
        }
    }
    LogMessage(L"Application exiting.");
    return (int)msg.wParam;
}

int wmain(int argc, wchar_t** argv)
{
	try
	{
        Application app;
        g_App = &app;
        app.Init();
        const int result = app.Run();
        return result;
	}
	CATCH_PRINT_ERROR(return EXIT_CODE_RUNTIME_ERROR;);
}
