#include "denoiser.h"
#include <OpenImageDenoise/oidn.hpp>
#include <mutex>

static std::mutex g_oidn_mutex;

CGDenoiser::CGDenoiser(Node *node) : PlanarIop(node) 
{
    m_filterW = 0;
    m_filterH = 0;

    m_device = nullptr;
    m_filter = nullptr;

    m_colorBuffer = nullptr;
    m_outputBuffer = nullptr;

    m_defaultChannels = Mask_RGB;
    m_defaultNumberOfChannels = m_defaultChannels.size();

    m_allocatedSize = 0;

    setupDevice();
}

void CGDenoiser::setupDevice()
{
    try {
        m_device = oidn::newDevice();
        if (!m_device) {
            error("OIDN device creation failed");
            return;
        }

        m_device.set("numThreads", 0); // Use all available logical cores
        m_device.set("setAffinity", true);

        m_device.commit();

        const char* errorMessage;
        if (m_device.getError(errorMessage) != oidn::Error::None) {
            std::cerr << "[OIDN] Device Error: " << errorMessage << std::endl;
        }
    }
    catch (const std::exception &e) {
        std::string message = e.what();
        error("[OIDN]: %s", message.c_str());
    }
}

void CGDenoiser::setupFilter(bool hasAlbedo, bool hasNormal)
{
    try {
        if (!m_device) {
            error("Failed to create OIDN device");
            return; // Guard against null device
        }
        m_filter = m_device.newFilter("RT");

        if (!m_filter) {
            error("Failed to create OIDN filter");
            return;
        }

        m_filter.setImage("color", m_colorBuffer, oidn::Format::Float3, m_filterW, m_filterH);
        m_filter.setImage("output", m_outputBuffer, oidn::Format::Float3, m_filterW, m_filterH);

        if (hasAlbedo && m_albedoBuffer) {
            m_filter.setImage("albedo", m_albedoBuffer, oidn::Format::Float3, m_filterW, m_filterH);
        }
        if (hasNormal && m_normalBuffer) {
            m_filter.setImage("normal", m_normalBuffer, oidn::Format::Float3, m_filterW, m_filterH);
        }

        m_filter.set("hdr", true);

        m_filter.commit();
    }
    catch (const std::exception &e)
    {
        std::string message = e.what();
        error("[OIDN]: %s", message.c_str());
    }
}

void CGDenoiser::knobs(Knob_Callback f) {
    const char* engine_names[] = {
        "OpenImageDenoise (OIDN)",
        #ifdef USE_OPTIX
        "NVIDIA OptiX",
        #endif
        nullptr
    };
    Enumeration_knob(f, &_engine, engine_names, "engine", "Engine");
}

void CGDenoiser::renderStripe(ImagePlane& plane) {
    if (!m_device) return;

    DD::Image::Box imageBounds = plane.bounds();
    m_filterW = imageBounds.w();
    m_filterH = imageBounds.h();

    Iop* colorIn  = dynamic_cast<Iop*>(input(0));
    Iop* albedoIn = dynamic_cast<Iop*>(input(1));
    Iop* normalIn = dynamic_cast<Iop*>(input(2));

    if (!colorIn) return;

    bool hasAlbedo = (albedoIn != nullptr);
    bool hasNormal = (normalIn != nullptr);

    auto bufferSize = m_filterW * m_filterH * m_defaultNumberOfChannels * sizeof(float);

    if (!m_colorBuffer || m_allocatedSize != bufferSize)
    {
        m_colorBuffer = m_device.newBuffer(bufferSize);
        m_outputBuffer = m_device.newBuffer(bufferSize);
        
        m_albedoBuffer = hasAlbedo ? m_device.newBuffer(bufferSize) : nullptr;
        m_normalBuffer = hasNormal ? m_device.newBuffer(bufferSize) : nullptr;
        
        m_allocatedSize = bufferSize;
    }

    setupFilter(hasAlbedo, hasNormal);

    for (int i = 0; i < node_inputs(); i++) {
        Iop* curIn = dynamic_cast<Iop*>(input(i));
        if (!curIn) continue;

        curIn->request(imageBounds, m_defaultChannels, 0);
        DD::Image::ImagePlane inputPlane(imageBounds, false, m_defaultChannels, m_defaultNumberOfChannels);
        curIn->fetchPlane(inputPlane);

        float* targetBuf = nullptr;
        if (i == 0) targetBuf = static_cast<float*>(m_colorBuffer.getData());
        if (i == 1) targetBuf = static_cast<float*>(m_albedoBuffer.getData());
        if (i == 2) targetBuf = static_cast<float*>(m_normalBuffer.getData());

        if (!targetBuf) continue;
    
        for (int y = 0; y < imageBounds.t(); y++) {
            for (int x = 0; x < imageBounds.r(); x++) {
                size_t pixelOffset = (size_t(y) * m_filterW + x) * m_defaultNumberOfChannels;

                targetBuf[pixelOffset + 0] = inputPlane.at(x, y, 0);
                targetBuf[pixelOffset + 1] = inputPlane.at(x, y, 1);
                targetBuf[pixelOffset + 2] = inputPlane.at(x, y, 2);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_oidn_mutex);
        if (_engine == 0) {
            runOIDN();
        }
        #ifdef USE_OPTIX
        if (_engine == 1) {
            runOptiX();
        }
        #endif
    }


    plane.writable();
    float* outputPtr = static_cast<float*>(m_outputBuffer.getData());

    std::vector<Channel> activeChans;
    foreach(c, m_defaultChannels) {
        activeChans.push_back(c);
    }

    // Ensure we don't exceed the number of channels OIDN actually processed (usually 3)
    int numChans = std::min((int)activeChans.size(), m_defaultNumberOfChannels);

    for(int z = 0; z < numChans; z++) {
        Channel c = activeChans[z];
        int pIdx = plane.chanNo(c);
        if (pIdx < 0) continue;
        
        for (int y = 0; y < imageBounds.t(); y++) {
            for (int x = 0; x < imageBounds.r(); x++) {
                size_t index = (size_t(y) * m_filterW + x) * m_defaultNumberOfChannels + z;
                plane.writableAt(x, y, pIdx) = outputPtr[index];
            }
        }
    }
}

void CGDenoiser::runOIDN() {
    try {
        m_filter.execute();
    }
    catch (const std::exception &e) {
        std::string message = e.what();
        error("[OIDN]: %s", message.c_str());
    }
}
#ifdef USE_OPTIX
void CGDenoiser::runOptiX() {

}
#endif

void CGDenoiser::_validate(bool for_real) { copy_info(); }
static Iop* build(Node* node) {return new CGDenoiser(node); }
const Iop::Description CGDenoiser::desc("CGDenoiser", "Filter/CGDenoiser", build);