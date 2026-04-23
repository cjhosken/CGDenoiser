#include "denoiser.h"
#include <DDImage/Black.h>
#include <DDImage/Knobs.h>


void CGDenoiser::renderStripe(DD::Image::ImagePlane& outputPlane)
{
    if (aborted() || cancelled()) return;
    
    // Check connections
    m_albedo_connected = !(dynamic_cast<DD::Image::Black*>(input(1)));
    m_normal_connected = !(dynamic_cast<DD::Image::Black*>(input(2)));
    m_motion_connected = !(dynamic_cast<DD::Image::Black*>(input(3)));

    DD::Image::Format imageFormat = input0().format();
    m_width = imageFormat.width();
    m_height = imageFormat.height();
    DD::Image::Box imageBounds = input0().format();

    auto bufferSize = m_width * m_height * 3 * sizeof(float);

    m_denoiserData.allocate(m_width, m_height, m_albedo_connected, m_normal_connected, m_motion_connected);
    
    auto fetchAndCopy = [&](int inputIdx, float* dstBuffer, DD::Image::ChannelSet mask, int numChans) {
        if (dstBuffer == nullptr) return;

        DD::Image::Iop* inputNode = dynamic_cast<DD::Image::Iop*>(input(inputIdx));
        if (!inputNode || !inputNode->tryValidate(true)) return;

        inputNode->request(imageBounds, mask, 0);
        DD::Image::ImagePlane plane(imageBounds, false, mask, numChans);
        inputNode->fetchPlane(plane);

        const float* srcData = static_cast<const float*>(plane.readable());
        if (!srcData) return;

        auto chanStride = plane.chanStride();

        for (int c = 0; c < numChans; c++) {
            const float* srcChan = &srcData[chanStride * c];
            float* dstPtr = dstBuffer + c;
            
            for (int y = 0; y < m_height; y++) {
                // Vertical flip: Nuke (0,0 is bottom-left) to OIDN (0,0 is top-left)
                int dstRowStart = ((m_height - y - 1) * m_width) * numChans;
                for (int x = 0; x < m_width; x++) {
                    *(dstPtr + dstRowStart + x * numChans) = srcChan[y * m_width + x];
                }
            }
        }
    };

    fetchAndCopy(0, m_denoiserData.getColor(), DD::Image::Mask_RGB, 3);

    if (m_albedo_connected) {
        fetchAndCopy(1, m_denoiserData.getAlbedo(), DD::Image::Mask_RGB, 3);
    }

    if (m_normal_connected) {
        fetchAndCopy(2, m_denoiserData.getNormal(), DD::Image::Mask_RGB, 3);
    }

    if (m_motion_connected) {
        DD::Image::ChannelSet motionMask;
        motionMask += DD::Image::Chan_Red;
        motionMask += DD::Image::Chan_Green;

        fetchAndCopy(3, m_denoiserData.getMotion(), motionMask, 2);
    }

    if (aborted() || cancelled()) return;

    #if OIDN
    if (m_engine == 0) {
        m_oidn->run(m_denoiserData, m_deviceDirty, m_filterDirty);
    }
    #endif

    #if OPTIX

    int optix_engine_target = 1;

    #if !OIDN
        optix_engine_target = 0;
    #endif

    if (m_engine == optix_engine_target) {
        m_optix->run(m_denoiserData, m_deviceDirty, m_filterDirty);
    }
    #endif

    #if !OIDN && !OPTIX
    std::memcpy(
        m_denoiserData.getOutput(),
        m_denoiserData.getColor(),
        m_width * m_height * 3 * sizeof(float)
    );
    #endif

    m_deviceDirty = false;
    m_filterDirty = false;

    // Run OIDN denoising
    // Write denoised output back
    outputPlane.writable();
    const float* outputData = m_denoiserData.getOutput();

    const DD::Image::Channel channels[] = {
        DD::Image::Channel::Chan_Red,
        DD::Image::Channel::Chan_Green,
        DD::Image::Channel::Chan_Blue
    };
    
    for (int chanNo = 0; chanNo < 3; chanNo++)
    {
        int c = outputPlane.chanNo(channels[chanNo]);
        if (c < 0) continue;
        
        const float* srcPtr = outputData + chanNo;

        for (int j = 0; j < m_height; j++) {
            int srcRowStart = ((m_height - j - 1) * m_width) * 3;
            for (int i = 0; i < m_width; i++) {
                outputPlane.writableAt(i, j, c) = *(srcPtr + srcRowStart + i * 3);
            }
        }
    }
}

void CGDenoiser::knobs(DD::Image::Knob_Callback f) {

    Enumeration_knob(f, &m_engine, Engine, "engine", "Engine");
    Tooltip(f, "The technique used for denoising.");

    Divider(f);

    #if OIDN

    // OIDN
    Enumeration_knob(f, &m_oidn->device_type, OIDN_Device, "oidn_device", "Device");
    Tooltip(f, "The hardware backend used for OIDN denoising");

    Enumeration_knob(f, &m_oidn->filter_type, OIDN_Filter, "oidn_filter", "Filter");
    Tooltip(f, "The filter method used for OIDN denoising. ");

    Enumeration_knob(f, &m_oidn->filter_quality, OIDN_Quality, "oidn_quality", "Quality");
    Tooltip(f, "Image quality.");

    Enumeration_knob(f, &m_oidn->filter_mode, OIDN_Mode, "oidn_mode", "Mode");
    Tooltip(f, "The input image encoding. Use HDR unless the main input image is encoded with the sRGB (or 2.2 gamma) curve or is linear; in which case use sRGB; The output will be encoded with the same curve.");

    Bool_knob(f, &m_oidn->filter_cleanAux, "oidn_clean", "Clean Aux");
    Tooltip(f, "Denoise the auxilliary features (albedo, normal).");

    Float_knob(f, &m_oidn->filter_inputScale, "oidn_inputScale", "Input Scale");
    Tooltip(f, "Scales values in the main input image before filtering, without scaling the output too, which can be used to map color or auxiliary feature values to the expected range, e.g. for mapping HDR values to physical units (which affects the quality of the output but not the range of the output values); if set to NaN, the scale is computed implicitly for HDR images or set to 1 otherwise.");

    Bool_knob(f, &m_oidn->filter_directional, "oidn_directional", "Directional");
    Tooltip(f, "Whether the input contains normalized coefficients (in [-1, 1]) of a directional lightmap (e.g. normalized L1 or higher spherical harmonics band with the L0 band divided out); if the range of the coefficients is different from [-1, 1], the inputScale parameter can be used to adjust the range without changing the stored values.");
    
    #endif

    // OptiX
    #if OPTIX

    Enumeration_knob(f, &m_optix->model, OptiX_MODEL, "optix_model", "Model");
    Tooltip(f, "The method used for OptiX denoising. Temporal requires motionvectors.");

    Float_knob(f, &m_optix->blend, "optix_blend", "Blend");
    Tooltip(f, "Denoising amount. 1.0 is completely denoised.");

    #endif
}

int CGDenoiser::knob_changed(DD::Image::Knob* k) {
    if (!k) return 0;

    // --- Read CURRENT values from knobs (not member vars) ---
    int engine = m_engine;
    if (DD::Image::Knob* ek = knob("engine"))
        engine = int(ek->get_value());

    bool useOIDN = (engine == 0);

    // --- Dirty flags ---
    if (k->is("engine") || k->is("oidn_device"))
        m_deviceDirty = true;

    m_filterDirty = true;

    #if OIDN

    int filter = m_oidn->filter_type;
    if (DD::Image::Knob* fk = knob("oidn_filter"))
        filter = int(fk->get_value());

    bool isRT = (filter == 0);


    // --- OIDN knobs ---
    const char* oidn_knobs[] = {
        "oidn_device", "oidn_filter", "oidn_quality", 
        "oidn_mode", "oidn_inputScale", "oidn_directional", "oidn_clean"
    };

    for (const char* name : oidn_knobs)
    {
        if (DD::Image::Knob* kk = knob(name))
        {
            bool v = useOIDN;

            if (strcmp(name, "oidn_mode") == 0)
                v = (useOIDN && isRT);

            if (strcmp(name, "oidn_directional") == 0)
                v = (useOIDN && !isRT);

            kk->visible(v);
        }
    } 

    #endif

    // --- OptiX knobs ---
    #if OPTIX

    #if !OIDN
    useOIDN = false;
    #endif

    const char* optix_knobs[] = { "optix_model", "optix_blend" };

    for (const char* name : optix_knobs)
    {
        if (DD::Image::Knob* kk = knob(name))
            kk->visible(!useOIDN);
    }
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

void CGDenoiser::_validate(bool for_real) { copy_info(); }

void CGDenoiser::getRequests(const DD::Image::Box& box, const DD::Image::ChannelSet& channels, int count, DD::Image::RequestOutput &reqData) const
{
    int nInputs = (int)getInputs().size();
    for (int i = 0; i < nInputs; i++) {
        const DD::Image::ChannelSet channels = input(i)->info().channels();
        input(i)->request(channels, count);
    }
}

bool CGDenoiser::useStripes() const { return false; }

bool CGDenoiser::renderFullPlanes() const { return true; }

static DD::Image::Iop* build(Node* node) { return new CGDenoiser(node); }

const DD::Image::Iop::Description CGDenoiser::description("CGDenoiser", "CGDenoiser", build);