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
    // OIDN device creation causes crashes in Nuke environment - disabled for now
    std::cout << "Making new Device" << std::endl;
    m_device = oidnNewDevice(OIDNDeviceType::OIDN_DEVICE_TYPE_CPU);
    m_device.commit();
}

void OIDNDenoiser::setupFilter() {
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
    m_filter.commit();
}


void OIDNDenoiser::run(DenoiserData& data, bool deviceDirty, bool filterDirty)
{
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

    m_colorBuffer.write(0, bufferSize, data.getColor());

    if (data.hasAlbedo()) {
        m_albedoBuffer.write(0, bufferSize, data.getAlbedo());
    }

    if (data.hasNormal()) {
        m_normalBuffer.write(0, bufferSize, data.getNormal());
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
