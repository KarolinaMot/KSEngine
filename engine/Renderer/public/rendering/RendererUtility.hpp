
#include <gpu_resources/DXResource.hpp>

namespace RendererUtility
{

// Resource must be a CPU
template <typename T>
void WriteResource(DXResource& resource, const T& source)
{
    auto mapped_ptr = resource.Map(0);
    std::memcpy(mapped_ptr.Get(), &source, sizeof(T));
    resource.Unmap(std::move(mapped_ptr), D3D12_RANGE { 0, sizeof(T) });
}

template <size_t S>
std::array<CD3DX12_RESOURCE_BARRIER, S> MakeTransitionBarriers(const std::array<ID3D12Resource*, S>& resources, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    std::array<CD3DX12_RESOURCE_BARRIER, S> out {};
    for (size_t i = 0; i < S; i++)
    {
        out.at(i) = CD3DX12_RESOURCE_BARRIER::Transition(resources.at(i), before, after);
    }
    return out;
}

}