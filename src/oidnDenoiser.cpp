#include "oidnDenoiser.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <cmath>

void OIDNDenoiser::setupDevice()
{
    // Destroy EVERYTHING tied to previous device
    m_filter = {};
    m_colorBuffer = {};
    m_outputBuffer = {};
    m_albedoBuffer = {};
    m_normalBuffer = {};
    m_device = {};

    const std::vector<oidn::DeviceType> devices = {
        oidn::DeviceType::Default
#if OIDN_CPU
        , oidn::DeviceType::CPU
#endif
#if OIDN_CUDA
        , oidn::DeviceType::CUDA
#endif
#if OIDN_HIP
        , oidn::DeviceType::HIP
#endif
#if OIDN_METAL
        , oidn::DeviceType::METAL
#endif
#if OIDN_SYCL
        , oidn::DeviceType::SYCL
#endif
    };

    if (device_types < 0 || device_types >= static_cast<int>(devices.size()))
    {
        std::cerr << "[OIDN] Invalid device index, falling back to Default\n";
        device_types = 0;
    }

    m_device = oidn::newDevice(devices[device_types]);

    m_device.setErrorFunction(
        [](void*, oidn::Error code, const char* msg)
        {
            std::cerr << "[OIDN] (" << static_cast<int>(code) 
                        << ") " << (msg ? msg : "") << "\n";
        },
        nullptr);

    m_device.commit();

    m_deviceDirty = false;
    m_filterDirty = true;
}


void OIDNDenoiser::setupFilter() 
{
    m_filter = {};

    m_filter = m_device.newFilter(kOIDNFilters[filter_type]);
    if (!m_filter) {
        std::cerr << "CRITICAL: OIDN Failed to create filter\n";
        return;
    }

    m_filter.setImage("color", m_colorBuffer, oidn::Format::Float3, m_width, m_height);
    m_filter.setImage("output", m_outputBuffer, oidn::Format::Float3, m_width, m_height);

    if (m_hasAlbedo)
    {
        m_filter.setImage("albedo", m_albedoBuffer, oidn::Format::Float3, m_width, m_height);

        if (m_hasNormal)
        {
            m_filter.setImage("normal", m_normalBuffer, oidn::Format::Float3, m_width, m_height);
        }
    }

    if (filter_type == 0) // RT
    {
        m_filter.set("hdr", filter_mode == 2);
        m_filter.set("srgb", filter_mode == 1);
    }
    else // RTLightmap
    {
        m_filter.set("directional", filter_directional);
    }

    static const oidn::Quality qualities[] = {
        oidn::Quality::Fast,
        oidn::Quality::Balanced,
        oidn::Quality::High
    };

    m_filter.set("quality", qualities[std::clamp(filter_quality, 0, 2)]);

    if (filter_inputScale <= 0.0f)
    {
        m_filter.set("inputScale", std::numeric_limits<float>::quiet_NaN());
    }
    else
    {
        m_filter.set("inputScale", filter_inputScale);
    }

    m_filter.set("cleanAux", filter_cleanAux);

    m_filter.commit();

    m_filterDirty = false;
}


void OIDNDenoiser::run(DenoiserData& data, bool deviceDirty, bool filterDirty)
{
    if (!data.valid())
        return;

    const int w = data.inWidth();
    const int h = data.inHeight();

    const bool hasAlbedo = data.hasAlbedo();
    const bool hasNormal = data.hasNormal();

    m_deviceDirty |= deviceDirty;
    m_filterDirty |= filterDirty;

    if (!m_device || m_deviceDirty || device_types != m_lastDeviceType)
    {
        setupDevice();
        m_lastDeviceType = device_types;
    }

    if (!m_device)
        return;

    const bool dimsChanged = (w != m_width || h != m_height);
    bool auxChanged = (hasAlbedo != m_hasAlbedo ||
                        hasNormal != m_hasNormal);

    bool filterTypeChanged = (filter_type != m_lastFilterType);

    m_width = w;
    m_height = h;
    m_hasAlbedo = hasAlbedo;
    m_hasNormal = hasNormal;
    m_lastFilterType = filter_type;

    if (dimsChanged || auxChanged || !m_colorBuffer || !m_outputBuffer)
    {
        const size_t bytes = data.colorBytes();

        m_colorBuffer = m_device.newBuffer(bytes);
        m_outputBuffer = m_device.newBuffer(bytes);

        m_albedoBuffer = hasAlbedo ? m_device.newBuffer(bytes) : oidn::BufferRef{};
        m_normalBuffer = hasNormal ? m_device.newBuffer(bytes) : oidn::BufferRef{};

        m_filterDirty = true;
    }

    if (m_filterDirty || filterTypeChanged || !m_filter)
    {
        setupFilter();
    }

    if (!m_filter)
    {
        std::cerr << "[OIDN] Filter is null, skipping\n";
        return;
    }


    const size_t pixels = static_cast<size_t>(w) * h;

    auto sanitize = [](float* data, size_t count)
    {
        if (!data) return;

        for (size_t i = 0; i < count; ++i)
        {
            if (!std::isfinite(data[i]))
                data[i] = 0.0f;
        }
    };

    // sanitize ALL inputs going into OIDN
    sanitize(data.color(), pixels * 3);

    if (data.hasAlbedo())
        sanitize(data.albedo(), pixels * 3);

    if (data.hasNormal())
        sanitize(data.normal(), pixels * 3);


    const size_t bytes = data.colorBytes();

    m_colorBuffer.write(0, bytes, data.color());

    if (data.hasAlbedo())
        m_albedoBuffer.write(0, bytes, data.albedo());

    if (data.hasNormal())
        m_normalBuffer.write(0, bytes, data.normal());
    
    m_filter.execute();

    const char* msg = nullptr;
    if (m_device.getError(msg) != oidn::Error::None)
    {
        std::cerr << "[OIDN Error] " << (msg ? msg : "") << "\n";
        return;
    }

    float* out = data.output();
    if (!out)
    {
        std::cerr << "[OIDN] Output buffer is null\n";
        return;
    }

    m_outputBuffer.read(0, bytes, out);
}
