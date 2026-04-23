#ifndef CGDENOISER_H
#define CGDENOISER_H

#include <memory>
#include <DDImage/PlanarIop.h>

#include "denoiserData.h"

#if OIDN
    #include "oidnDenoiser.h"
#endif

#if OPTIX
    #include "optixDenoiser.h"
#endif

static const char* const kEngineLabels[] = {
#if OIDN
    "OpenImageDenoiser (OIDN)",
#endif
#if OPTIX
    "OptiX",
#endif    
    nullptr
};

class CGDenoiser final: public DD::Image::PlanarIop
{
public:
    explicit CGDenoiser(Node* node);
    ~CGDenoiser() override = default;

    const char* input_label(int, char*) const override;
    void _validate(bool) override;
    void getRequests(const DD::Image::Box& box,
                     const DD::Image::ChannelSet& channels,
                     int count,
                     DD::Image::RequestOutput& reqData) const override;

    void renderStripe(DD::Image::ImagePlane& outputPlane) override;

    void knobs(DD::Image::Knob_Callback) override;
    int knob_changed(DD::Image::Knob* k) override;

    bool useStripes() const override;
    bool renderFullPlanes() const override;

    const char* Class() const override { return "CGDenoiser"; }
    const char* node_help() const override { return "OIDN / OptiX denoiser for CG Passes."; }

    static const Iop::Description description;

private:
    int m_engine = 0;

    unsigned int m_width = 0;
    unsigned int m_height = 0;

    bool m_albedoConnected = false;
    bool m_normalConnected = false;
    bool m_motionConnected = false;

    bool m_deviceDirty = false;
    bool m_filterDirty = false;

    DenoiserData m_denoiserData;
    
#if OIDN
    std::unique_ptr<OIDNDenoiser> m_oidn;
#endif

#if OPTIX
    std::unique_ptr<OptiXDenoiser> m_optix;
#endif
};

#endif // CGDENOISER_H