#include "denoiserData.h"
#include <algorithm>


void DenoiserData::allocate(int width, int height, 
    bool needAlbedo, 
    bool needNormal, 
    bool needMotion)
{
    if (width <= 0 || height <= 0)
    {
        clear();
        return;
    }

    const bool sameConfig = 
        (m_width == width && m_height == height) &&
        (needAlbedo = hasAlbedo()) &&
        (needNormal == hasNormal()) &&
        (needMotion == hasMotion());

    if (sameConfig)
        return;

    m_width = width;
    m_height = height;

    const size_t pixels = static_cast<size_t>(width) * height;

    m_colorBytes = pixels * 3 * sizeof(float);
    m_motionBytes = pixels * 2 * sizeof(float);

    // Allocate color and output buffers (always needed)
    m_color.resize(pixels * 3);
    m_output.resize(pixels * 3);

    // Allocate optional buffers
    if (needAlbedo)
        m_albedo.resize(pixels * 3);
    else
        m_albedo.clear();

    if (needNormal)
        m_normal.resize(pixels * 3);
    else
        m_normal.clear();

    if (needMotion)
        m_motion.resize(pixels * 2);
    else
        m_motion.clear();
}

void DenoiserData::clear() noexcept
{
    m_color.clear();
    m_albedo.clear();
    m_normal.clear();
    m_motion.clear();
    m_output.clear();

    m_width = m_height = 0;
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