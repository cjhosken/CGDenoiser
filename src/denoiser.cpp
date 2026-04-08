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

    _engine = 0;
    
    // --- OIDN Setting Defaults ---
    _filterType = 0;

    _numThreads = 0;
    _setAffinity = true;

    _hdr = false;
    _srgb = false;
    _inputScale = 1.0;
    _cleanAux = false;
    _quality = 2;
    _maxMemoryMB = -1;
    _directional = false;

    setupDevice();
}

void CGDenoiser::setupDevice()
{
    try {
        m_device = oidn::newDevice(oidn::DeviceType::CUDA);
        if (!m_device) {
            m_device = oidn::newDevice(oidn::DeviceType::CPU);
        }

        if (!m_device) {
            error("OIDN device creation failed");
            return;
        }

        m_device.set("numThreads", _numThreads); // Use all available logical cores
        m_device.set("setAffinity", _setAffinity);

        m_device.commit();
    }
    catch (...) {
        try {
            // Fallback logic if CUDA init fails
            m_device = oidn::newDevice(oidn::DeviceType::CPU);
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
}

void CGDenoiser::setupFilter(bool hasAlbedo, bool hasNormal)
{
    try {
        if (!m_device) return;

        const char* filterName = (_filterType == 1) ? "RTLightmap" : "RT";
        m_filter = m_device.newFilter(filterName);

        if (!m_filter) return;

        m_filter.setImage("color", m_colorBuffer, oidn::Format::Float3, m_filterW, m_filterH);
        m_filter.setImage("output", m_outputBuffer, oidn::Format::Float3, m_filterW, m_filterH);

        if (hasAlbedo && m_albedoBuffer)
            m_filter.setImage("albedo", m_albedoBuffer, oidn::Format::Float3, m_filterW, m_filterH);

        if (hasNormal && m_normalBuffer)
            m_filter.setImage("normal", m_normalBuffer, oidn::Format::Float3, m_filterW, m_filterH);

        oidn::Quality q = oidn::Quality::Balanced;
        if (_quality == 0)      q = oidn::Quality::Fast;
        else if (_quality == 2) q = oidn::Quality::High;
        m_filter.set("quality", q);

        m_filter.set("srgb", _srgb);

        if (_inputScale <= 0.0f) {
            m_filter.set("inputScale", std::numeric_limits<float>::quiet_NaN());
        } else {
            m_filter.set("inputScale", _inputScale);
        }

        m_filter.set("hdr", _hdr);
        m_filter.set("cleanAux", _cleanAux);
        
        if (_maxMemoryMB > 0) {
            m_filter.set("maxMemoryMB", _maxMemoryMB);
        }
        
        // 5. RTLightmap specialized setting
        if (_filterType == 1) {
            m_filter.set("directional", _directional);
        }

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
    Tooltip(f, "Select the denoising backend.");

    Divider(f);

    // --- Main OIDN Settings ---

    const char* filter_names[] = { "Ray Tracing (Beauty)", "RT Lightmap", nullptr };
    Enumeration_knob(f, &_filterType, filter_names, "filter_type", "Filter Type");
    
    const char* quality_names[] = { "Fast", "Balanced", "High", nullptr };
    Enumeration_knob(f, &_quality, quality_names, "quality", "Quality");
    Tooltip(f, "Fast: lowest quality. Balanced: medium. High: best quality, slowest.");

    Bool_knob(f, &_hdr, "hdr", "HDR");
    Tooltip(f, "The main input image is HDR");

    Bool_knob(f, &_srgb, "srgb", "sRGB");
    Tooltip(f, "The main input image is encoded with the sRGB (or 2.2 gamma) curve (LDR only) or is linear; the output will be encoded with the same curve");

    Float_knob(f, &_inputScale, "input_scale", "Input Scale");
    Tooltip(f, "Scales values in the main input image before filtering, without scaling the output too, which can be used to map color or auxiliary feature values to the expected range, e.g. for mapping HDR values to physical units (which affects the quality of the output but not the range of the output values); if set to NaN, the scale is computed implicitly for HDR images or set to 1 otherwise");

    Bool_knob(f, &_cleanAux, "clean_aux", "Clean Aux (Albedo/Normal)");
    Tooltip(f, "The auxiliary feature (albedo, normal) images are noise-free; recommended for highest quality but should not be enabled for noisy auxiliary images to avoid residual noise");

    Int_knob(f, &_numThreads, "num_threads", "Threads");
    Tooltip(f, "Maximum number of threads which the library should use; 0 will set it automatically to get the best performance.");

    Int_knob(f, &_maxMemoryMB, "max_memory", "Max Memory (MB)");
    Tooltip(f, "If set to >= 0, a request is made to limit the memory usage below the specified amount in megabytes at the potential cost of slower performance, but actual memory usage may be higher (the target may not be achievable or there may be additional allocations beyond the control of the library); otherwise, memory usage will be limited to an unspecified device-dependent amount; in both cases, filters on the same device share almost all of their allocated memory to minimize total memory usage");

    Bool_knob(f, &_setAffinity, "set_affinity", "Set Affinity");
    Tooltip(f, "Enables thread affinitization (pinning software threads to hardware threads) if it is necessary for achieving optimal performance.");

    Divider(f);

    // --- Advanced / Specialized ---
    Bool_knob(f, &_directional, "directional", "Directional (Lightmap Only)");
    Tooltip(f, "Only used for RTLightmap filter to handle directional components.");
}

int CGDenoiser::knob_changed(Knob* k) {
    bool isOIDN = (_engine == 0);

    static const std::set<std::string> dirtyKnobs = {
        "engine", "filter_type", "quality", "hdr", "srgb", 
        "clean_aux", "num_threads", "max_memory", "set_affinity", 
        "input_scale", "directional"
    };

    if (dirtyKnobs.count(k->name())) {
        m_settingsDirty = true;
    }

    if (k->is("engine") || k->is("showPanel")) {
        knob("filter_type")->visible(isOIDN);
        knob("quality")->visible(isOIDN);
        knob("hdr")->visible(isOIDN);
        knob("srgb")->visible(isOIDN);
        knob("clean_aux")->visible(isOIDN);
        knob("num_threads")->visible(isOIDN);
        knob("max_memory")->visible(isOIDN);
        knob("set_affinity")->visible(isOIDN);
        knob("input_scale")->visible(isOIDN);

        knob("directional")->visible(_filterType == 1 && isOIDN);
        return 1;
    }

    if (k->is("filter_type")) {
        knob("directional")->visible(_filterType == 1 && isOIDN);
        return 1;
    }

    return PlanarIop::knob_changed(k);
}

void CGDenoiser::renderStripe(ImagePlane& plane) {
    if (!m_device) return;

    DD::Image::Box imageBounds = plane.bounds();
    int W = imageBounds.w();
    int H = imageBounds.h();

    // 1. Only re-allocate and re-commit if dimensions changed
    bool dimsChanged = (W != m_filterW || H != m_filterH);
    m_filterW = W; 
    m_filterH = H;

    Iop* colorIn  = dynamic_cast<Iop*>(input(0));
    Iop* albedoIn = dynamic_cast<Iop*>(input(1));
    Iop* normalIn = dynamic_cast<Iop*>(input(2));

    if (!colorIn) return;

    bool hasAlbedo = (albedoIn != nullptr);
    bool hasNormal = (normalIn != nullptr);

    if (m_settingsDirty) {
        m_filter = nullptr;
        setupDevice();
    }

    if (dimsChanged || m_settingsDirty) {
        if (dimsChanged)
        {
            auto bufferSize = m_filterW * m_filterH * 3 * sizeof(float);

            m_colorBuffer = m_device.newBuffer(bufferSize);
            m_outputBuffer = m_device.newBuffer(bufferSize);
                
            m_albedoBuffer = hasAlbedo ? m_device.newBuffer(bufferSize) : nullptr;
            m_normalBuffer = hasNormal ? m_device.newBuffer(bufferSize) : nullptr;
                
            m_allocatedSize = bufferSize;
        }
        
        setupFilter(hasAlbedo, hasNormal);
        m_settingsDirty = false;
    }

    float* targetBuffers[3] = {
        m_colorBuffer ? static_cast<float*>(m_colorBuffer.getData()) : nullptr,
        m_albedoBuffer ? static_cast<float*>(m_albedoBuffer.getData()) : nullptr,
        m_normalBuffer ? static_cast<float*>(m_normalBuffer.getData()) : nullptr
    };


    for (int i = 0; i < node_inputs(); i++) {
        Iop* curIn = dynamic_cast<Iop*>(input(i));
        if (!curIn) continue;

        curIn->request(imageBounds, m_defaultChannels, 0);
        DD::Image::ImagePlane inputPlane(imageBounds, false, m_defaultChannels, m_defaultNumberOfChannels);
        curIn->fetchPlane(inputPlane);

        float* targetBuf = targetBuffers[i];

        if (!targetBuf) continue;
        
        #pragma omp parallel for schedule(static)
        for (int y = imageBounds.y(); y < imageBounds.t(); y++) {
            for (int x = imageBounds.x(); x < imageBounds.r(); x++) {
                float localX = x - imageBounds.x();
                float localY = y - imageBounds.y();

                size_t pixelOffset = (size_t(localY) * m_filterW + localX) * m_defaultNumberOfChannels;

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

    int numChans = std::min((int)activeChans.size(), m_defaultNumberOfChannels);

    #pragma omp parallel for
    for(int z = 0; z < numChans; z++) {
        Channel c = activeChans[z];
        int pIdx = plane.chanNo(c);
        if (pIdx < 0) continue;

        for (int y = imageBounds.y(); y < imageBounds.t(); y++) {
            for (int x = imageBounds.x(); x < imageBounds.r(); x++) {
                float localX = x - imageBounds.x();
                float localY = y - imageBounds.y();

                size_t index = (size_t(localY) * m_filterW + localX) * m_defaultNumberOfChannels + z;
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