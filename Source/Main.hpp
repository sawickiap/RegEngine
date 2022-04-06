#pragma once

#include "Time.hpp"

class Renderer;
class Game;
class ImGuiUtils;

typedef int ImGuiMouseCursor;

// Use constants like IDC_ARROW, IDC_CROSS, NULL for no cursor.
typedef LPWSTR NativeCursorID;

/*
Represents the main object responsible for application initialization and management
of a Windows window.
*/
class Application
{
public:
    Application();
    ~Application();
    void Init();
    void InitWindowAndRenderer();
    int Run();

    HINSTANCE GetInsatnce() const { return m_Instance; }
    HWND GetWnd() const { return m_Wnd; }
    IDXGIFactory4* GetFactory4() const { return m_DXGIFactory4.Get(); }
    IDXGIFactory6* GetFactory6() const { return m_DXGIFactory6.Get(); }
    IDXGIAdapter1* GetAdapter() const { return m_Adapter.Get(); }
    const TimeData& GetTime() const { return m_Time; }

    void Exit();
    void SetCursor(NativeCursorID id);

private:
    HINSTANCE m_Instance = NULL;
    HWND m_Wnd = NULL;
    ComPtr<IDXGIFactory4> m_DXGIFactory4;
    ComPtr<IDXGIFactory6> m_DXGIFactory6;
    ComPtr<IDXGIAdapter1> m_Adapter;
    unique_ptr<ImGuiUtils> m_ImGuiContext;
    unique_ptr<Renderer> m_Renderer;
    unique_ptr<Game> m_Game;
    NativeCursorID m_NativeCursorID = IDC_CROSS;
    ImGuiMouseCursor m_LastImGuiMouseCursor = -1; // ImGuiMouseCursor_None
    TimeData m_Time;

    static LRESULT WINAPI GlobalWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static uint32_t GetKeyModifiers(); // Returns bit flags KEY_MODIFIERS.
    void LoopIteration();
    void SelectAdapter();
    void RegisterWindowClass();
    void CreateWindow_();
    LRESULT WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnDestroy();
    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);
    void OnChar(wchar_t ch);
};

extern Application* g_App;
