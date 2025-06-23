#pragma once

namespace KS
{

class Device;
class Scene;
class Editor
{
public:
    Editor(Device& device);
    ~Editor();

    void RenderWindows(Device& device, Scene& scene, float deltaTime);
    void SceneHierarchy(Scene& scene);
    void TransformWindow(Device& device, Scene& scene);
    void FogWindow(Device& device, Scene& scene);
    void FPSWindow(Device& device, float deltaTime);

private:
    int m_selectedObject = -1;
};
}
