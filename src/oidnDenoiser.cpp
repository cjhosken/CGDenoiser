#include "oidnDenoiser.h"

OIDNDenoiser::OIDNDenoiser() {
    m_width = 0;
    m_height = 0;

    m_deviceDirty = true;
    m_filterDirty = true;

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

    m_device = nullptr;
    m_device = oidn::newDevice(oidn::DeviceType::CPU);
    if (m_device)
    {
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

    m_filter.set("quality", OIDN_Quality[filter_quality]);

    std::cout << "[OIDN] Filter Created!" << std::endl;

    m_filter.commit();

    std::cout << "[OIDN] Filter Comitted!" << std::endl;

}


void OIDNDenoiser::run(float* color, float* albedo, float* normal, int w, int h)
{
    std::cout << "[OIDN] Rendering..." << std::endl;

    if (!m_device || m_deviceDirty)
    {
        setupDevice();
        m_deviceDirty = false;
        m_filterDirty = true;
    }

    if (!m_device)
        return;
    
    bool dimsChanged = (w != m_width || h != m_height);
    if (dimsChanged || m_filterDirty || !m_filter)
    {
        std::cout << "[OIDN] Dims Changed! Updating Buffers..." << std::endl;

        m_width = w;
        m_height = h;

        size_t bufferSize = static_cast<size_t>(m_width) * m_height * 3 * sizeof(float);

        m_colorBuffer = m_device.newBuffer(bufferSize);
        m_outputBuffer = m_device.newBuffer(bufferSize);

        if (albedo) {
            m_albedoBuffer = m_device.newBuffer(bufferSize);
        } else {m_albedoBuffer = nullptr;}

        if (normal) {
            m_normalBuffer = m_device.newBuffer(bufferSize);
        } else {
            m_albedoBuffer = nullptr;
        }

        setupFilter();
        m_filterDirty = false;
    }

    size_t bufferSize = (size_t)w * h * 3 * sizeof(float);

    std::cout << "[OIDN] Getting Data..." << std::endl;

    float *colorPtr = (float *)m_colorBuffer.getData();
    float* outputPtr = (float*)m_outputBuffer.getData();

    if (!colorPtr || !outputPtr)
        return;

    memcpy(colorPtr, color, bufferSize);

    if (albedo) {
        float* albedoPtr = (float*)m_albedoBuffer.getData();
        memcpy(albedoPtr, albedo, bufferSize);
    }

    if (normal) {
        float* normalPtr = (float*)m_normalBuffer.getData();
        memcpy(normalPtr, normal, bufferSize);
    }

    std::cout << "[OIDN] Data Retrieved! Executing Denoiser..." << std::endl;

    m_filter.execute();

    std::cout << "[OIDN] Denoised! Writing Data..." << std::endl;

    memcpy(color, outputPtr, (size_t)w * h * 3 * sizeof(float));
    std::cout << "[OIDN] Finished!" << std::endl;
}