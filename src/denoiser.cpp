#include "denoiser.h"
#include <mutex>

CGDenoiser::CGDenoiser(Node *node) : PlanarIop(node)
{
    inputs(1);

    m_engine = 0;

    m_oidn = OIDNDenoiser();

    #if USE_OPTIX
        m_optix = OptiXDenoiser();
    #endif
}

CGDenoiser::~CGDenoiser() {}


void CGDenoiser::renderStripe(ImagePlane &plane) {
    if (!m_cached)
        std::cout << "[CGDenoiser] Rendering (compute phase)..." << std::endl;

    Box box = plane.bounds();

    Iop *colorIn = dynamic_cast<Iop *>(input(0));
    if (!colorIn)
        return;

    Box full = colorIn->info().box();
    int fullW = full.w();
    int fullH = full.h();

    if (fullW <= 0 || fullH <= 0) return;

    if (!m_cached || m_dirty || fullW != m_fullW || fullH != m_fullH)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_cached || m_dirty || fullW != m_fullW || fullH != m_fullH) {
            std::cout << "[CGDenoiser] Computing full-frame denoise..." << std::endl;

            m_fullW = fullW;
            m_fullH = fullH;

            // Fetch FULL image
            ImagePlane fullInput(full, false, Mask_RGB, 3);
            colorIn->fetchPlane(fullInput);

            m_cachedOutput.resize((size_t)m_fullW * m_fullH * 3);

            // Copy into contiguous buffer
            for (int y = full.y(); y < full.t(); y++)
            {
                for (int x = full.x(); x < full.r(); x++)
                {
                    size_t idx = ((y - full.y()) * m_fullW + (x - full.x())) * 3;

                    m_cachedOutput[idx + 0] = fullInput.at(x, y, 0);
                    m_cachedOutput[idx + 1] = fullInput.at(x, y, 1);
                    m_cachedOutput[idx + 2] = fullInput.at(x, y, 2);
                }
            }

            if (m_engine == 0)
            {
                m_oidn.run(m_cachedOutput.data(), m_fullW, m_fullH);
            }
#if USE_OPTIX
            else if (m_engine == 1)
            {
                m_optix.run(m_cachedOutput.data(), m_fullW, m_fullH);
            }
#endif

            m_cached = true;
            m_dirty = false;
        }
    }

    // --- SERVE STRIPE FROM CACHE ---
    plane.writable();

    int r = plane.chanNo(DD::Image::Chan_Red);
    int g = plane.chanNo(DD::Image::Chan_Green);
    int b = plane.chanNo(DD::Image::Chan_Blue);

    for (int y = box.y(); y < box.t(); y++)
    {
        for (int x = box.x(); x < box.r(); x++)
        {
            size_t idx = ((y - full.y()) * m_fullW + (x - full.x())) * 3;

            if (r >= 0) plane.writableAt(x, y, r) = m_cachedOutput[idx + 0];
            if (g >= 0) plane.writableAt(x, y, g) = m_cachedOutput[idx + 1];
            if (b >= 0) plane.writableAt(x, y, b) = m_cachedOutput[idx + 2];
        }
    }
}

void CGDenoiser::knobs(Knob_Callback f)
{
    const char *engine_names[] = {
        "OpenImageDenoise (OIDN)",
#if USE_OPTIX
        "NVIDIA OptiX",
#endif
        nullptr};
    Enumeration_knob(f, &m_engine, engine_names, "engine", "Engine");
    Tooltip(f, "Select the denoising backend.");
    Divider(f);

    // --- Main OIDN Settings
    const char *filter_names[] = {"Ray Tracing (RT)", "RT Lightmap", nullptr};
    Enumeration_knob(f, &m_oidn.filter_type, filter_names, "filter_type", "Filter Type");

    const char *quality_names[] = {"Fast", "Balanced", "High", nullptr};
    Enumeration_knob(f, &m_oidn.filter_quality, quality_names, "quality", "Quality");
    Tooltip(f, "Fast: lowest quality. Balanced: medium. High: best quality, slowest.");

    Bool_knob(f, &m_oidn.filter_hdr, "hdr", "HDR");
    Tooltip(f, "The main input image is HDR");

    Bool_knob(f, &m_oidn.filter_srgb, "srgb", "sRGB");
    Tooltip(f, "The main input image is encoded with the sRGB (or 2.2 gamma) curve (LDR only) or is linear; the output will be encoded with the same curve");

    Float_knob(f, &m_oidn.filter_inputScale, "input_scale", "Input Scale");
    Tooltip(f, "Scales values in the main input image before filtering, without scaling the output too, which can be used to map color or auxiliary feature values to the expected range, e.g. for mapping HDR values to physical units (which affects the quality of the output but not the range of the output values); if set to NaN, the scale is computed implicitly for HDR images or set to 1 otherwise");

    Bool_knob(f, &m_oidn.filter_cleanAux, "clean_aux", "Clean Aux (Albedo/Normal)");
    Tooltip(f, "The auxiliary feature (albedo, normal) images are noise-free; recommended for highest quality but should not be enabled for noisy auxiliary images to avoid residual noise");

    Divider(f);

    Bool_knob(f, &m_oidn.filter_directional, "directional", "Directional (Lightmap Only)");
    Tooltip(f, "Only used for RTLightmap filter to handle directional components.");
}

int CGDenoiser::knob_changed(Knob *k)
{
    m_filterDirty = true;

    if (k->is("engine"))
    {
        m_deviceDirty = true;
    }

    std::cout << "[CGDenoiser] Knob Changed" << std::endl;

    return 1;
}

const char *CGDenoiser::input_label(int n, char *) const
{
    switch (n)
    {
    case 0:
        return "color";
    default: return "color";
    }
}

void CGDenoiser::_validate(bool for_real)
{
    copy_info();
}

static Iop *build(Node *node) { return new CGDenoiser(node); }
const Iop::Description CGDenoiser::desc("CGDenoiser", build);