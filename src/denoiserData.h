#ifndef DENOISER_DATA_H
#define DENOISER_DATA_H

#include <vector>
#include <cstring>

/**
 * @class DenoiserData
 * @brief Shared data structure for storing denoising input/output buffers
 * 
 * This class manages memory for image data that can be used by both
 * OIDN and OptiX denoisers, eliminating duplication and simplifying
 * data flow management.
 */
class DenoiserData
{
public:
    DenoiserData();
    ~DenoiserData();

    /**
     * @brief Allocate buffers based on dimensions and channel requirements
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param hasAlbedo Whether to allocate albedo buffer
     * @param hasNormal Whether to allocate normal buffer
     */
    void allocate(int width, int height, bool hasAlbedo = false, bool hasNormal = false);

    /**
     * @brief Release all allocated buffers
     */
    void deallocate();

    /**
     * @brief Set color buffer data
     */
    void setColor(const float* data, size_t size);

    /**
     * @brief Set albedo buffer data
     */
    void setAlbedo(const float* data, size_t size);

    /**
     * @brief Set normal buffer data
     */
    void setNormal(const float* data, size_t size);

    // Accessors
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    size_t getColorSize() const { return m_colorSize; }

    float* getColor() { return m_color.data(); }
    const float* getColor() const { return m_color.data(); }

    float* getAlbedo() { return m_albedo.empty() ? nullptr : m_albedo.data(); }
    const float* getAlbedo() const { return m_albedo.empty() ? nullptr : m_albedo.data(); }

    float* getNormal() { return m_normal.empty() ? nullptr : m_normal.data(); }
    const float* getNormal() const { return m_normal.empty() ? nullptr : m_normal.data(); }

    float* getOutput() { return m_output.data(); }
    const float* getOutput() const { return m_output.data(); }
    

    // Status checks
    bool hasAlbedo() const { return !m_albedo.empty(); }
    bool hasNormal() const { return !m_normal.empty(); }
    bool isAllocated() const { return m_width > 0 && m_height > 0; }

private:
    int m_width;
    int m_height;

    std::vector<float> m_color;      // RGB color data (3 channels)
    std::vector<float> m_albedo;     // RGB albedo data (3 channels) - optional
    std::vector<float> m_normal;     // RGB normal data (3 channels) - optional
    std::vector<float> m_output;     // RGB output data (3 channels)

    size_t m_colorSize;  // Size in bytes for color/albedo/normal buffers
};

#endif // DENOISER_DATA_H
