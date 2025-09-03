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
        virtual void Bind(const Device& device, DXCommandList& commandList, const ShaderInputDesc& desc,
                          uint32_t offsetIndex = 0) = 0;
        virtual size_t GetGPUAddress(int elementIndex, int frameIndex) const = 0;

    };
}  // namespace KS
