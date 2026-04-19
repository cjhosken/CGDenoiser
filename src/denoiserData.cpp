#include "denoiserData.h"
#include <iostream>

DenoiserData::DenoiserData()
    : m_width(0), m_height(0), m_colorSize(0)
{
}

DenoiserData::~DenoiserData()
{
    deallocate();
}

void DenoiserData::allocate(int width, int height, bool hasAlbedo, bool hasNormal, bool hasMotion)
{
    if (m_width == width && m_height == height && 
        hasAlbedo == !m_albedo.empty() && 
        hasNormal == !m_normal.empty() &&
        hasMotion == !m_motion.empty())
    {
        return; // Already allocated with same configuration
    }

    deallocate();

    m_width = width;
    m_height = height;

    // Calculate buffer sizes
    m_colorSize = static_cast<size_t>(width) * height * 3 * sizeof(float);
    m_motionSize = static_cast<size_t>(width) * height * 2 * sizeof(float);

    // Allocate color and output buffers (always needed)
    m_color.resize(width * height * 3, 0.0f);
    m_output.resize(width * height * 3, 0.0f);

    // Allocate optional buffers
    if (hasAlbedo)
    {
        m_albedo.resize(width * height * 3, 0.0f);
    }

    if (hasNormal)
    {
        m_normal.resize(width * height * 3, 0.0f);
    }

    if (hasMotion)
    {
        m_motion.resize(width * height * 2, 0.0f);
    }
}

void DenoiserData::deallocate()
{
    m_color.clear();
    m_albedo.clear();
    m_normal.clear();
    m_motion.clear();
    m_output.clear();

    m_width = 0;
    m_height = 0;
    m_colorSize = 0;
}

void DenoiserData::setColor(const float* data, size_t size)
{
    if (!data || m_color.empty())
        return;

    size_t copySize = (size < m_colorSize) ? size : m_colorSize;
    std::memcpy(m_color.data(), data, copySize);
}

void DenoiserData::setAlbedo(const float* data, size_t size)
{
    if (!data || m_albedo.empty())
        return;

    size_t copySize = (size < m_colorSize) ? size : m_colorSize;
    std::memcpy(m_albedo.data(), data, copySize);
}

void DenoiserData::setNormal(const float* data, size_t size)
{
    if (!data || m_normal.empty())
        return;

    size_t copySize = (size < m_colorSize) ? size : m_colorSize;
    std::memcpy(m_normal.data(), data, copySize);
}

void DenoiserData::setMotion(const float* data, size_t size)
{
    if (!data || m_motion.empty())
        return;

    size_t copySize = (size < m_colorSize) ? size : m_motionSize;
    std::memcpy(m_motion.data(), data, copySize);
}