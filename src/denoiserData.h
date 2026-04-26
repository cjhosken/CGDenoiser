#ifndef DENOISER_DATA_H
#define DENOISER_DATA_H

#include <vector>
#include <cstddef>


class DenoiserData
{
public:
    DenoiserData() = default;
    ~DenoiserData() = default;

    DenoiserData(const DenoiserData&) = delete;
    DenoiserData& operator=(const DenoiserData&) = delete;

    DenoiserData(DenoiserData&&) noexcept = default;
    DenoiserData& operator=(DenoiserData&&) noexcept = default;

    void allocate(int width, int height, 
        bool needAlbedo = false, 
        bool needNormal = false, 
        bool needMotion = false,
        bool upscale = false
    );

    void clear() noexcept;


    void setColor(const float* data, size_t bytes);
    void setAlbedo(const float* data, size_t bytes);
    void setNormal(const float* data, size_t bytes);
    void setMotion(const float* data, size_t bytes);

    int inWidth() const noexcept { return m_inWidth; }
    int inHeight() const noexcept { return m_inHeight; }
    int outWidth() const noexcept { return m_outWidth; }
    int outHeight() const noexcept { return m_outHeight; }

    size_t colorBytes() const { return m_colorBytes; }
    size_t motionBytes() const { return m_motionBytes; }

    float* color() noexcept { return m_color.data(); }
    const float* color() const noexcept { return m_color.data(); }

    float* albedo() noexcept { return m_albedo.empty() ? nullptr : m_albedo.data(); }
    const float* albedo() const noexcept { return m_albedo.empty() ? nullptr : m_albedo.data(); }

    float* normal() noexcept { return m_normal.empty() ? nullptr : m_normal.data(); }
    const float* normal() const noexcept { return m_normal.empty() ? nullptr : m_normal.data(); }

    float* motion() noexcept { return m_motion.empty() ? nullptr : m_motion.data(); }
    const float* motion() const noexcept { return m_motion.empty() ? nullptr : m_motion.data(); }

    float* output() noexcept { return m_output.data(); }
    const float* output() const noexcept { return m_output.data(); }
    
    // Status checks
    bool hasAlbedo() const noexcept { return !m_albedo.empty(); }
    bool hasNormal() const noexcept { return !m_normal.empty(); }
    bool hasMotion() const noexcept { return !m_motion.empty(); }
    bool valid() const noexcept { return m_inWidth > 0 && m_inHeight > 0; }

private:
    void copyBuffer(std::vector<float>& dst, const float * src, size_t maxBytes);

private:
    int m_inWidth, m_inHeight;
    int m_outWidth, m_outHeight;

    size_t m_colorBytes = 0;
    size_t m_motionBytes = 0;

    std::vector<float> m_color;      // RGB color data (3 channels)
    std::vector<float> m_albedo;     // RGB albedo data (3 channels) - optional
    std::vector<float> m_normal;     // RGB normal data (3 channels) - optional
    std::vector<float> m_motion;     // UV motion data (2 channels) - optional
    std::vector<float> m_output;     // RGB output data (3 channels)
};

#endif // DENOISER_DATA_H