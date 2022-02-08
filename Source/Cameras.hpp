#pragma once

class Camera
{
public:
    virtual ~Camera() = default;

    float GetFovY() const { return m_FovY; }
    float GetAspectRatio() const { return m_AspectRatio; }
    float GetZNear() const { return m_ZNear; }

    // Angle in radians
    void SetFovY(float fovY)
    {
        if(fovY != m_FovY) { m_FovY = fovY; InvalidateProjection(); }
    }
    // Viewport Width / Height
    void SetAspectRatio(float aspectRatio)
    {
        if(aspectRatio != m_AspectRatio) { m_AspectRatio = aspectRatio; InvalidateProjection(); }
    }
    void SetZNear(float ZNear)
    {
        if(ZNear != m_ZNear) { m_ZNear = ZNear; InvalidateProjection(); }
    }

    const mat4& GetView();
    const mat4& GetProjection();
    const mat4& GetProjectionInverse();
    const mat4& GetViewProjection();

protected:
    void InvalidateView() { m_ViewValid = false; m_ViewProjectionValid = false; }
    void InvalidateProjection() { m_ProjectionValid = m_ProjectionInverseValid = m_ViewProjectionValid = false; }
    virtual mat4 CalculateView() = 0;

private:
    float m_FovY = glm::radians(80.f);
    float m_AspectRatio= 1.f;
    float m_ZNear = 0.5f;
    mat4 m_Projection;
    mat4 m_ProjectionInverse;
    mat4 m_View;
    mat4 m_ViewProjection;
    bool m_ProjectionValid = false;
    bool m_ProjectionInverseValid = false;
    bool m_ViewValid = false;
    bool m_ViewProjectionValid = false;
};

/*
Looking at Target from some Distance and angle denoted by 2 Euler angles.
With Yaw = Pitch = 0, the camera is looking in the forward direction (negative Y).
*/
class OrbitingCamera : public Camera
{
public:
    const vec3& GetTarget() const { return m_Target; }
    float GetDistance() const { return m_Distance; }
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }

    void SetTarget(const vec3& target)
    {
        if(target != m_Target) { m_Target = target; InvalidateView(); }
    }
    void SetDistance(float distance)
    {
        if(distance != m_Distance) { m_Distance = distance; InvalidateView(); }
    }
    void SetYaw(float yaw)
    {
        if(yaw != m_Yaw) { m_Yaw = yaw; InvalidateView(); }
    }
    void SetPitch(float pitch)
    {
        pitch = std::clamp(pitch, -glm::half_pi<float>() + 1e-6f, glm::half_pi<float>() - 1e-6f);
        if(pitch != m_Pitch) { m_Pitch = pitch; InvalidateView(); }
    }

protected:
    mat4 CalculateView() override;

private:
    vec3 m_Target = vec3(0.f);
    float m_Distance = 10.f;
    // Rotation around up (Z) axis.
    float m_Yaw = 0.f;
    // Rotation around right (X) axis.
    float m_Pitch = 0.f;
};
