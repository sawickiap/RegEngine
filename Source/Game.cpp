#include "BaseUtils.hpp"
#include "Main.hpp"
#include "Game.hpp"
#include "Renderer.hpp"
#include "Cameras.hpp"
#include "Settings.hpp"
#include "ImGuiUtils.hpp"
#include "../WorkingDir/Shaders/Include/ShaderConstants.h"

extern VecSetting<glm::uvec2> g_Size;

static VecSetting<vec2> g_FlyingCameraRotationSpeed(SettingCategory::Runtime, "FlyingCamera.RotationSpeed.RadiansPerPixel", vec2(-0.005, 0.005));
static FloatSetting g_FlyingCameraMovementSpeed(SettingCategory::Runtime, "FlyingCamera.MovementSpeed.UnitsPerSecond", 1.f);
static FloatSetting g_SceneTimeSpeed(SettingCategory::Runtime, "SceneTimeSpeed", 1.f);

FrameTimeHistory::Entry FrameTimeHistory::Get(size_t i) const
{
    i = (m_Back + m_Count - i - 1) % m_Capacity;
    return m_Entries[i];
}

void FrameTimeHistory::Post(float dt)
{
    m_Entries[m_Front] = {dt, log2(dt)};
    m_Front = (m_Front + 1) % m_Capacity;
    if(m_Count == m_Capacity)
        m_Back = m_Front;
    else
        ++m_Count;
}

Game::Game()
{

}

void Game::Init()
{
    m_SceneTime.Start(g_App->GetTime().m_Time);
}

Game::~Game()
{

}

void Game::Reload(bool refreshAll)
{
    m_SceneTime.Start(g_App->GetTime().m_Time);
}

void Game::Update()
{
    const TimeData& appTime = g_App->GetTime();

    m_FrameTimeHistory.Post(appTime.m_DeltaTime_Float);

    {
        Time deltaTime = {0};
        const float timeSpeed = std::max(0.f, g_SceneTimeSpeed.GetValue());
        if(!m_TimePaused && timeSpeed > 0.f)
        {
            deltaTime = appTime.m_DeltaTime;
            if(timeSpeed != 1.f)
            {
                const int64_t numerator = (int64_t)(timeSpeed * 1000.f + 0.5f);
                deltaTime.m_Value = deltaTime.m_Value * numerator / 1000;
            }
        }
        m_SceneTime.NewFrameFromDelta(deltaTime);
    }

    for(size_t li = 1; li < g_Renderer->m_Lights.size(); ++li)
    {
        Light& l = g_Renderer->m_Lights[li];
        if(l.m_Type == LIGHT_TYPE_DIRECTIONAL)
        {
            l.m_DirectionToLight_Position = vec3(
                sin(appTime.m_Time_Float * 0.3545f * (float)li + 13.1535f + (float)li),
                sin(appTime.m_Time_Float * 0.5435f * (float)li + 12.4553f + (float)li),
                sin(appTime.m_Time_Float * 0.7453f + 54.3253f + (float)li));
        }
    }

    // WSADQE camera control.
    ImGuiIO& io = ImGui::GetIO();
    if(m_MouseDragEnabled &&
        (io.KeyMods == 0 || io.KeyMods == ImGuiKeyModFlags_Shift))
    {
        FlyingCamera* cam = g_Renderer->GetCamera();
        vec3 v = vec3(0.f);
        const vec3 forward = cam->CalculateForward();
        const vec3 right = cam->CalculateRight();
        if(ImGui::IsKeyDown(ImGuiKey_S))
            v -= forward;
        if(ImGui::IsKeyDown(ImGuiKey_W))
            v += forward;
        if(ImGui::IsKeyDown(ImGuiKey_A))
            v -= right;
        if(ImGui::IsKeyDown(ImGuiKey_D))
            v += right;
        if(ImGui::IsKeyDown(ImGuiKey_Q))
            v -= vec3(0.f, 0.f, 1.f);
        if(ImGui::IsKeyDown(ImGuiKey_E))
            v += vec3(0.f, 0.f, 1.f);
        v *= g_FlyingCameraMovementSpeed.GetValue() * appTime.m_DeltaTime_Float;
        if(io.KeyMods & ImGuiKeyModFlags_Shift)
            v *= 4.f;
        cam->SetPosition(cam->GetPosition() + v);
    }

    ImGui();
}

void Game::OnKeyDown(WPARAM key, uint32_t modifiers)
{
    if(modifiers == 0)
    {
        switch(key)
        {
        case 'R':
        {
            FlyingCamera* cam = g_Renderer->GetCamera();
            cam->SetPosition(vec3(0.f));
            cam->SetYaw(0.f);
            cam->SetPitch(0.f);
            break;
        }
        case '0':
            g_Renderer->m_AmbientEnabled = !g_Renderer->m_AmbientEnabled;
            break;
        case '1':
            if(g_Renderer->m_Lights.size() > 0)
                g_Renderer->m_Lights[0].m_Enabled = !g_Renderer->m_Lights[0].m_Enabled;
            break;
        case '2':
            if(g_Renderer->m_Lights.size() > 1)
                g_Renderer->m_Lights[1].m_Enabled = !g_Renderer->m_Lights[1].m_Enabled;
            break;
        case '3':
            if(g_Renderer->m_Lights.size() > 1)
                g_Renderer->m_Lights[2].m_Enabled = !g_Renderer->m_Lights[2].m_Enabled;
            break;
        case VK_PAUSE:
            m_TimePaused = !m_TimePaused;
            break;
        }
    }
}

void Game::OnKeyUp(WPARAM key, uint32_t modifiers)
{

}

void Game::OnChar(wchar_t ch)
{
}

void Game::OnMouseDown(MouseButton button, uint32_t buttonDownFlags, const ivec2& pos)
{
    if(button == MouseButton::Right)
    {
        m_MouseDragPrevPos = pos;
        m_MouseDragEnabled = true;
        g_App->SetCursor(NULL);
    }
}

void Game::OnMouseUp(MouseButton button, uint32_t buttonDownFlags, const ivec2& pos)
{
    if(button == MouseButton::Right)
    {
        g_App->SetCursor(IDC_CROSS);
        m_MouseDragEnabled = false;
        m_MouseDragPrevPos = ivec2(INT_MAX, INT_MAX);
    }
}

void Game::OnMouseMove(uint32_t buttonDownFlags, const ivec2& pos)
{
    /*
    float yaw = ((float)pos.x / g_Renderer->GetFinalResolutionF().x - 0.5f) * glm::two_pi<float>();
    float pitch = ((float)pos.y / g_Renderer->GetFinalResolutionF().y - 0.5f) * glm::pi<float>();
    OrbitingCamera* cam = g_Renderer->GetCamera();
    cam->SetYaw(yaw);
    cam->SetPitch(pitch);
    */
    
    if(m_MouseDragEnabled)
    {
        const ivec2 deltaPos = pos - m_MouseDragPrevPos;
        if(glm::any(glm::notEqual(deltaPos, ivec2(0))))
        {
            FlyingCamera* cam = g_Renderer->GetCamera();
            cam->SetYaw(cam->GetYaw() + (float)deltaPos.x * g_FlyingCameraRotationSpeed.GetValue().x);
            cam->SetPitch(cam->GetPitch() + (float)deltaPos.y * g_FlyingCameraRotationSpeed.GetValue().y);
            m_MouseDragPrevPos = pos;
        }
    }
}

void Game::OnMouseWheel(int16_t distance, uint32_t buttonDownFlags, const ivec2& pos)
{
}

void Game::ImGui()
{
    ImGui::Begin("RegEngine", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar);
    ImGui::BeginMenuBar();
    if(ImGui::BeginMenu("File"))
    {
        if(ImGui::MenuItem("Exit", "ESC"))
            g_App->Exit(false);
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Edit"))
    {
        ImGui::MenuItem("Settings", nullptr, &g_SettingsImGuiWindowVisible);
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Statistics", nullptr, &m_StatisticsWindowVisible);
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Help"))
    {
        ImGui::MenuItem("ImGui demo", nullptr, &m_DemoWindowVisible);
        ImGui::MenuItem("ImGui metrics", nullptr, &m_MetricsWindowVisible);
        ImGui::MenuItem("ImGui stack tool", nullptr, &m_StackToolWindowVisible);
        ImGui::Separator();
        ImGui::MenuItem("About", nullptr, &m_AboutWindowVisible);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    ImGui::End();

    SettingsImGui();
    if(m_StatisticsWindowVisible)
        ShowStatisticsWindow();
    if(m_DemoWindowVisible)
        ImGui::ShowDemoWindow(&m_DemoWindowVisible);
    if(m_MetricsWindowVisible)
        ImGui::ShowMetricsWindow(&m_MetricsWindowVisible);
    if(m_StackToolWindowVisible)
        ImGui::ShowStackToolWindow(&m_StackToolWindowVisible);
    if(m_AboutWindowVisible)
        ShowAboutWindow();
}

void Game::ShowAboutWindow()
{
    string t = "RegEngine\nConfiguration: ";
#ifdef _DEBUG
    t += "Debug";
#else
    t += "Release";
#endif

    ImGui::Begin("About", &m_AboutWindowVisible);
    ImGui::Text(t.c_str());
    ImGui::End();
}

static vec4 DeltaTimeToColor(float dt)
{
    constexpr vec3 colors[] = {
        {0.f, 0.f, 1.f}, // blue
        {0.f, 1.f, 0.f}, // green
        {1.f, 1.f, 0.f}, // yellow
        {1.f, 0.f, 0.f}, // red
    };
    constexpr float dts[] = {
        1.f / 120.f,
        1.f / 60.f,
        1.f / 30.f,
        1.f / 15.f,
    };
    if(dt < dts[0])
        return vec4(colors[0], 1.f);
    for(size_t i = 1; i < _countof(dts); ++i)
    {
        if(dt < dts[i])
        {
            const float t = (dt - dts[i - 1]) / (dts[i] - dts[i - 1]);
            return vec4(glm::mix(colors[i - 1], colors[i], t), 1.f);
        }
    }
    return vec4(colors[_countof(dts) - 1], 1.f);
}

static string DurationToStr(Time duration)
{
    uint32_t msec = TimeToMilliseconds<uint32_t>(duration);
    uint32_t sec = msec / 1000;
    msec %= 1000;
    uint32_t min = sec / 60;
    sec %= 60;
    uint32_t hr = min / 60;
    min %= 60;
    return std::format("{}:{:02}:{:02}:{:03}", hr, min, sec, msec);
}

void Game::ShowStatisticsWindow()
{
    const TimeData& appTime = g_App->GetTime();
    
    ImGui::Begin("Statistics", &m_StatisticsWindowVisible);
    
    ImGui::Text("FPS: %.1f", g_App->GetFPS());
    ImGui::Text("Application time: %s, frame: %u", DurationToStr(appTime.m_Time).c_str(), appTime.m_FrameIndex);
    ImGui::Text("Scene time: %s, frame: %u", DurationToStr(m_SceneTime.m_Time).c_str(), m_SceneTime.m_FrameIndex);

    if(ImGui::CollapsingHeader("Frame time graph"))
        ShowFrameTimeGraph();

    if(ImGui::CollapsingHeader("D3D12 Memory Allocator"))
        g_Renderer->ImGui_D3D12MAStatistics();

    ImGui::End();
}

void Game::ShowFrameTimeGraph()
{
    const float width = ImGui::GetWindowWidth();
    const size_t frameCount = m_FrameTimeHistory.GetCount();
    if(width > 0.f && frameCount > 0)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 basePos = ImGui::GetCursorScreenPos();
        constexpr float minHeight = 2.f;
        constexpr float maxHeight = 64.f;
        float endX = width;
        constexpr float dtMin = 1.f / 120.f;
        constexpr float dtMax = 1.f / 15.f;
        const float dtMin_Log2 = log2(dtMin);
        const float dtMax_Log2 = log2(dtMax);
        drawList->AddRectFilled(basePos, ImVec2(basePos.x + width, basePos.y + maxHeight), 0xFF404040);
        for(size_t frameIndex = 0; frameIndex < frameCount && endX > 0.f; ++frameIndex)
        {
            const FrameTimeHistory::Entry dt = m_FrameTimeHistory.Get(frameIndex);
            const float frameWidth = dt.m_DT / dtMin;
            const float frameHeightFactor = (dt.m_DT_Log2 - dtMin_Log2) / (dtMax_Log2 - dtMin_Log2);
            const float frameHeightFactor_Nrm = std::min(std::max(0.f, frameHeightFactor), 1.f);
            const float frameHeight = glm::mix(minHeight, maxHeight, frameHeightFactor_Nrm);
            const float begX = endX - frameWidth;
            const uint32_t color = glm::packUnorm4x8(DeltaTimeToColor(dt.m_DT));
            drawList->AddRectFilled(
                ImVec2(basePos.x + std::max(0.f, floor(begX)), basePos.y + maxHeight - frameHeight),
                ImVec2(basePos.x + ceil(endX), basePos.y + maxHeight),
                color);
            endX = begX;
        }
        ImGui::Dummy(ImVec2(width, maxHeight));
    }
}
