#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>
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
    void MeshInspector(Scene& scene);
    void PointLightInspector(Scene& scene);
    void DirLightInspector(Scene& scene);
    void AmbientLightInspector(Scene& scene);
    void Viewport(uint64_t imagePtr, uint32_t width, uint32_t height);

    int m_selectedObject = -1;
    SceneObjectTypes m_type = MESH;
};
}
