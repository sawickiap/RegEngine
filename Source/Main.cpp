#include "BaseUtils.hpp"
#include "Main.hpp"
#include "Game.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"
#include "SmallFileCache.hpp"
#include <windowsx.h>

enum EXIT_CODE
{
	EXIT_CODE_SUCCESS = 0,
	EXIT_CODE_RUNTIME_ERROR = -1,
};

static const wchar_t* const CLASS_NAME = L"REG_ENGINE_1";
static const wchar_t* const WINDOW_TITLE = L"RegEngine";

VecSetting<glm::uvec2> g_Size(SettingCategory::Startup, "Size", glm::uvec2(1024, 576));

struct ApplicationParameters
{
    enum class Mode { Normal, AssimpPrint };
    Mode m_Mode = Mode::Normal;
    wstring m_Path; // When m_Mode == Mode::AssimpPrint

    int ParseCommandLine(int argc, wchar_t** argv);
};

int ApplicationParameters::ParseCommandLine(int argc, wchar_t** argv)
{
    enum OPTION
    {
        OPTION_ASSIMP_PRINT,
        OPTION_COUNT
    };
    static const wchar_t* ERR_MSG = L"Command line syntax error.";

    CommandLineParser parser(argc, argv);
    parser.RegisterOption(OPTION_ASSIMP_PRINT, L"AssimpPrint", true);
    for(;;)
    {
        CommandLineParser::Result res = parser.ReadNext();
        switch(res)
        {
        case CommandLineParser::Result::End:
            return 0;
        case CommandLineParser::Result::Option:
            switch(parser.GetOptionID())
            {
            case OPTION_ASSIMP_PRINT:
                if(m_Mode == Mode::Normal)
                {
                    m_Mode = Mode::AssimpPrint;
                    m_Path = parser.GetParameter();
                }
                else
                    FAIL(ERR_MSG);
                break;
            default:
                assert(0);
            }
            break;
        default:
            FAIL(ERR_MSG);
        }
    }
}

#ifndef _APPLICATION_IMPL

Application* g_App;

Application::Application()
{
    m_Instance = (HINSTANCE)GetModuleHandle(NULL);
    SetThreadName(GetCurrentThreadId(), "MAIN");
}

Application::~Application()
{
    delete g_SmallFileCache;
    g_SmallFileCache = nullptr;
}

void Application::Init()
{
    ERR_TRY;

    LogMessage(L"Application starting.");
    CHECK_HR(CoInitialize(NULL));
    CHECK_HR(CreateDXGIFactory1(IID_PPV_ARGS(&m_DXGIFactory4)));
    CHECK_HR(m_DXGIFactory4->QueryInterface(IID_PPV_ARGS(&m_DXGIFactory6)));
    g_SmallFileCache = new SmallFileCache{};
    LoadStartupSettings();
    LoadLoadSettings();
    SelectAdapter();
    assert(m_Adapter);

    ERR_CATCH_MSG(L"Failed to initialize application.");
}

void Application::InitWindowAndRenderer()
{
    ERR_TRY;

    RegisterWindowClass();
    CreateWindow_();
    m_Renderer = make_unique<Renderer>(m_DXGIFactory4.Get(), m_Adapter.Get(), m_Wnd);
    m_Renderer->Init();
    m_Game = std::make_unique<Game>();
    m_Game->Init();
    
    ERR_CATCH_MSG(L"Failed to initialize window and renderer.");
}

void Application::SelectAdapter()
{
    ComPtr<IDXGIAdapter1> tmpAdapter;
    for(UINT i = 0; m_DXGIFactory6->EnumAdapterByGpuPreference(
        i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&tmpAdapter)) != DXGI_ERROR_NOT_FOUND; ++i)
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

uint32_t Application::GetKeyModifiers()
{
    uint32_t modifiers = 0;
    if((GetKeyState(VK_CONTROL) & 0x8000) != 0)
        modifiers |= KEY_MODIFIER_CONTROL;
    if((GetKeyState(VK_MENU) & 0x8000) != 0)
        modifiers |= KEY_MODIFIER_ALT;
    if((GetKeyState(VK_SHIFT) & 0x8000) != 0)
        modifiers |= KEY_MODIFIER_SHIFT;
    return modifiers;
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
    case WM_KEYUP:
        try
        {
            if(msg == WM_KEYDOWN)
                OnKeyDown(wParam);
            else // WM_KEYUP
                OnKeyUp(wParam);
        }
        CATCH_PRINT_ERROR(DestroyWindow(m_Wnd);)
        return 0;
    case WM_CHAR:
        try
        {
            OnChar((wchar_t)wParam);
        }
        CATCH_PRINT_ERROR(DestroyWindow(m_Wnd);)
        return 0;
    case WM_MOUSEMOVE:
    {
        const uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(wParam);
        const ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        m_Game->OnMouseMove(mouseButtonDownFlag, pos);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        SetCapture(m_Wnd);
        const MouseButton button = msg == WM_LBUTTONDOWN ? MouseButton::Left :
            msg == WM_MBUTTONDOWN ? MouseButton::Middle :
            MouseButton::Right;
        const uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(wParam);
        const ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        m_Game->OnMouseDown(button, mouseButtonDownFlag, pos);
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    {
        const MouseButton button = msg == WM_LBUTTONUP ? MouseButton::Left :
            msg == WM_MBUTTONUP ? MouseButton::Middle :
            MouseButton::Right;
        const uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(wParam);
        const ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        m_Game->OnMouseUp(button, mouseButtonDownFlag, pos);
        ReleaseCapture();
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        const int16_t distance = GET_WHEEL_DELTA_WPARAM(wParam);
        const uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(GET_KEYSTATE_WPARAM(wParam));
        const ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        m_Game->OnMouseWheel(distance, mouseButtonDownFlag, pos);
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
    const uint32_t modifiers = GetKeyModifiers();

    // ESC: Exit.
    if(key == VK_ESCAPE && modifiers == 0)
    {
        PostMessage(m_Wnd, WM_CLOSE, 0, 0);
        return;
    }
    
    // F5: Refresh. Ctrl+F5: Force-refresh.
    if(key == VK_F5 && (modifiers & ~KEY_MODIFIER_CONTROL) == 0)
    {
        const bool refreshAll = (modifiers & KEY_MODIFIER_CONTROL) != 0;
        if(refreshAll)
            g_SmallFileCache->Clear();
        LoadLoadSettings();
        if(g_Renderer)
            g_Renderer->Reload(refreshAll);
        return;
    }

    m_Game->OnKeyDown(key, modifiers);
}

void Application::OnKeyUp(WPARAM key)
{
    const uint32_t modifiers = GetKeyModifiers();

    m_Game->OnKeyUp(key, modifiers);
}

void Application::OnChar(wchar_t ch)
{
    m_Game->OnChar(ch);
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
                m_Game->Update();
                m_Renderer->Render();
            }
        }
    }
    LogMessage(L"Application exiting.");
    return (int)msg.wParam;
}

#endif // _APPLICATION_IMPL

int wmain(int argc, wchar_t** argv)
{
	try
	{
        ApplicationParameters params;
        int result = params.ParseCommandLine(argc, argv);
        if(result != 0)
            return result;

        Application app;
        g_App = &app;
        app.Init();

        switch(params.m_Mode)
        {
        case ApplicationParameters::Mode::Normal:
            app.InitWindowAndRenderer();
            result = app.Run();
            break;
        case ApplicationParameters::Mode::AssimpPrint:
            AssimpPrint(params.m_Path);
            break;
        default:
            assert(0);
        }
        
        return result;
	}
	CATCH_PRINT_ERROR(return EXIT_CODE_RUNTIME_ERROR;);
}
