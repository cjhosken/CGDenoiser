#include "denoiser.h"

#include <cstring>
#include <DDImage/Black.h>
#include <DDImage/Knobs.h>

namespace
{
    inline bool isConnected(DD::Image::Op* op)
    {
        return op && !dynamic_cast<DD::Image::Black*>(op);
    }

    inline void verticalFlipCopy(
        const float* src,
        float* dst,
        int width,
        int height,
        int channels,
        int chanStride)
    {
        for (int c = 0; c < channels; ++c)
        {
            const float* srcChan = src + chanStride * c;
            float* dstChan = dst + c;

            for (int y = 0; y < height; ++y)
            {
                const int dstRow = (height - 1 - y) * width * channels;
                const int srcRow = y * width;

                for (int x = 0; x < width; ++x)
                {
                    dstChan[dstRow + x * channels] = srcChan[srcRow + x];
                }
            }
        }
    }

    void fetchPlaneToBuffer(
        DD::Image::Iop* input,
        const DD::Image::Box& bounds,
        const DD::Image::ChannelSet& mask,
        int numChannels,
        float* dst,
        int width,
        int height)
    {
        if (!input || !dst || !input->tryValidate(true))
            return;

        input->request(bounds, mask, 0);

        DD::Image::ImagePlane plane(bounds, false, mask, numChannels);
        input->fetchPlane(plane);

        const float* src = static_cast<const float*>(plane.readable());
        if (!src)
            return;

        verticalFlipCopy(src, dst, width, height, numChannels, plane.chanStride());
    }
}

CGDenoiser::CGDenoiser(Node* node)
    : PlanarIop(node)
{
    inputs(4);

#if OIDN
    m_oidn = std::make_unique<OIDNDenoiser>();
#endif

#if OPTIX
    m_optix = std::make_unique<OptiXDenoiser>();
#endif
}

void CGDenoiser::renderStripe(DD::Image::ImagePlane& outputPlane)
{
    if (aborted() || cancelled()) 
        return;
    
    // Connections
    m_albedoConnected = isConnected(input(1));
    m_normalConnected = isConnected(input(2));
    m_motionConnected = isConnected(input(3));

    const auto format = input0().format();
    m_width = format.width();
    m_height = format.height();
    const DD::Image::Box bounds = format;
    
    m_denoiserData.allocate(
        m_width, 
        m_height, 
        m_albedoConnected, 
        m_normalConnected, 
        m_motionConnected);

    fetchPlaneToBuffer(&input0(), bounds, DD::Image::Mask_RGB, 3,
                        m_denoiserData.color(), m_width, m_height);

    if (m_albedoConnected)
    {
        fetchPlaneToBuffer(input(1), bounds, DD::Image::Mask_RGB, 3,
                        m_denoiserData.albedo(), m_width, m_height);
    }

    if (m_normalConnected)
    {
        fetchPlaneToBuffer(input(2), bounds, DD::Image::Mask_RGB, 3,
                        m_denoiserData.normal(), m_width, m_height);
    }

    if (m_motionConnected)
    {
        DD::Image::ChannelSet motionMask;
        motionMask += DD::Image::Chan_Red;
        motionMask += DD::Image::Chan_Green;

        fetchPlaneToBuffer(input(3), bounds, motionMask, 2,
                        m_denoiserData.motion(), m_width, m_height);
    }

    if (aborted() || cancelled())
        return;
    
#if OIDN
    if (m_engine == 0) 
    {
        m_oidn->run(m_denoiserData, m_deviceDirty, m_filterDirty);
    }
#endif

#if OPTIX
    if (m_engine == 1)
    {
        m_optix->run(m_denoiserData, m_deviceDirty, m_filterDirty);
    }
#endif

#if !OIDN && !OPTIX
    std::memcpy(
        m_denoiserData.output(),
        m_denoiserData.color(),
        m_width * m_height * 3 * sizeof(float)
    );
#endif

    m_deviceDirty = false;
    m_filterDirty = false;

    // Write output
    outputPlane.writable();
    const float* src = m_denoiserData.output();

    const DD::Image::Channel rgb[3] = {
        DD::Image::Channel::Chan_Red,
        DD::Image::Channel::Chan_Green,
        DD::Image::Channel::Chan_Blue
    };
    
    for (int c = 0; c < 3; ++c)
    {
        const int chan = outputPlane.chanNo(rgb[c]);
        if (chan < 0)
            continue;
        
        const float* srcPtr = src + c;

        for (int y = 0; y < m_height; ++y) 
        {
            const int srcRow = (m_height - 1 - y) * m_width * 3;
            for (int x = 0; x < m_width; ++x) 
            {
                outputPlane.writableAt(x, y, c) = 
                    srcPtr[srcRow + x * 3];
            }
        }
    }
}

void CGDenoiser::knobs(DD::Image::Knob_Callback f) 
{

    Enumeration_knob(f, reinterpret_cast<int*>(&m_engine), kEngineLabels, "engine", "Engine");
    Tooltip(f, "The technique used for denoising.");

#if OIDN
    Divider(f);

    Enumeration_knob(f, &m_oidn->device_types, kOIDNDevices, "oidn_device", "Device");
    Tooltip(f, "The hardware backend used for OIDN denoising");

    Enumeration_knob(f, &m_oidn->filter_type, kOIDNFilters, "oidn_filter", "Filter");
    Tooltip(f, "The filter method used for OIDN denoising. ");

    Enumeration_knob(f, &m_oidn->filter_quality, kOIDNQualities, "oidn_quality", "Quality");
    Tooltip(f, "Image quality.");

    Enumeration_knob(f, &m_oidn->filter_mode, kOIDNModes, "oidn_mode", "Mode");
    Tooltip(f, "The input image encoding. Use HDR unless the main input image is encoded with the sRGB (or 2.2 gamma) curve or is linear; in which case use sRGB; The output will be encoded with the same curve.");

    Bool_knob(f, &m_oidn->filter_cleanAux, "oidn_clean", "Clean Aux");
    Tooltip(f, "Denoise the auxilliary features (albedo, normal).");

    Float_knob(f, &m_oidn->filter_inputScale, "oidn_inputScale", "Input Scale");
    Tooltip(f, "Scales values in the main input image before filtering, without scaling the output too, which can be used to map color or auxiliary feature values to the expected range, e.g. for mapping HDR values to physical units (which affects the quality of the output but not the range of the output values); if set to NaN, the scale is computed implicitly for HDR images or set to 1 otherwise.");

    Bool_knob(f, &m_oidn->filter_directional, "oidn_directional", "Directional");
    Tooltip(f, "Whether the input contains normalized coefficients (in [-1, 1]) of a directional lightmap (e.g. normalized L1 or higher spherical harmonics band with the L0 band divided out); if the range of the coefficients is different from [-1, 1], the inputScale parameter can be used to adjust the range without changing the stored values.");
#endif

#if OPTIX
    Divider(f);

    Enumeration_knob(f, &m_optix->model, OptiX_MODEL, "optix_model", "Model");
    Tooltip(f, "The method used for OptiX denoising. Temporal requires motionvectors.");

    Float_knob(f, &m_optix->blend, "optix_blend", "Blend");
    Tooltip(f, "Denoising amount. 1.0 is completely denoised.");
#endif
}

int CGDenoiser::knob_changed(DD::Image::Knob* k) {
    if (!k) 
        return 0;

    if (k->is("engine") || k->is("oidn_device"))
        m_deviceDirty = true;

    m_filterDirty = true;

#if OIDN
    const bool useOIDN = (m_engine == 0);

    int filter = m_oidn->filter_type;
    if (auto fk = knob("oidn_filter"))
        filter = int(fk->get_value());

    const bool isRT = (filter == 0);

    auto setVisible = [&](const char* name, bool v)
    {
        if (auto* kk = knob(name))
            kk->visible(v);
    };

    setVisible("oidn_device", useOIDN);
    setVisible("oidn_filter", useOIDN);
    setVisible("oidn_quality", useOIDN);
    setVisible("oidn_clean", useOIDN);
    setVisible("oidn_inputScale", useOIDN);

    setVisible("oidn_mode", useOIDN && isRT);
    setVisible("oidn_directional", useOIDN && !isRT);
#endif

#if OPTIX
    const bool useOptix = (m_engine == 1);

    if (auto* k1 = knob("optix_model")) k1->visible(useOptix);
    if (auto* k2 = knob("optix_blend")) k2->visible(useOptix);
#endif

    return 1;
}

const char* CGDenoiser::input_label(int n, char*) const
{
    switch (n)
    {
        case 1: return "albedo";
        case 2: return "normal";
        case 3: return "motion";
        default: return "color";
    }
}

void CGDenoiser::_validate(bool) { copy_info(); }

void CGDenoiser::getRequests(
    const DD::Image::Box& box, 
    const DD::Image::ChannelSet& channels, 
    int count, 
    DD::Image::RequestOutput &reqData) const
{
    for (int i = 0; i < int(getInputs().size()); ++i)
    {
        auto* in = input(i);
        if (!in) continue;

        in->request(in->info().channels(), count);
    }
}

bool CGDenoiser::useStripes() const { return false; }

bool CGDenoiser::renderFullPlanes() const { return true; }

static DD::Image::Iop* build(Node* node) { return new CGDenoiser(node); }

const DD::Image::Iop::Description CGDenoiser::description("CGDenoiser", "CGDenoiser", build);