#include "BaseUtils.hpp"
#include "Main.hpp"
#include "Game.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"
#include "SmallFileCache.hpp"
#include "ImGuiUtils.hpp"
#include "Time.hpp"
#include <windowsx.h>

enum EXIT_CODE
{
	EXIT_CODE_SUCCESS = 0,
	EXIT_CODE_RUNTIME_ERROR = -1,
};

static const wchar_t* const CLASS_NAME = L"REG_ENGINE_1";
static const wchar_t* const WINDOW_TITLE = L"RegEngine";

VecSetting<uvec2> g_Size(SettingCategory::Startup, "Size", uvec2(1024, 576));
VecSetting<uvec2> g_FrameRandomSleep(SettingCategory::Runtime, "FrameRandomSleep", uvec2(0, 0));

static const NativeCursorID SYSTEM_CURSOR_IDENTIFIERS[] = {
    IDC_APPSTARTING,
    IDC_ARROW,
    IDC_CROSS,
    IDC_HAND,
    IDC_HELP,
    IDC_IBEAM,
    IDC_NO,
    IDC_SIZEALL,
    IDC_SIZENESW,
    IDC_SIZENS,
    IDC_SIZENWSE,
    IDC_SIZEWE,
    IDC_UPARROW,
    IDC_WAIT,
};

static NativeCursorID ImGuiCursorToNativeCursorID(ImGuiMouseCursor imGuiCursor)
{
    switch(imGuiCursor)
    {
    case ImGuiMouseCursor_None: return NULL;
    case ImGuiMouseCursor_Arrow: return IDC_ARROW;
    case ImGuiMouseCursor_TextInput: return IDC_IBEAM;
    case ImGuiMouseCursor_ResizeAll: return IDC_SIZEALL;
    case ImGuiMouseCursor_ResizeNS: return IDC_SIZENS;
    case ImGuiMouseCursor_ResizeEW: return IDC_SIZEWE;
    case ImGuiMouseCursor_ResizeNESW: return IDC_SIZENESW;
    case ImGuiMouseCursor_ResizeNWSE: return IDC_SIZENWSE;
    case ImGuiMouseCursor_Hand: return IDC_HAND;
    case ImGuiMouseCursor_NotAllowed: return IDC_NO;
    default:
        assert(0);
        return NULL;
    }
}

class SystemCursors
{
public:
    SystemCursors();
    HCURSOR GetCursor(const NativeCursorID id) const;
    void SetCursorFromImGui(ImGuiMouseCursor imGuiCursor) const;
    void SetCursorFromID(const NativeCursorID id) const;

private:
    HCURSOR m_Cursors[_countof(SYSTEM_CURSOR_IDENTIFIERS)];
};

static SystemCursors g_SystemCursors;

SystemCursors::SystemCursors()
{
    for(size_t i = 0; i < _countof(SYSTEM_CURSOR_IDENTIFIERS); ++i)
    {
        m_Cursors[i] = LoadCursor(NULL, SYSTEM_CURSOR_IDENTIFIERS[i]);
        assert(m_Cursors[i]);
    }
}

HCURSOR SystemCursors::GetCursor(const NativeCursorID id) const
{
    switch((ULONG_PTR)id)
    {
    case (ULONG_PTR)IDC_APPSTARTING: return m_Cursors[0];
    case (ULONG_PTR)IDC_ARROW: return m_Cursors[1];
    case (ULONG_PTR)IDC_CROSS: return m_Cursors[2];
    case (ULONG_PTR)IDC_HAND: return m_Cursors[3];
    case (ULONG_PTR)IDC_HELP: return m_Cursors[4];
    case (ULONG_PTR)IDC_IBEAM: return m_Cursors[5];
    case (ULONG_PTR)IDC_NO: return m_Cursors[6];
    case (ULONG_PTR)IDC_SIZEALL: return m_Cursors[7];
    case (ULONG_PTR)IDC_SIZENESW: return m_Cursors[8];
    case (ULONG_PTR)IDC_SIZENS: return m_Cursors[9];
    case (ULONG_PTR)IDC_SIZENWSE: return m_Cursors[10];
    case (ULONG_PTR)IDC_SIZEWE: return m_Cursors[11];
    case (ULONG_PTR)IDC_UPARROW: return m_Cursors[12];
    case (ULONG_PTR)IDC_WAIT: return m_Cursors[13];
    default:
        assert(0);
        return NULL;
    }
}

void SystemCursors::SetCursorFromImGui(ImGuiMouseCursor imGuiCursor) const
{
    const NativeCursorID id = ImGuiCursorToNativeCursorID(imGuiCursor);
    SetCursorFromID(id);
}

void SystemCursors::SetCursorFromID(const NativeCursorID id) const
{
    if(id)
    {
        const HCURSOR nativeCursor = GetCursor(id);
        SetCursor(nativeCursor);
    }
    else
        SetCursor(NULL);
}

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

#ifndef _FPS_CALCULATOR_IMPL

FPSCalculator::FPSCalculator()
{
    m_CalcMinInterval = MillisecondsToTime(CALC_MIN_INTERVAL_MILLISECONDS);
}

void FPSCalculator::Start(const TimeData& appTime)
{
    m_LastCalcTime = appTime.m_Time;
    m_FrameCount = 0.f;
    m_FPS = 0.f;
}

void FPSCalculator::NewFrame(const TimeData& appTime)
{
    m_FrameCount += 1.f;
    if(appTime.m_Time >= m_LastCalcTime + m_CalcMinInterval)
    {
        m_FPS = m_FrameCount / TimeToSeconds<float>(appTime.m_Time - m_LastCalcTime);
        m_LastCalcTime = appTime.m_Time;
        m_FrameCount = 0.f;
    }
}

#endif // #ifndef _FPS_CALCULATOR_IMPL

#ifndef _APPLICATION_IMPL

Application* g_App;

Application::Application()
{
    m_Instance = (HINSTANCE)GetModuleHandle(NULL);
    SetThreadName(GetCurrentThreadId(), "MAIN");
}

Application::~Application()
{
    if(m_ExitWithFailure)
        LogMessage(L"Exiting with failure. Runtime settings are not saved.");
    else
        SaveRuntimeSettings();

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
    LoadRuntimeSettings();
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
    m_ImGuiContext = make_unique<ImGuiUtils>();
    m_ImGuiContext->Init(m_Wnd);
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
    wndClass.hCursor = g_SystemCursors.GetCursor(m_NativeCursorID);
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

    m_ImGuiContext->WndProc(wnd, msg, wParam, lParam);

    switch(msg)
    {
    case WM_CREATE:
        return 0;
    case WM_DESTROY:
        try
        {
            OnDestroy();
        }
        CATCH_PRINT_ERROR(m_ExitWithFailure=true;)
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
        try
        {
            if(!ImGui::GetIO().WantCaptureKeyboard)
            {
                if(msg == WM_KEYDOWN)
                    OnKeyDown(wParam);
                else // WM_KEYUP
                    OnKeyUp(wParam);
            }
        }
        CATCH_PRINT_ERROR(m_ExitWithFailure=true; DestroyWindow(m_Wnd);)
        return 0;
    case WM_CHAR:
        try
        {
            if(!ImGui::GetIO().WantCaptureKeyboard)
            {
                OnChar((wchar_t)wParam);
            }
        }
        CATCH_PRINT_ERROR(DestroyWindow(m_Wnd);)
        return 0;
    case WM_MOUSEMOVE:
        try
        {
            if(ImGui::GetIO().WantCaptureMouse)
            {
                g_SystemCursors.SetCursorFromImGui(m_LastImGuiMouseCursor);
            }
            else
            {
                const uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(wParam);
                const ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                m_Game->OnMouseMove(mouseButtonDownFlag, pos);
                g_SystemCursors.SetCursorFromID(m_NativeCursorID);
            }
        }
        CATCH_PRINT_ERROR(m_ExitWithFailure=true; DestroyWindow(m_Wnd);)
        return 0;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        try
        {
            if(ImGui::GetIO().WantCaptureMouse)
            {
                g_SystemCursors.SetCursorFromImGui(m_LastImGuiMouseCursor);
            }
            else
            {
                const MouseButton button = msg == WM_LBUTTONDOWN ? MouseButton::Left :
                    msg == WM_MBUTTONDOWN ? MouseButton::Middle :
                    MouseButton::Right;
                const uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(wParam);
                const ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                m_Game->OnMouseDown(button, mouseButtonDownFlag, pos);
                g_SystemCursors.SetCursorFromID(m_NativeCursorID);
            }
        }
        CATCH_PRINT_ERROR(m_ExitWithFailure=true; DestroyWindow(m_Wnd);)
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        try
        {
            if(ImGui::GetIO().WantCaptureMouse)
            {
                g_SystemCursors.SetCursorFromImGui(m_LastImGuiMouseCursor);
            }
            else
            {
                const MouseButton button = msg == WM_LBUTTONUP ? MouseButton::Left :
                    msg == WM_MBUTTONUP ? MouseButton::Middle :
                    MouseButton::Right;
                const uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(wParam);
                const ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                m_Game->OnMouseUp(button, mouseButtonDownFlag, pos);
                g_SystemCursors.SetCursorFromID(m_NativeCursorID);
            }
        }
        CATCH_PRINT_ERROR(m_ExitWithFailure=true; DestroyWindow(m_Wnd);)
        return 0;
    case WM_MOUSEWHEEL:
        try
        {
            if(ImGui::GetIO().WantCaptureMouse)
            {
                g_SystemCursors.SetCursorFromImGui(m_LastImGuiMouseCursor);
            }
            else
            {
                const int16_t distance = GET_WHEEL_DELTA_WPARAM(wParam);
                const uint32_t mouseButtonDownFlag = WParamToMouseButtonDownFlags(GET_KEYSTATE_WPARAM(wParam));
                const ivec2 pos = ivec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                m_Game->OnMouseWheel(distance, mouseButtonDownFlag, pos);
                g_SystemCursors.SetCursorFromID(m_NativeCursorID);
            }
        }
        CATCH_PRINT_ERROR(m_ExitWithFailure=true; DestroyWindow(m_Wnd);)
        return 0;
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
        Exit(false);
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
        if(m_Game)
            m_Game->Reload(refreshAll);
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
    try
    {
        LogMessage(L"Application initialized, running...");
        const Time now = Now();
        m_Time.Start(now);
        m_FPSCalculator.Start(m_Time);
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
                if(m_Renderer)
                    LoopIteration();
            }
        }
        LogMessage(L"Application exiting.");
        return (int)msg.wParam;
    }
    CATCH_PRINT_ERROR(m_ExitWithFailure=true; return EXIT_CODE_RUNTIME_ERROR;)
}

void Application::LoopIteration()
{
    const Time now = Now();
    m_Time.NewFrame(now);
    m_FPSCalculator.NewFrame(m_Time);

    m_ImGuiContext->NewFrame();
    m_Game->Update();

    const ImGuiMouseCursor newImGuiMouseCursor = ImGui::GetMouseCursor();
    if (newImGuiMouseCursor != m_LastImGuiMouseCursor)
    {
        g_SystemCursors.SetCursorFromImGui(newImGuiMouseCursor);
        m_LastImGuiMouseCursor = newImGuiMouseCursor;
    }

    m_Renderer->Render();

    FrameRandomSleep();
}

void Application::FrameRandomSleep()
{
    uvec2 range = g_FrameRandomSleep.GetValue();
    range.y = std::max(range.x, range.y);
    if(range.y > 0)
    {
        uint32_t val = range.x;
        if(range.y > range.x)
            val += rand() % (range.y - range.x);
        if(val)
            Sleep(val);
    }
}

void Application::Exit(bool failure)
{
    if(failure)
        m_ExitWithFailure = true;
    PostMessage(m_Wnd, WM_CLOSE, 0, 0);
}

void Application::SetCursor(NativeCursorID id)
{
    if(id != m_NativeCursorID)
    {
        if(m_Wnd && !ImGui::GetIO().WantCaptureMouse)
        {
            g_SystemCursors.SetCursorFromID(id);
        }
        m_NativeCursorID = id;
    }
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

        InitTime(); // Must be called before Application constructor.
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
