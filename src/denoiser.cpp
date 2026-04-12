#include "denoiser.h"

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
    Box box = plane.bounds();
    int width = box.w();
    int height = box.h();

    Iop *colorIn = dynamic_cast<Iop *>(input(0));
    if (!colorIn || width <= 0 || height <= 0)
        return;

    ImagePlane inputPlane(box, false, Mask_RGB, 3);
    colorIn->fetchPlane(inputPlane);

    if (m_engine == 0)
    {
       m_oidn.render(plane, inputPlane, box);
    }
    #if USE_OPTIX
    else if (m_engine == 1)
    {
        m_optix.render(plane, inputPlane, box);
    }
    #endif
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