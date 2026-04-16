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

void DenoiserData::allocate(int width, int height, bool hasAlbedo, bool hasNormal)
{
    if (m_width == width && m_height == height && 
        hasAlbedo == !m_albedo.empty() && 
        hasNormal == !m_normal.empty())
    {
        std::cout << "[DenoiserData] Using cached allocation" << std::endl;
        return; // Already allocated with same configuration
    }

    deallocate();

    m_width = width;
    m_height = height;

    // Calculate buffer sizes
    m_colorSize = static_cast<size_t>(width) * height * 3 * sizeof(float);

    std::cout << "[DenoiserData] Allocating: " << width << "x" << height 
              << " (hasAlbedo=" << hasAlbedo << ", hasNormal=" << hasNormal << ")" << std::endl;

    // Allocate color and output buffers (always needed)
    m_color.resize(width * height * 3, 0.0f);
    m_output.resize(width * height * 3, 0.0f);

    std::cout << "[DenoiserData] Allocated color/output buffers: " << width << "x" << height << std::endl;

    // Allocate optional buffers
    if (hasAlbedo)
    {
        m_albedo.resize(width * height * 3, 0.0f);
        std::cout << "[DenoiserData] Allocated albedo buffer" << std::endl;
    }

    if (hasNormal)
    {
        m_normal.resize(width * height * 3, 0.0f);
        std::cout << "[DenoiserData] Allocated normal buffer" << std::endl;
    }

    std::cout << "[DenoiserData] Allocation complete!" << std::endl;
}

void DenoiserData::deallocate()
{
    m_color.clear();
    m_albedo.clear();
    m_normal.clear();
    m_output.clear();

    m_width = 0;
    m_height = 0;
    m_colorSize = 0;

    std::cout << "[DenoiserData] Deallocated all buffers" << std::endl;
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