#include "BaseUtils.hpp"
#include "Settings.hpp"
#include "Renderer.hpp"

enum EXIT_CODE
{
	EXIT_CODE_SUCCESS = 0,
	EXIT_CODE_RUNTIME_ERROR = -1,
};

static const wchar_t* const CLASS_NAME = L"REG_ENGINE_1";
static const wchar_t* const WINDOW_TITLE = L"RegEngine";

VecSetting<glm::uvec2> g_size(SettingCategory::Startup, "Size", glm::uvec2(1024, 576));

class Application
{
public:
    Application();
    void Init();
    int Run();

private:
    HINSTANCE m_instance = NULL;
    HWND m_wnd = NULL;
    ComPtr<IDXGIFactory4> m_dxgiFactory;
    ComPtr<IDXGIAdapter1> m_adapter;
    unique_ptr<Renderer> m_renderer;

    static LRESULT WINAPI GlobalWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void SelectAdapter();
    void RegisterWindowClass();
    void CreateWindow_();
    LRESULT WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnDestroy();
    void OnKeyDown(WPARAM key);
};

static Application* g_app;

Application::Application()
{
    m_instance = (HINSTANCE)GetModuleHandle(NULL);
    SetThreadName(GetCurrentThreadId(), "MAIN");
}

void Application::Init()
{
    ERR_TRY;
    wprintf(L"Application starting.\n");
    CHECK_HR(CoInitialize(NULL));
    CHECK_HR(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));
    LoadStartupSettings();
    LoadLoadSettings();
    SelectAdapter();
    assert(m_adapter);
    RegisterWindowClass();
    CreateWindow_();
    m_renderer = make_unique<Renderer>(m_dxgiFactory.Get(), m_adapter.Get(), m_wnd);
    m_renderer->Init();
    ERR_CATCH_MSG(L"Failed to initialize application.");
}

void Application::SelectAdapter()
{
    ComPtr<IDXGIAdapter1> tmpAdapter;
    for(UINT i = 0; m_dxgiFactory->EnumAdapters1(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        tmpAdapter->GetDesc1(&desc);
        if((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
        {
            m_adapter = std::move(tmpAdapter);
            return;
        }
    }
    FAIL(L"Couldn't find suitable DXGI adapter.");
}

LRESULT WINAPI Application::GlobalWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    assert(g_app);
    return g_app->WndProc(wnd, msg, wParam, lParam);
}

void Application::RegisterWindowClass()
{
    WNDCLASSEX wndClass = {};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    wndClass.hbrBackground = NULL;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hInstance = m_instance;
    wndClass.lpfnWndProc = &GlobalWndProc;
    wndClass.lpszClassName = CLASS_NAME;

    ATOM classR = RegisterClassEx(&wndClass);
    assert(classR);
}

void Application::CreateWindow_()
{
    assert(m_instance);
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    constexpr DWORD exStyle = 0;
    RECT rect = {0, 0, (int)g_size.GetValue().x, (int)g_size.GetValue().y};
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    m_wnd = CreateWindowEx(
        exStyle,
        CLASS_NAME,
        WINDOW_TITLE,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL,
        NULL,
        m_instance,
        NULL);
    assert(m_wnd);
}

LRESULT Application::WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if(m_wnd == NULL)
        m_wnd = wnd;

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
        CATCH_PRINT_ERROR(DestroyWindow(m_wnd);)
        return 0;
    }

    return DefWindowProc(wnd, msg, wParam, lParam);
}

void Application::OnDestroy()
{
    m_renderer.reset();
}

void Application::OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case VK_F5:
        LoadLoadSettings();
        break;

    case VK_ESCAPE:
        PostMessage(m_wnd, WM_CLOSE, 0, 0);
        break;
    }
}

int Application::Run()
{
    wprintf(L"Application initialized, running...\n");
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

            //Update();
            if(m_renderer)
                m_renderer->Render();
        }
    }
    wprintf(L"Application exiting.\n");
    return (int)msg.wParam;
}

int wmain(int argc, wchar_t** argv)
{
	try
	{
        Application app;
        g_app = &app;
        app.Init();
        const int result = app.Run();
        g_app = nullptr;
        return result;
	}
	CATCH_PRINT_ERROR(return EXIT_CODE_RUNTIME_ERROR;);
}
