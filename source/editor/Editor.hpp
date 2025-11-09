#pragma once
#include <memory>

namespace KS
{

class Device;
class Scene;
class Editor
{
public:
    Editor(Device& device);
    ~Editor();

    void RenderWindows(Device& device, std::unique_ptr<Scene>* scenes, uint32_t sceneCount, float deltaTime,
                       bool& recompileShaders, bool& raytraced, int& sceneIndex);
    void ChooseScene(std::unique_ptr<Scene>* scenes, uint32_t sceneCount, int& index);
    void SceneHierarchy(Scene& scene);
    void TransformWindow(Scene& scene);
    void FogWindow(Device& device, Scene& scene);
    void InfoWindow(Device&, float deltaTime, bool& recompileShaders, bool& raytraced);

private:
    int m_selectedObject = -1;
};
}
