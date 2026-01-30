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

    void RenderWindows(Device& device, std::unique_ptr<Scene>* scenes, uint32_t sceneCount, float fps, float ms,
                       bool& recompileShaders, bool& raytraced, int& sceneIndex, ComponentFirstPersonCamera& info,
                       ComponentTransform& camTransform, float& exposure);
    glm::ivec2 GetViewportSize() const { return m_viewportSize; }

private:

    void ChooseScene(std::unique_ptr<Scene>* scenes, uint32_t sceneCount, int& index);
    void SceneHierarchy(Scene& scene);
    void TransformWindow(Scene& scene);
    void InfoWindow(float fps, float ms, int& shadowSample, int& giSample, bool& recompileShaders, bool& raytraced,
                    bool& vSync, float& exposure);
    void CameraWindow(ComponentFirstPersonCamera& info, ComponentTransform& camTransform);
    void MeshInspector(Scene& scene);
    void PointLightInspector(Scene& scene);
    void DirLightInspector(Scene& scene);
    void AmbientLightInspector(Scene& scene);
    void Viewport(uint64_t imagePtr, uint32_t width, uint32_t height);

    int m_selectedObject = -1;
    SceneObjectTypes m_type = MESH;
    glm::ivec2 m_viewportSize = {1, 1};
};
}
