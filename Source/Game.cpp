#include "BaseUtils.hpp"
#include "Game.hpp"
#include "Renderer.hpp"
#include "Cameras.hpp"
#include "Settings.hpp"

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

}

void Game::OnMouseMove(uint32_t buttonDownFlags, const ivec2& pos)
{
    float yaw = ((float)pos.x / g_Renderer->GetFinalResolutionF().x - 0.5f) * glm::two_pi<float>();
    float pitch = ((float)pos.y / g_Renderer->GetFinalResolutionF().y - 0.5f) * glm::pi<float>();
    OrbitingCamera* cam = g_Renderer->GetCamera();
    cam->SetYaw(yaw);
    cam->SetPitch(pitch);
}