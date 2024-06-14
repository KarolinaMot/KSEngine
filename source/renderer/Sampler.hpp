#pragma once

namespace KS
{
    enum class SamplerFilter
    {
        SF_NEAREST,
        SF_LINEAR,
        SF_ANISOTROPIC
    };

    enum class SamplerAddressMode
    {
        SAM_WRAP,
        SAM_MIRROR,
        SAM_CLAMP,
        SAM_BORDER,
        SAM_MIRROR_ONCE
    };

    enum class SamplerBorderColor
    {
        SBC_TRANSPARENT_BLACK,
        SBC_OPAQUE_BLACK,
        SBC_OPAQUE_WHITE
    };

    struct SamplerDesc
    {
        SamplerFilter filter = SamplerFilter::SF_LINEAR;
        SamplerAddressMode addressMode = SamplerAddressMode::SAM_CLAMP;
        SamplerBorderColor borderColor = SamplerBorderColor::SBC_OPAQUE_BLACK;
    };

} // namespace KS
