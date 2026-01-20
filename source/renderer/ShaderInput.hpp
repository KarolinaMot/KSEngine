#pragma once
class DXCommandList;

namespace KS
{
    struct ShaderInputDesc;
    class Device;
    class ShaderInput
    {
    public:
        ShaderInput(){};
        virtual void Bind(uint32_t frameIndex, void* resourceHeap, DXCommandList& commandList, const ShaderInputDesc& desc,
                          uint32_t offset = 0) = 0;
        virtual size_t GetGPUAddress(int elementIndex, int frameIndex) const = 0;

    };
}  // namespace KS
