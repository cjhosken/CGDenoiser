#include "denoiser.h"
#include "DDImage/Black.h"
#include "DDImage/Knobs.h"


void CGDenoiser::renderStripe(DD::Image::ImagePlane& outputPlane)
{
    if (aborted() || cancelled()) return;

    // Check connections
    m_albedo_connected = !(dynamic_cast<DD::Image::Black*>(input(1)));
    m_normal_connected = !(dynamic_cast<DD::Image::Black*>(input(2)));

    DD::Image::Format imageFormat = input0().format();
    m_width = imageFormat.width();
    m_height = imageFormat.height();
    DD::Image::Box imageBounds = input0().format();


    auto bufferSize = m_width * m_height * 3 * sizeof(float);

    m_denoiserData.allocate(m_width, m_height, m_albedo_connected, m_normal_connected);
    
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

        #pragma omp parallel for
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

    if (aborted() || cancelled()) return;

    if (m_engine == 0) {
        m_oidn->run(m_denoiserData, m_deviceDirty, m_filterDirty);
    }

    #if OPTIX
    else {
        m_optix->run(m_denoiserData, m_deviceDirty, m_filterDirty);
    }
    #endif

    // Run OIDN denoising
    // Write denoised output back
    outputPlane.writable();
    const float* outputData = m_denoiserData.getOutput();

    const DD::Image::Channel channels[] = {
        DD::Image::Channel::Chan_Red,
        DD::Image::Channel::Chan_Green,
        DD::Image::Channel::Chan_Blue
    };
    
    #pragma omp parallel for
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

    Enumeration_knob(f, &m_engine, Engine, "Engine");
    Tooltip(f, "The hardward backend used for denoising");

    Divider(f);

    BeginGroup(f, "oidn_group", "OIDN Settings");

    // OIDN
    Enumeration_knob(f, &m_oidn->device_type, OIDN_Device, "Device");
    Tooltip(f, "The hardward backend used for denoising");

    Enumeration_knob(f, &m_oidn->filter_type, OIDN_Filter, "Filter");
    Tooltip(f, "Filter method.");

    Enumeration_knob(f, &m_oidn->filter_quality, OIDN_Quality, "Quality");
    Tooltip(f, "Quality");

    Enumeration_knob(f, &m_oidn->filter_mode, OIDN_Mode, "Mode");
    Tooltip(f, "Use sRGB if your data is already gamma encoded.");

    Float_knob(f, &m_oidn->filter_inputScale, "filter_inputScale", "Input Scale");
    Tooltip(f, "Manuall scale the input values (usually 1.0).");

    Bool_knob(f, &m_oidn->filter_directional, "filter_directional", "Directional");
    Tooltip(f, "Only used for RTLightmap filter.");

    EndGroup(f);
    
    // OptiX
    #if OPTIX

    BeginGroup(f, "optix_group", "OptiX Settings");

    Enumeration_knob(f, &m_optix->model, OptiX_MODEL, "Model");
    Tooltip(f, "model type");

    Float_knob(f, &m_optix->blend, "filter_blend", "Blend");
    Tooltip(f, "Blend. 1.0 is denoised.");

    EndGroup(f);

    #endif
}

int CGDenoiser::knob_changed(DD::Image::Knob* k) { 
    // Device changes
    if (k->is("Engine") || k->is("Device")) { // Ensure this matches the internal name in knobs()
        m_deviceDirty = true;
        m_filterDirty = true;

        bool useOIDN = (m_engine == 0);

        if (knob("oidn_group"))
            knob("oidn_group")->visible(useOIDN);


        if (knob("optix_group"))
            knob("optix_group")->visible(!useOIDN);
        
        return 1;
    }    

    // Filter/Workflow changes
    if (k->is("Filter") || k->is("Mode") || k->is("Quality") || 
        k->is("filter_inputScale") || k->is("filter_directional")) {
        
        m_filterDirty = true;
        
        // Use the actual value from your OIDN object to determine state
        bool isRT = (m_oidn->filter_type == 0);
        bool isLightmap = (m_oidn->filter_type == 1);

        // Workflow (HDR/sRGB) should only be visible for standard RT
        if (knob("Mode")) {
            knob("Mode")->visible(isRT);
        }

        // Directional should only be visible for Lightmap
        if (knob("filter_directional")) {
            knob("filter_directional")->visible(isLightmap); 
        }

        return 1;
    }
    
    return 0;
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

const DD::Image::Iop::Description CGDenoiser::description("CGDenoiser", nullptr, build);