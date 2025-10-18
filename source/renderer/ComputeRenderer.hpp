#pragma once
#include "SubRenderer.hpp"

namespace KS
{
class Scene;
class ComputeRenderer : public SubRenderer
{
public:
    ComputeRenderer(const Device& device, SubRendererDesc& desc);
    ~ComputeRenderer();

    virtual void Render(Device& device, DXCommandContext* commandContext, Scene& scene,
                                 std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>& inputs, bool clearRT);
    void SetDispatchSize(uint32_t width, uint32_t height)
    {
        m_dispatchWidth = width;
        m_dispatchHeight = height;
    }

private:
    uint32_t m_dispatchWidth = 0;
    uint32_t m_dispatchHeight = 0;
};
}  // namespace KS
