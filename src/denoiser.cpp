#include "denoiser.h"
#include <OpenImageDenoise/oidn.hpp>
#include <mutex>

CGDenoiser::CGDenoiser(Node *node) : PlanarIop(node)
{
    m_filterW = 0;
    m_filterH = 0;

    m_device = nullptr;
    m_filter = nullptr;
    m_colorBuffer = nullptr;
    m_outputBuffer = nullptr;

    m_deviceDirty = true;
    m_filterDirty = true;

    _engine = 0;
    m_defaultChannels = 3;
}

CGDenoiser::~CGDenoiser() {}

void CGDenoiser::setupDevice()
{
    m_device = nullptr;
    m_device = oidn::newDevice(oidn::DeviceType::CPU);
    if (m_device)
    {
        m_device.commit();
    }
}

void CGDenoiser::renderStripe(ImagePlane &plane)
{
    Box box = plane.bounds();
    int W = box.w();
    int H = box.h();

    Iop *colorIn = dynamic_cast<Iop *>(input(0));
    if (!colorIn || W <= 0 || H <= 0)
        return;

    ImagePlane inputPlane(box, false, Mask_RGB, 3);
    colorIn->fetchPlane(inputPlane);

    std::cerr << "A" << std::endl;

    if (!m_device || m_deviceDirty)
    {
        setupDevice();
        m_deviceDirty = false;
        m_filterDirty = true;
    }

    if (!m_device)
        return;

    std::cerr << "B" << std::endl;

    bool dimsChanged = (W != m_filterW || H != m_filterH);
    if (dimsChanged || m_filterDirty || !m_filter)
    {
        m_filterW = W;
        m_filterH = H;

        m_filter = nullptr;
        m_colorBuffer = nullptr;
        m_outputBuffer = nullptr;

        size_t bufferSize = static_cast<size_t>(m_filterW) * m_filterH * 3 * sizeof(float);

        m_colorBuffer = m_device.newBuffer(bufferSize);
        m_outputBuffer = m_device.newBuffer(bufferSize);

        m_filter = m_device.newFilter("RT");
        m_filter.setImage("color", m_colorBuffer, oidn::Format::Float3, W, H);
        m_filter.setImage("output", m_outputBuffer, oidn::Format::Float3, W, H);
        m_filter.commit();
        m_filterDirty = false;
    }

    std::cerr << "C" << std::endl;

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

    std::cerr << "D" << std::endl;

    m_filter.execute();

    std::cerr << "E" << std::endl;

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

void CGDenoiser::_validate(bool for_real) { copy_info(); }

static Iop *build(Node *node) { return new CGDenoiser(node); }
const Iop::Description CGDenoiser::desc("CGDenoiser", "Filter/CGDenoiser", build);