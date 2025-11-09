#include "Editor.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx12.h>
#include <imgui/imgui_impl_glfw.h>

#include <device/Device.hpp>

#pragma warning(push, 0)
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>  // <-- This one is key
#include <glm/gtx/quaternion.hpp>
#include <scene/Scene.hpp>
#pragma warning(pop)


KS::Editor::Editor(Device& device) { device.InitializeImGUI(); }

KS::Editor::~Editor() {}

void KS::Editor::RenderWindows(Device& device, std::unique_ptr<Scene>* scenes, uint32_t sceneCount, float deltaTime,
                               bool& recompileShaders, bool& raytraced, int& sceneIndex)
{
    ChooseScene(scenes, sceneCount, sceneIndex);
    SceneHierarchy(*scenes[sceneIndex].get());
    TransformWindow(*scenes[sceneIndex].get());
    FogWindow(device, *scenes[sceneIndex].get());
    InfoWindow(device, deltaTime, recompileShaders, raytraced);
}

void KS::Editor::ChooseScene(std::unique_ptr<Scene>* scenes, uint32_t sceneCount, int& index)
{
    bool open = true;
    ImGui::Begin("Choose scene", &open);

    for (uint32_t i = 0; i < sceneCount; i++)
    {
        if (ImGui::RadioButton(scenes[i]->GetName().c_str(), &index, scenes[i]->GetIndex()))
        { /* changed to Path */
        }
    }

    ImGui::End();
}

void KS::Editor::SceneHierarchy(Scene& scene)
{
    const auto& drawQueue = scene.GetQueue();
    bool open = true;
    int i = 0;
    ImGui::Begin("Scene hierarchy", &open);
    for (const auto& drawObject : drawQueue)
    {
        const auto& objectName = drawObject.second.mesh->GetName() + "##" + drawObject.first;

        const bool is_selected = (m_selectedObject == i);
        if (ImGui::Selectable(objectName.c_str(), is_selected)) m_selectedObject = i;

        // Optionally focus selected item
        if (is_selected) ImGui::SetItemDefaultFocus();
        i++;
    }
    ImGui::End();
}

// Helper to decompose mat4 into T/R/S
bool DecomposeTransform(const glm::mat4& mat, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
{
    using namespace glm;
    vec3 skew;
    vec4 perspective;
    quat orientation;

    if (!decompose(mat, scale, orientation, translation, skew, perspective)) return false;

    rotation = degrees(eulerAngles(orientation));  // Convert to degrees for UI
    return true;
}

// Helper to recompose mat4 from T/R/S
glm::mat4 RecomposeTransform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
{
    glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 r = glm::toMat4(glm::quat(glm::radians(rotation)));
    glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
    return t * r * s;
}

void KS::Editor::TransformWindow(Scene& scene)
{
    auto& drawQueue = scene.GetQueue();
    bool open = true;

    ImGui::Begin("Transform", &open);
    if (m_selectedObject >= drawQueue.size())
    {
        m_selectedObject = -1;
    }

    if (m_selectedObject == -1)
        ImGui::Text("No object was selected");
    else
    {
        auto& object = *std::next(drawQueue.begin(), m_selectedObject);

        glm::vec3 translation, rotation, scale;
        glm::mat4 oldTransform = object.second.modelMat;
        DecomposeTransform(oldTransform, translation, rotation, scale);
        bool transfromChanged = false;
        
        if (ImGui::DragFloat3("Translation", &translation.x, 0.1f)) transfromChanged = true;
        if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) transfromChanged = true;
        if (ImGui::DragFloat3("Scale", &scale.x, 0.1f)) transfromChanged = true;

        if (transfromChanged)
        {
            glm::mat4 newTransform = RecomposeTransform(translation, rotation, scale);
            glm::mat4 delta = glm::inverse(newTransform) * oldTransform;
            scene.ApplyModelTransform(object.first, delta);
        }

    }
    ImGui::End();
}

void KS::Editor::FogWindow(Device& device, Scene& scene)
{
    bool open = true;
    FogInfo fogInfo = scene.GetFogValues();
    bool fogChanged = false;

    ImGui::Begin("Fog window", &open);

    if (ImGui::DragFloat("Fog density", &fogInfo.fogDensity, 0.01f)) fogChanged = true;
    if (ImGui::DragInt("Sample numbers", &fogInfo.lightShaftNumberSamples, 1)) fogChanged = true;
    if (ImGui::DragFloat("Sample weight", &fogInfo.weight, 0.001f)) fogChanged = true;
    if (ImGui::DragFloat("Sample decay", &fogInfo.decay, 0.001f)) fogChanged = true;
    if (ImGui::DragFloat("Exposure", &fogInfo.exposure, 0.01f)) fogChanged = true;

    if (fogChanged) 
        scene.SetFogValues(device, fogInfo);

    ImGui::End();
}

void KS::Editor::InfoWindow(Device&, float, bool& recompileShaders, bool& raytraced)
{ 
    float FPS = 1.f / ImGui::GetIO().DeltaTime;
    bool open = true;
    ImGui::Begin("DT window", &open); 
    ImGui::Text(("FPS: " + std::to_string(FPS)).c_str());
    ImGui::Text(("Ms: " + std::to_string(ImGui::GetIO().DeltaTime * 1000.f)).c_str());

    if (ImGui::Button("Recompile shaders")) { recompileShaders = true; }

    bool currentValue = raytraced;
    if (ImGui::Checkbox("Raytraced", &currentValue))
    {
        raytraced = !raytraced;
    }

    ImGui::End();
}