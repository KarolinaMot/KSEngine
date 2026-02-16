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
#pragma warning(pop)

#include <components/ComponentCamera.hpp>
#include <components/ComponentTransform.hpp>
#include <scene/Scene.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>

KS::Editor::Editor(Device& device)
{
    device.InitializeImGUI();
    auto& style{ImGui::GetStyle()};
    // Borders
    style.WindowBorderSize = 3.0f;

    // Rounding
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;

    // Docking
    style.DockingSeparatorSize = 3.0f;

    {
        constexpr auto ToRGBA = [](uint32_t argb) constexpr
        {
            ImVec4 color{};
            color.x = ((argb >> 16) & 0xFF) / 255.0f;
            color.y = ((argb >> 8) & 0xFF) / 255.0f;
            color.z = (argb & 0xFF) / 255.0f;
            color.w = ((argb >> 24) & 0xFF) / 255.0f;
            return color;
        };

        constexpr auto Lerp = [](const ImVec4& a, const ImVec4& b, float t) constexpr {
            return ImVec4{std::lerp(a.x, b.y, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t), std::lerp(a.w, b.w, t)};
        };

        auto colors{style.Colors};
        colors[ImGuiCol_Text] = ToRGBA(0xFFABB2BF);
        colors[ImGuiCol_TextDisabled] = ToRGBA(0xFF565656);
        colors[ImGuiCol_WindowBg] = ToRGBA(0xFF282C34);
        colors[ImGuiCol_ChildBg] = ToRGBA(0xFF21252B);
        colors[ImGuiCol_PopupBg] = ToRGBA(0xFF2E323A);
        colors[ImGuiCol_Border] = ToRGBA(0xFF2E323A);
        colors[ImGuiCol_BorderShadow] = ToRGBA(0x00000000);
        colors[ImGuiCol_FrameBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_FrameBgHovered] = ToRGBA(0xFF484C52);
        colors[ImGuiCol_FrameBgActive] = ToRGBA(0xFF54575D);
        colors[ImGuiCol_TitleBg] = colors[ImGuiCol_WindowBg];
        colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_TitleBgCollapsed] = ToRGBA(0x8221252B);
        colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_PopupBg];
        colors[ImGuiCol_ScrollbarGrab] = ToRGBA(0xFF3E4249);
        colors[ImGuiCol_ScrollbarGrabHovered] = ToRGBA(0xFF484C52);
        colors[ImGuiCol_ScrollbarGrabActive] = ToRGBA(0xFF54575D);
        colors[ImGuiCol_CheckMark] = colors[ImGuiCol_Text];
        colors[ImGuiCol_SliderGrab] = ToRGBA(0xFF353941);
        colors[ImGuiCol_SliderGrabActive] = ToRGBA(0xFF7A7A7A);
        colors[ImGuiCol_Button] = colors[ImGuiCol_SliderGrab];
        colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_ScrollbarGrabActive];
        colors[ImGuiCol_Header] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_HeaderHovered] = ToRGBA(0xFF353941);
        colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_Separator] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_SeparatorHovered] = ToRGBA(0xFF3E4452);
        colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_SeparatorHovered];
        colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_Separator];
        colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_SeparatorHovered];
        colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_SeparatorActive];
        colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
        colors[ImGuiCol_Tab] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_WindowBg];
        colors[ImGuiCol_PlotLines] = ImVec4{0.61f, 0.61f, 0.61f, 1.00f};
        colors[ImGuiCol_PlotLinesHovered] = ImVec4{1.00f, 0.43f, 0.35f, 1.00f};
        colors[ImGuiCol_PlotHistogram] = ImVec4{0.90f, 0.70f, 0.00f, 1.00f};
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4{1.00f, 0.60f, 0.00f, 1.00f};
        colors[ImGuiCol_TableHeaderBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_SliderGrab];
        colors[ImGuiCol_TableBorderLight] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_TableRowBg] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
        colors[ImGuiCol_TableRowBgAlt] = ImVec4{1.00f, 1.00f, 1.00f, 0.06f};
        colors[ImGuiCol_TextSelectedBg] = ToRGBA(0xFF243140);
        colors[ImGuiCol_DragDropTarget] = colors[ImGuiCol_Text];
        colors[ImGuiCol_NavWindowingHighlight] = colors[ImGuiCol_Text];
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4{0.80f, 0.80f, 0.80f, 0.20f};
        colors[ImGuiCol_ModalWindowDimBg] = ToRGBA(0xC821252B);
    }
}

KS::Editor::~Editor() {}

void KS::Editor::RenderWindows(Device& device, std::unique_ptr<Scene>* scenes, uint32_t, float fps, float ms,
                               bool& recompileShaders, uint32_t numLights, uint32_t numMeshes, bool& raytraced, int& sceneIndex,
                               ComponentFirstPersonCamera& info, ComponentTransform& camTransform, float& exposure)
{
    ImGui::DockSpaceOverViewport();

    auto frameIndex = device.GetFrameIndex();
    uint64_t gpuPtr;
    uint32_t width, height;
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetImguiHeap());
    scenes[sceneIndex]->GetFinalRTInfo(device, heap, frameIndex, gpuPtr, width, height);
    Viewport(gpuPtr, width, height);

    int shadowSample = scenes[sceneIndex]->GetShadowSample();
    int giSamples = scenes[sceneIndex]->GetGISample();
    bool m_vSyncOn = device.GetVSync();
    InfoWindow(numLights, numMeshes, fps, ms, shadowSample, giSamples, recompileShaders, raytraced, m_vSyncOn, exposure);
    device.SetVSync(m_vSyncOn);

    if (m_showEditor)
    {
        SceneHierarchy(*scenes[sceneIndex].get());
        TransformWindow(*scenes[sceneIndex].get());

        CameraWindow(info, camTransform);
    }

    scenes[sceneIndex]->SetGISample(giSamples);
    scenes[sceneIndex]->SetShadowSample(shadowSample);

}

void KS::Editor::SceneHierarchy(Scene& scene)
{
    bool open = true;
    auto lightInfo = scene.GetLightInfo();

    ImGui::Begin("Scene hierarchy", &open);
    static const char* current_item = NULL;

    {
        const bool is_selected = 0;
        if (ImGui::Selectable("Ambient light", is_selected))
        {
            m_selectedObject = 0;
            m_type = AMBIENT_LIGHT;
        }
        // Optionally focus selected item
        if (is_selected) ImGui::SetItemDefaultFocus();
    }

    if (ImGui::CollapsingHeader("Point lights"))
    {
        auto numPointLights = static_cast<int>(lightInfo.numPointLights);
        for (int i = 0; i < numPointLights; i++)
        {
            const auto& objectName = "PointLight" + std::to_string(i);

            const bool is_selected = (m_selectedObject == i);
            if (ImGui::Selectable(objectName.c_str(), is_selected))
            {
                m_selectedObject = i;
                m_type = POINT_LIGHT;
            }
            // Optionally focus selected item
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
    }

    if (ImGui::CollapsingHeader("Directional lights"))
    {
        auto numDirLights = static_cast<int>(lightInfo.numDirLights);

        for (int i = 0; i < numDirLights; i++)
        {
            const auto& objectName = "DirLight" + std::to_string(i);

            const bool is_selected = (m_selectedObject == i);
            if (ImGui::Selectable(objectName.c_str(), is_selected))
            {
                m_selectedObject = i;
                m_type = DIR_LIGHT;
            }
            // Optionally focus selected item
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
    }

    if (ImGui::CollapsingHeader("Scene meshes"))
    {
        auto queueSize = scene.GetDrawQueueSize();

        for (uint32_t i = 0; i < queueSize; i++)
        {
            auto drawObject = scene.GetDrawEntry(i);
            auto mesh = scene.GetMesh(drawObject.meshHandle);
            const auto& objectName = mesh->GetName() + "##" + std::to_string(i);

            const bool is_selected = (m_selectedObject == static_cast<int>(i));
            if (ImGui::Selectable(objectName.c_str(), is_selected))
            {
                m_selectedObject = i;
                m_type = MESH;
            }
            // Optionally focus selected item
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
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
    bool open = true;

    ImGui::Begin("Inspector", &open);

    switch (m_type)

    {
        case MESH:
            MeshInspector(scene);
            break;
        case DIR_LIGHT:
            DirLightInspector(scene);
            break;
        case POINT_LIGHT:
            PointLightInspector(scene);
            break;
        case AMBIENT_LIGHT:
            AmbientLightInspector(scene);
            break;
        default:
            MeshInspector(scene);
            break;
    }
    ImGui::End();
}

void KS::Editor::InfoWindow(uint32_t numLights, uint32_t numMeshes, float fps, float ms, int& shadowSample, int& giSample, bool& recompileShaders, bool& raytraced,
                            bool& vSync, float& exposure)
{
    bool open = true;

    ImGui::Begin("Show Editor", &open);
    ImGui::Checkbox("Show editor windows", &m_showEditor);
    ImGui::End();

    if (!m_showEditor)
    {
        return;
    }

    ImGui::Begin("Scene info", &open);

    ImGui::Text(("FPS: " + std::format("{:.2f}", fps)).c_str());
    ImGui::Text(("Ms: " + std::format("{:.2f}", ms)).c_str());
    ImGui::Text(("Number of lights: " + std::to_string(numLights)).c_str());
    ImGui::Text(("Number of meshes: " + std::to_string(numMeshes)).c_str());

    ImGui::Spacing();

    if (ImGui::Button("Recompile shaders"))
    {
        recompileShaders = true;
    }

    ImGui::Spacing();


    bool currentVSync = vSync;
    if (ImGui::Checkbox("VSync", &currentVSync))
    {
        vSync = !vSync;
    }

    bool currentValue = raytraced;
    if (ImGui::Checkbox("Raytraced", &currentValue))
    {
        raytraced = !raytraced;
    }

    ImGui::Spacing();

    if (raytraced)
    {
        ImGui::DragInt("Number of GI samples", &giSample, 4, 1);   
        ImGui::DragInt("Number of shadow samples", &shadowSample, 4, 1);

        glm::clamp(giSample, 0, 64);
        glm::clamp(shadowSample, 0, 16);
    }

    ImGui::DragFloat("Exposure", &exposure, 0.5f, 0.f);
    exposure = glm::max(0.f, exposure);


    ImGui::End();
}

void KS::Editor::CameraWindow(ComponentFirstPersonCamera& info, ComponentTransform& camTransform)
{
    bool open = true;
    ImGui::Begin("Camera window", &open);

    glm::vec3 camPosition = camTransform.GetLocalTranslation();

    ImGui::DragFloat3("Camera position", &camPosition[0], 0.1f);
    float degfieldOfView = glm::degrees(info.fieldOfView);

    ImGui::DragFloat("Camera FoV (degrees)", &degfieldOfView, 1.f);
    info.fieldOfView = glm::radians(degfieldOfView);
    ImGui::DragFloat("Camera far plane", &info.farPlane, 0.01f);
    ImGui::DragFloat("Camera near plane", &info.nearPlane, 0.01f);
    camTransform.SetLocalTranslation(camPosition);

    ImGui::End();
}

void KS::Editor::MeshInspector(Scene& scene)
{
    if (m_selectedObject >= static_cast<int>(scene.GetDrawQueueSize()))
    {
        m_selectedObject = -1;
    }

    if (m_selectedObject == -1)
    {
        ImGui::Text("No object was selected");
        return;
    }

    auto& object = scene.GetDrawEntry(m_selectedObject);

    glm::vec3 translation, rotation, scale;
    glm::mat4 oldTransform = object.modelMat;
    DecomposeTransform(oldTransform, translation, rotation, scale);
    bool transfromChanged = false;

    if (ImGui::DragFloat3("Translation", &translation.x, 0.1f)) transfromChanged = true;
    if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) transfromChanged = true;
    if (ImGui::DragFloat3("Scale", &scale.x, 0.1f)) transfromChanged = true;

    if (transfromChanged)
    {
        glm::mat4 newTransform = RecomposeTransform(translation, rotation, scale);
        glm::mat4 delta = glm::inverse(newTransform) * oldTransform;
        scene.ApplyModelTransform(m_selectedObject, delta);
    }
}

void KS::Editor::PointLightInspector(Scene& scene)
{

    if (m_selectedObject >= static_cast<int>(scene.GetLightInfo().numPointLights))
    {
        m_selectedObject = -1;
    }

    if (m_selectedObject == -1)
    {
        ImGui::Text("No object was selected");
        return;
    }

    bool lightChanged = false;

    auto light = scene.GetPointLight(m_selectedObject);

    if (ImGui::DragFloat3("Position", &light.mPosition.x, 0.1f)) lightChanged = true;
    if (ImGui::DragFloat4("Color and intensity", &light.mColorAndIntensity.x, 0.1f)) lightChanged = true;
    if (ImGui::DragFloat("Radius", &light.mRadius, 0.1f)) lightChanged = true;

     if (lightChanged)
     scene.UpdatePointLights(m_selectedObject, light);
}

void KS::Editor::DirLightInspector(Scene& scene)
{
    if (m_selectedObject >= static_cast<int>(scene.GetLightInfo().numDirLights))
    {
        m_selectedObject = -1;
    }

    if (m_selectedObject == -1)
    {
        ImGui::Text("No object was selected");
        return;
    }

    auto light = scene.GetDirLight(m_selectedObject);

    bool lightChanged = false;
    float degAngularRadius = glm::degrees(light.mAngularRadius);
    if (ImGui::DragFloat3("Direction", &light.mDir.x, 0.1f)) lightChanged = true;
    if (ImGui::DragFloat4("Color and intensity", &light.mColorAndIntensity.x, 0.1f)) lightChanged = true;
    if (ImGui::DragFloat("Angular radius (degrees)", &degAngularRadius, 0.25f)) lightChanged = true;
    light.mAngularRadius = glm::radians(degAngularRadius);
    
    if (lightChanged)
     scene.UpdateDirLights(m_selectedObject, light);
}

void KS::Editor::AmbientLightInspector(Scene& scene)
{
    auto lightInfo = scene.GetLightInfo();
    ImGui::DragFloat4("Color and intensity", &lightInfo.mAmbientAndIntensity.x, 0.1f);
    scene.SetAmbientLight(glm::vec3(lightInfo.mAmbientAndIntensity), lightInfo.mAmbientAndIntensity.a);
}

void KS::Editor::Viewport(uint64_t imagePtr, uint32_t, uint32_t)
{
    bool open = true;
    int FULL_SCREEN_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Viewport", &open, m_showEditor ? 0 : FULL_SCREEN_FLAGS);
    ImVec2 viewportSize = ImGui::GetWindowSize();
    m_viewportSize.x = static_cast<int>(viewportSize.x);
    m_viewportSize.y = static_cast<int>(viewportSize.y);
    ImGui::Image((ImTextureID)imagePtr, viewportSize);
    ImGui::End();
}
