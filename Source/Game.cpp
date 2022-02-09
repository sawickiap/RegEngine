#include "BaseUtils.hpp"
#include "Game.hpp"
#include "Renderer.hpp"
#include "Cameras.hpp"
#include "Settings.hpp"
#include "../WorkingDir/Data/Include/ShaderConstants.h"

extern VecSetting<glm::uvec2> g_Size;

Game::Game()
{

}

void Game::Init()
{

}

Game::~Game()
{

}

void Game::Update()
{
    float time = (float)GetTickCount() * 1e-3f;

    for(size_t li = 1; li < g_Renderer->m_Lights.size(); ++li)
    {
        Light& l = g_Renderer->m_Lights[li];
        if(l.m_Type == LIGHT_TYPE_DIRECTIONAL)
        {
            l.m_DirectionToLight_Position = vec3(
                sin(time * 0.3545f * (float)li + 13.1535f + (float)li),
                sin(time * 0.5435f * (float)li + 12.4553f + (float)li),
                sin(time * 0.7453f + 54.3253f + (float)li));
        }
    }
}

void Game::OnKeyDown(WPARAM key)
{
    switch(key)
    {
    case 'R':
    {
        OrbitingCamera* cam = g_Renderer->GetCamera();
        cam->SetYaw(-glm::radians(10.f));
        cam->SetPitch(glm::radians(30.f));
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
    }
}

void Game::OnMouseMove(uint32_t buttonDownFlags, const ivec2& pos)
{
    float yaw = ((float)pos.x / g_Renderer->GetFinalResolutionF().x - 0.5f) * glm::two_pi<float>();
    float pitch = ((float)pos.y / g_Renderer->GetFinalResolutionF().y - 0.5f) * glm::pi<float>();
    OrbitingCamera* cam = g_Renderer->GetCamera();
    cam->SetYaw(yaw);
    cam->SetPitch(pitch);
}
