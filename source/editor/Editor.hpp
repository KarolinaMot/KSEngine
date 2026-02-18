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

    void RenderWindows(Device& device, std::unique_ptr<Scene>* scenes, float fps, float ms,
                               bool& recompileShaders,  bool& raytraced, int& sceneIndex,
                               ComponentFirstPersonCamera& info, ComponentTransform& camTransform);
    glm::ivec2 GetViewportSize() const { return m_viewportSize; }

private:

    void SceneHierarchy(Scene& scene);
    void TransformWindow(Scene& scene);
    void InfoWindow(uint32_t numLights, uint32_t numMeshes, float fps, float ms, int& giSample, bool& recompileShaders,
                    bool& raytraced, bool& vSync, float& exposure, float& giBounceStrength);
    void CameraWindow(ComponentFirstPersonCamera& info, ComponentTransform& camTransform);
    void MeshInspector(Scene& scene);
    void PointLightInspector(Scene& scene);
    void DirLightInspector(Scene& scene);
    void AmbientLightInspector(Scene& scene);
    void Viewport(uint64_t imagePtr, uint32_t width, uint32_t height);

    int m_selectedObject = -1;
    SceneObjectTypes m_type = MESH;
    glm::ivec2 m_viewportSize = {1, 1};
    bool m_showEditor = true;
};
}
