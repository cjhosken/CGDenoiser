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
    m_device = nullptr;
    m_device = oidn::newDevice(oidn::DeviceType::CPU);
    if (m_device)
    {
        m_device.commit();
    }
}

void OIDNDenoiser::setupFilter() {
    m_filter = m_device.newFilter(OIDN_Filter[filter_type]);

    m_filter.setImage("color", m_colorBuffer, oidn::Format::Float3, m_width, m_height);
    m_filter.setImage("output", m_outputBuffer, oidn::Format::Float3, m_width, m_height);

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
    m_filter.commit();
}


void OIDNDenoiser::render(ImagePlane &plane, ImagePlane &inputPlane, Box box)
{
    int W = box.w();
    int H = box.h();


    if (!m_device || m_deviceDirty)
    {
        setupDevice();
        m_deviceDirty = false;
        m_filterDirty = true;
    }

    if (!m_device)
        return;

    bool dimsChanged = (W != m_width || H != m_height);
    if (dimsChanged || m_filterDirty || !m_filter)
    {
        m_width = W;
        m_height = H;

        m_filter = nullptr;
        m_colorBuffer = nullptr;
        m_outputBuffer = nullptr;

        size_t bufferSize = static_cast<size_t>(m_width) * m_height * 3 * sizeof(float);

        m_colorBuffer = m_device.newBuffer(bufferSize);
        m_outputBuffer = m_device.newBuffer(bufferSize);

        if (!m_filter || m_filterDirty)
        {
            setupFilter();
            m_filterDirty = false;
        }
    }

    float *colorPtr = (float *)m_colorBuffer.getData();
    if (!colorPtr)
        return;

    for (int y = box.y(); y < box.t(); y++)
    {
        // OIDN expects top-to-bottom.
        // We map Nuke's bottom row to OIDN's top row.
        size_t oidnY = (size_t)(box.t() - 1 - y);

        for (int x = box.x(); x < box.r(); x++)
        {
            size_t lx = (size_t)(x - box.x());
            size_t idx = (oidnY * W + lx) * 3;

            colorPtr[idx + 0] = inputPlane.at(x, y, 0);
            colorPtr[idx + 1] = inputPlane.at(x, y, 1);
            colorPtr[idx + 2] = inputPlane.at(x, y, 2);
        }
    }

    m_filter.execute();

    plane.writable();
    float *denoisedPtr = (float *)m_outputBuffer.getData();
    if (!denoisedPtr)
        return;

    DD::Image::Channel rgb[3] = {DD::Image::Chan_Red, DD::Image::Chan_Green, DD::Image::Chan_Blue};
    for (int z = 0; z < 3; z++) // Hardcode to 3 since OIDN buffer is Float3
    {
        int pIdx = plane.chanNo(rgb[z]);
        if (pIdx < 0)
            continue;

        for (int y = box.y(); y < box.t(); y++)
        {
            // Reverse the Y mapping again to get back to Nuke space
            size_t oidnY = (size_t)(box.t() - 1 - y);

            for (int x = box.x(); x < box.r(); x++)
            {
                size_t lx = (size_t)(x - box.x());
                size_t oidnIdx = (oidnY * W + lx) * 3 + z;

                plane.writableAt(x, y, pIdx) = denoisedPtr[oidnIdx];
            }
        }
    }
}