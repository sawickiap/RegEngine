#pragma once

#include "../ThirdParty/imgui-1.87/imgui.h"

class ImGuiUtils
{
public:
    ImGuiUtils();
    void Init(HWND wnd);
    ~ImGuiUtils();
    void NewFrame();
    LRESULT WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
};
