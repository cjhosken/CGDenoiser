#include "denoiserData.h"
#include <algorithm>
#include <cstring>

void DenoiserData::allocate(
    int width,
    int height,
    bool needAlbedo,
    bool needNormal,
    bool needMotion,
    bool upscale
)
{
    if (width <= 0 || height <= 0)
    {
        clear();
        return;
    }

    const int outW = upscale ? width * 2 : width;
    const int outH = upscale ? height * 2 : height;

    const bool sameConfig =
        (m_inWidth == width &&
         m_inHeight == height &&
         m_outWidth == outW &&
         m_outHeight == outH &&
         needAlbedo == hasAlbedo() &&
         needNormal == hasNormal() &&
         needMotion == hasMotion());

    if (sameConfig)
        return;

    m_inWidth = width;
    m_inHeight = height;
    m_outWidth = outW;
    m_outHeight = outH;

    const size_t inPixels  = static_cast<size_t>(width) * height;
    const size_t outPixels = static_cast<size_t>(outW) * outH;

    m_colorBytes  = inPixels * 3 * sizeof(float);
    m_motionBytes = inPixels * 2 * sizeof(float);

    m_color.resize(inPixels * 3, 0.0f);

    m_output.resize(outPixels * 3, 0.0f);

    if (needAlbedo)
        m_albedo.resize(inPixels * 3, 0.0f);

    if (needNormal)
        m_normal.resize(inPixels * 3, 0.0f);

    if (needMotion)
        m_motion.resize(inPixels * 2, 0.0f);
}

void DenoiserData::clear() noexcept
{
    m_color.clear();
    m_albedo.clear();
    m_normal.clear();
    m_motion.clear();
    m_output.clear();

    m_inWidth = m_inHeight = m_outWidth = m_outHeight = 0;
    m_colorBytes = m_motionBytes = 0;
}

void DenoiserData::copyBuffer(std::vector<float>& dst,
                                const float* src,
                                size_t maxBytes)
{
    if (!src || dst.empty())
        return;

    const size_t dstBytes = dst.size() * sizeof(float);
    const size_t bytes = std::min(dstBytes, maxBytes);

    std::memcpy(dst.data(), src, bytes);
}

void DenoiserData::setColor(const float* data, size_t bytes)
{
    copyBuffer(m_color, data, bytes);
}

void DenoiserData::setAlbedo(const float* data, size_t bytes)
{
    copyBuffer(m_albedo, data, bytes);
}

void DenoiserData::setNormal(const float* data, size_t bytes)
{
    copyBuffer(m_normal, data, bytes);
}

void DenoiserData::setMotion(const float* data, size_t bytes)
{
    copyBuffer(m_motion, data, bytes);
}