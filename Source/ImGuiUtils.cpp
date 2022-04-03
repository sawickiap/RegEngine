#include "BaseUtils.hpp"
#include "ImGuiUtils.hpp"
#include "../ThirdParty/imgui-1.87/backends/imgui_impl_win32.h"
#include "../ThirdParty/imgui-1.87/backends/imgui_impl_dx12.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiUtils::ImGuiUtils()
{

}

void ImGuiUtils::Init(HWND wnd)
{
    assert(wnd);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard |
        ImGuiConfigFlags_NavEnableSetMousePos |
        ImGuiConfigFlags_IsSRGB;
    io.BackendFlags = ImGuiBackendFlags_HasMouseCursors |
        ImGuiBackendFlags_HasSetMousePos |
        ImGuiBackendFlags_RendererHasVtxOffset;

    ImGui_ImplWin32_Init((void*)wnd);
}

ImGuiUtils::~ImGuiUtils()
{
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiUtils::NewFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Text("Hello, world!");
}

LRESULT ImGuiUtils::WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return ImGui_ImplWin32_WndProcHandler(wnd, msg, wParam, lParam);
}
