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

    void RenderWindows(Device& device, Scene& scene);
    void SceneHierarchy(Scene& scene);
    void TransformWindow(Device& device, Scene& scene);

private:
    int m_selectedObject = -1;
};
}
