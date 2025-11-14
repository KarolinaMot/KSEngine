#pragma once
#include <memory>

namespace KS
{

class Device;
class Scene;
class ComponentFirstPersonCamera;
class ComponentTransform;
class Editor
{
public:
    Editor(Device& device);
    ~Editor();

    void RenderWindows(Device& device, std::unique_ptr<Scene>* scenes, uint32_t sceneCount, float deltaTime,
                       bool& recompileShaders, bool& raytraced, int& sceneIndex, ComponentFirstPersonCamera& info,
                       ComponentTransform& camTransform);

private:

    void ChooseScene(std::unique_ptr<Scene>* scenes, uint32_t sceneCount, int& index);
    void SceneHierarchy(Scene& scene);
    void TransformWindow(Scene& scene);
    void FogWindow(Device& device, Scene& scene);
    void InfoWindow(Device&, float deltaTime, bool& recompileShaders, bool& raytraced);
    void CameraWindow(ComponentFirstPersonCamera& info, ComponentTransform& camTransform);

    int m_selectedObject = -1;
};
}
