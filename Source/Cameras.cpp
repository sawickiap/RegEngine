#include "BaseUtils.hpp"
#include "Cameras.hpp"

// Source: "Reversed-Z in OpenGL", https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
static mat4 MakeInfReversedZProjLH(float fovY_radians, float aspectWbyH, float zNear)
{
    float f = 1.0f / tan(fovY_radians / 2.0f);
    return mat4(
        f / aspectWbyH, 0.0f,  0.0f,  0.0f,
        0.0f,    f,  0.0f,  0.0f,
        0.0f, 0.0f,  0.0f,  1.0f,
        0.0f, 0.0f, zNear,  0.0f);
}

const mat4& Camera::GetProjection()
{
    if(!m_ProjectionValid)
    {
        m_Projection = MakeInfReversedZProjLH(m_FovY, m_AspectRatio, m_ZNear);
        m_ProjectionValid = true;
    }
    return m_Projection;
}

const mat4& Camera::GetProjectionInverse()
{
    if(!m_ProjectionInverseValid)
    {
        m_ProjectionInverse = glm::inverse(GetProjection());
        m_ProjectionInverseValid = true;
    }
    return m_ProjectionInverse;
}

const mat4& Camera::GetViewProjection()
{
    if(!m_ViewProjectionValid)
    {
        mat4 proj = GetProjection();
        mat4 view = GetView();
        m_ViewProjection = proj * view;
        m_ViewProjectionValid = true;
    }
    return m_ViewProjection;
}

const mat4& Camera::GetView()
{
    if(!m_ViewValid)
    {
        m_View = CalculateView();
        m_ViewValid = true;
    }
    return m_View;
}

mat4 OrbitingCamera::CalculateView()
{
    vec3 targetToEye = vec3(
        -m_Distance * sin(m_Yaw),
        m_Distance * cos(m_Yaw) * cos(m_Pitch),
        m_Distance * sin(m_Pitch));
    return glm::lookAtLH(
        m_Target + targetToEye, // eye
        m_Target, // center
        vec3(0.f, 0.f, 1.f)); // up
}

vec3 FlyingCamera::CalculateForward() const
{
    // Is this normalization needed?
    return glm::normalize(vec3(
        sin(m_Yaw),
        cos(m_Yaw) * cos(m_Pitch),
        -sin(m_Pitch)));
}

vec3 FlyingCamera::CalculateRight() const
{
    return glm::normalize(glm::cross(
        vec3(0.f, 0.f, 1.f),
        CalculateForward()));
}

mat4 FlyingCamera::CalculateView()
{
    return glm::lookAtLH(
        m_Position, // eye
        m_Position + CalculateForward(), // center
        vec3(0.f, 0.f, 1.f)); // up
}
