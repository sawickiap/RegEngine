#pragma once

#include "../ThirdParty/imgui-1.87/imgui.h"
#include "../ThirdParty/imgui-1.87/misc/cpp/imgui_stdlib.h"
#include "../ThirdParty/IconFontCppHeaders/IconsFontAwesome6.h"

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
