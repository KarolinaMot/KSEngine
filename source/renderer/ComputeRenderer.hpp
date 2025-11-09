#pragma once
#include "SubRenderer.hpp"

namespace KS
{
class Scene;
class ComputeRenderer : public SubRenderer
{
public:
    ComputeRenderer(const Device& device, std::shared_ptr<Shader>& shader);
    ~ComputeRenderer();

    virtual void Render(Device& device, DXCommandContext* commandContext, RenderParameters& par);
    void SetDispatchSize(uint32_t width, uint32_t height, uint32_t depth = 1)
    {
        m_dispatchWidth = width;
        m_dispatchHeight = height;
        m_dispatchDepth = depth;
    }

private:
    uint32_t m_dispatchWidth = 0;
    uint32_t m_dispatchHeight = 0;
    uint32_t m_dispatchDepth = 1;
};
}  // namespace KS
