#include "oidnDenoiser.h"
#include <algorithm>

OIDNDenoiser::OIDNDenoiser() {
    m_width = 0;
    m_height = 0;

    m_filterDirty = true;
    device_type = 0;

    filter_type = 0;
    filter_hdr = false;
    filter_srgb = false;
    filter_inputScale = 1.0;
    filter_cleanAux = false;
    filter_quality = 0;
    filter_directional = false;

    m_defaultNumChannels = 3;
}

OIDNDenoiser::~OIDNDenoiser() {}

void OIDNDenoiser::setupDevice() {
    std::cout << "[OIDN] Setting up Device..." << std::endl;

    oidn::DeviceType type = oidn::DeviceType::Default;

    oidn::DeviceType device_types[] = {
        oidn::DeviceType::Default,

        #if OIDN_CPU
        oidn::DeviceType::CPU,
        #endif

        #if OIDN_CUDA
        oidn::DeviceType::CUDA,
        #endif

        #if OIDN_HIP
        oidn::DeviceType::HIP,
        #endif

        #if OIDN_METAL
        oidn::DeviceType::Metal,
        #endif

        #if OIDN_SYCL
        oidn::DeviceType::SYCL
        #endif
    };

    m_device = nullptr;

    const int maxIndex = static_cast<int>(sizeof(device_types) / sizeof(device_types[0])) - 1;
    const int clampedIndex = std::max(0, std::min(device_type, maxIndex));
    type = device_types[clampedIndex];

    std::cout << "[OIDN] Using Device Type... " << std::endl;
    try
    {
        m_device = oidn::newDevice(type);
        m_device.setErrorFunction([](void*, oidn::Error code, const char* message) {
            std::cerr << "[OIDN Error] (" << static_cast<int>(code) << ") " << (message ? message : "") << std::endl;
        });
        m_device.commit();
    }
    catch(const std::exception& e)
    {
        std::cerr << "[OIDN Error] Failed to init device: " << e.what() << std::endl;
        std::cerr << "[OIDN] Falling back to Default device..." << std::endl;
        m_device = oidn::newDevice(oidn::DeviceType::Default);
        m_device.setErrorFunction([](void*, oidn::Error code, const char* message) {
            std::cerr << "[OIDN Error] (" << static_cast<int>(code) << ") " << (message ? message : "") << std::endl;
        });
        m_device.commit();
    }
    
    std::cout << "[OIDN] Device Created!" << std::endl;
}

void OIDNDenoiser::setupFilter() {
    std::cout << "[OIDN] Setting up Filter..." << std::endl;

    m_filter = m_device.newFilter(OIDN_Filter[filter_type]);

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
        m_filter.set("hdr", filter_hdr);
        m_filter.set("srgb", filter_srgb);
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

    std::cout << "[OIDN] Filter Created!" << std::endl;

    m_filter.commit();

    std::cout << "[OIDN] Filter Comitted!" << std::endl;

}


void OIDNDenoiser::run(DenoiserData& data, bool deviceDirty, bool filterDirty)
{
    std::cout << "[OIDN] Rendering..." << std::endl;

    int w = data.getWidth();
    int h = data.getHeight();

    if (!m_device || deviceDirty)
    {
        setupDevice();
        m_filterDirty = true;
    }

    if (!m_device)
        return;

    if (filterDirty)
        m_filterDirty = true;
    
    const bool dimsChanged = (w != m_width || h != m_height);
    const bool auxChanged =
        (data.hasAlbedo() != static_cast<bool>(m_albedoBuffer)) ||
        (data.hasNormal() != static_cast<bool>(m_normalBuffer));

    if (dimsChanged || auxChanged || !m_colorBuffer || !m_outputBuffer)
    {
        std::cout << "[OIDN] Dims Changed! Updating Buffers..." << std::endl;

        m_width = w;
        m_height = h;

        size_t bufferSize = data.getColorSize();

        m_colorBuffer = m_device.newBuffer(bufferSize);
        m_outputBuffer = m_device.newBuffer(bufferSize);

        if (data.hasAlbedo()) {
            m_albedoBuffer = m_device.newBuffer(bufferSize);
        } else {
            m_albedoBuffer = {};
        }

        if (data.hasNormal()) {
            m_normalBuffer = m_device.newBuffer(bufferSize);
        } else {
            m_normalBuffer = {};
        }

        m_filterDirty = true;
    }

    if (m_filterDirty || !m_filter)
    {
        setupFilter();
        m_filterDirty = false;
    }

    size_t bufferSize = data.getColorSize();

    std::cout << "[OIDN] Getting Data..." << std::endl;

    m_colorBuffer.write(0, bufferSize, data.getColor());

    if (data.hasAlbedo()) {
        m_albedoBuffer.write(0, bufferSize, data.getAlbedo());
    }

    if (data.hasNormal()) {
        m_normalBuffer.write(0, bufferSize, data.getNormal());
    }

    std::cout << "[OIDN] Data Retrieved! Executing Denoiser..." << std::endl;

    m_filter.execute();

    const char* message = nullptr;
    const auto err = m_device.getError(message);
    if (err != oidn::Error::None)
    {
        std::cerr << "[OIDN Error] (" << static_cast<int>(err) << ") " << (message ? message : "") << std::endl;
        return;
    }

    std::cout << "[OIDN] Denoised! Writing Data..." << std::endl;

    float* outputPtr = data.getOutput();
    if (!outputPtr)
    {
        std::cerr << "[OIDN Error] Output buffer is null!" << std::endl;
        return;
    }

    m_outputBuffer.read(0, bufferSize, outputPtr);
    std::cout << "[OIDN] Finished!" << std::endl;
}
