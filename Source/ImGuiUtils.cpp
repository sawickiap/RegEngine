#include "BaseUtils.hpp"
#include "ImGuiUtils.hpp"
#include "Settings.hpp"
#include "../ThirdParty/imgui-1.87/backends/imgui_impl_win32.h"
#include "../ThirdParty/imgui-1.87/backends/imgui_impl_dx12.h"

static StringSetting g_ImGuiFontFilePath(SettingCategory::Startup, "ImGui.Font.FilePath");
static FloatSetting g_ImGuiFontSize(SettingCategory::Startup, "ImGui.Font.Size", 16.f);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiUtils::ImGuiUtils()
{

}

void ImGuiUtils::Init(HWND wnd)
{
    assert(wnd);

    CHECK_BOOL(!g_ImGuiFontFilePath.GetValue().empty());
    CHECK_BOOL(g_ImGuiFontSize.GetValue() > 0.f);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard |
        ImGuiConfigFlags_NavEnableSetMousePos |
        ImGuiConfigFlags_IsSRGB;
    io.BackendFlags = ImGuiBackendFlags_HasMouseCursors |
        ImGuiBackendFlags_HasSetMousePos |
        ImGuiBackendFlags_RendererHasVtxOffset;
    const ImWchar glyphRanges[] = {
        32, 127,
        // Polish diacritics
        0x0104, 0x0105,
        0x0106, 0x0107,
        0x0118, 0x0119,
        0x0141, 0x0142,
        0x0143, 0x0144,
        0x00D3, 0x00D3,
        0x00F3, 0x00F3,
        0x015A, 0x015B,
        0x0179, 0x017C};
    const string filePath = g_ImGuiFontFilePath.GetValue();
    ImFont* const font = io.Fonts->AddFontFromFileTTF(filePath.c_str(), g_ImGuiFontSize.GetValue(), nullptr, glyphRanges);
    CHECK_BOOL(font);

    const ImWchar awesomeGlyphRanges[] = {
        0xf002, 0xf002, // ICON_FA_MAGNIFYING_GLASS
        0xf00d, 0xf00d}; // ICON_FA_XMARK
    ImFontConfig awesomeConfig;
    awesomeConfig.MergeMode = true;
    ImFont* const awesomeFont = io.Fonts->AddFontFromFileTTF(
        "Data/Fonts/Font Awesome 6 Free-Solid-900.otf",
        g_ImGuiFontSize.GetValue(), &awesomeConfig, awesomeGlyphRanges);
    CHECK_BOOL(awesomeFont);

    CHECK_BOOL(io.Fonts->Build());
    
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
}

LRESULT ImGuiUtils::WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return ImGui_ImplWin32_WndProcHandler(wnd, msg, wParam, lParam);
}
