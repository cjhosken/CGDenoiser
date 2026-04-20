#include "oidnDenoiser.h"
#include <algorithm>

OIDNDenoiser::OIDNDenoiser() {
    m_width = 0;
    m_height = 0;

    m_filterDirty = true;
    device_type = 0;
    filter_type = 0;
    filter_mode = 0;

    filter_inputScale = 1.0;
    filter_cleanAux = false;
    filter_quality = 0;
    filter_directional = false;

    m_defaultNumChannels = 3;
}

OIDNDenoiser::~OIDNDenoiser() {}

void OIDNDenoiser::setupDevice()
{
    // Destroy EVERYTHING tied to previous device
    m_filter = nullptr;
    m_colorBuffer = {};
    m_outputBuffer = {};
    m_albedoBuffer = {};
    m_normalBuffer = {};

    m_device = nullptr;

    std::vector<oidn::DeviceType> device_list = {
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

    std::cout << "Making new device..." << std::endl;

    m_device = oidn::newDevice(device_list.at(device_type));

    std::cout << "New device made!" << std::endl;


    m_device.setErrorFunction(
        [](void*, oidn::Error code, const char* msg)
        {
            std::cerr << "[OIDN] (" << (int)code << ") " << (msg ? msg : "") << "\n";
        },
        nullptr
    );

    m_device.commit();

    m_filterDirty = true;
}


void OIDNDenoiser::setupFilter() {
    m_filter = nullptr; // KILL the old filter object immediately

    m_filter = m_device.newFilter(OIDN_Filter[filter_type]);
    if (!m_filter) {
        std::cerr << "CRITICAL: OIDN Failed to create filter type: " << OIDN_Filter[filter_type] << std::endl;
        return;
    }

    m_filter.setImage("color", m_colorBuffer, oidn::Format::Float3, m_width, m_height);
    m_filter.setImage("output", m_outputBuffer, oidn::Format::Float3, m_width, m_height);

    if (m_albedoBuffer)
    {
        m_filter.setImage("albedo", m_albedoBuffer, oidn::Format::Float3, m_width, m_height);
    }

    if (m_normalBuffer)
    {
        m_filter.setImage("normal", m_normalBuffer, oidn::Format::Float3, m_width, m_height);
    }

    if (filter_type == 0)
    {
        m_filter.set("hdr", filter_mode == 2);
        m_filter.set("srgb", filter_mode == 1);
    }
    else if (filter_type == 1)
    {
        m_filter.set("directional", filter_directional);
    }

    m_filter.set("inputScale", std::max(0.01f, filter_inputScale));
    m_filter.set("cleanAux", filter_cleanAux);

    static const oidn::Quality quality[] = {
        oidn::Quality::Default,
        oidn::Quality::Fast,
        oidn::Quality::Balanced,
        oidn::Quality::High
    };

    m_filter.set("quality", quality[filter_quality]);
    m_filter.commit();

    m_filterDirty = false;
}


void OIDNDenoiser::run(DenoiserData& data, bool deviceDirty, bool filterDirty)
{
    int w = data.getWidth();
    int h = data.getHeight();

    bool hasAlbedo = data.hasAlbedo();
    bool hasNormal = data.hasNormal();

    if (!m_device || deviceDirty || device_type != m_lastDeviceType)
    {
        setupDevice();
        m_lastDeviceType = device_type;
    }

    if (!m_device)
        return;

    const bool dimsChanged = (w != m_width || h != m_height);
    bool auxChanged =
        (hasAlbedo != m_hasAlbedo ||
         hasNormal != m_hasNormal);


    bool filterChanged =
        filterDirty ||
        dimsChanged ||
        auxChanged ||
        (filter_type != m_lastFilterType);

    m_width = w;
    m_height = h;
    m_hasAlbedo = hasAlbedo;
    m_hasNormal = hasNormal;
    m_lastFilterType = filter_type;

    if (dimsChanged || auxChanged || !m_colorBuffer || !m_outputBuffer)
    {
        size_t bufferSize = data.getColorSize();

        m_colorBuffer = m_device.newBuffer(bufferSize);
        m_outputBuffer = m_device.newBuffer(bufferSize);

        m_albedoBuffer = hasAlbedo ? m_device.newBuffer(bufferSize) : oidn::BufferRef{};
        m_normalBuffer = hasNormal ? m_device.newBuffer(bufferSize) : oidn::BufferRef{};

        m_filterDirty = true;
    }

    if (m_filterDirty || !m_filter)
    {
        setupFilter();
        m_filterDirty = false;
    }

    size_t bufferSize = data.getColorSize();

    m_colorBuffer.write(0, bufferSize, data.getColor());

    if (data.hasAlbedo()) {
        m_albedoBuffer.write(0, bufferSize, data.getAlbedo());
    }

    if (data.hasNormal()) {
        m_normalBuffer.write(0, bufferSize, data.getNormal());
    }

    if (!m_filter) {
        std::cerr << "[OIDN] Filter is null, skipping.\n";
        return;
    }

    m_filter.execute();

    const char* message = nullptr;
    const auto err = m_device.getError(message);
    if (err != oidn::Error::None)
    {
        std::cerr << "[OIDN Error] (" << static_cast<int>(err) << ") " << (message ? message : "") << std::endl;
        return;
    }

    float* outputPtr = data.getOutput();
    if (!outputPtr)
    {
        std::cerr << "[OIDN Error] Output buffer is null!" << std::endl;
        return;
    }

    m_outputBuffer.read(0, bufferSize, outputPtr);
}
