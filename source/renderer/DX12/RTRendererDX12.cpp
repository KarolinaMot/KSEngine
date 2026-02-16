#include "../RTRenderer.hpp"
#include <device/Device.hpp>
#include <fileio\ResourceHandle.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Texture.hpp>
#include <resources/Skydome.hpp>
#pragma warning(push, 0)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DXRTPipeline.hpp>
#include <renderer/DX12/Helpers/DXShaderTable.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/ShaderInputBlueprintBuilder.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <scene/Scene.hpp>
#include <renderer/TLAS.hpp>

KS::RTRenderer::RTRenderer(const Device& device, std::shared_ptr<Shader>& shader) : SubRenderer(device, shader) {}

KS::RTRenderer::~RTRenderer() {}

void KS::RTRenderer::Render(Device& device, DXCommandContext* commandContext, RenderParameters& par)
{
    auto commandList = commandContext->m_commandList.get();
    auto frameIndex = device.GetFrameIndex();
    auto prevFrameIndex = device.GetPrevFrameIndex();
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(par.scene->GetResourceHeap());

    if (par.rt) par.rt->GetTexture(frameIndex, 0)->TransitionToRW(resourceHeap, *commandList);

    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    auto shaderInputs = m_shader->GetShaderInput();

    if (shaderInputs)
    {
        commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(shaderInputs->GetSignature()), true);

        for (const auto& input : *par.inputs)
        {
            if (input.first)
            input.first->Bind(input.second.prev ? prevFrameIndex : frameIndex, resourceHeap, *commandList, input.second.desc,
                              input.second.bindOffset);
        }

        commandList->BindHeapSlot(*resourceHeap, NORMALS_SLOT, m_shader->GetShaderInput()->GetInput("normals").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, INDICES_SLOT, m_shader->GetShaderInput()->GetInput("indices").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, VPOS_SLOT, m_shader->GetShaderInput()->GetInput("vertexPos").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, UVS_SLOT, m_shader->GetShaderInput()->GetInput("uvs").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, TAN_SLOT, m_shader->GetShaderInput()->GetInput("tangents").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, 0, m_shader->GetShaderInput()->GetInput("textures").rootIndex);
    }

    auto pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline());
    auto shaderTable = pipeline->m_shaderTable->get();

    D3D12_DISPATCH_RAYS_DESC desc = shaderTable->FillDispatchDesc(par.rt->GetWidth(), par.rt->GetHeight(), 1);

    // Bind the raytracing pipeline
    commandList->GetCommandList()->SetPipelineState1(pipeline->m_pipeline.Get());
    // Dispatch the rays and write to the raytracing output
    commandList->GetCommandList()->DispatchRays(&desc);

    m_frameCount++;
}