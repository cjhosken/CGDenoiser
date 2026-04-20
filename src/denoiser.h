#ifndef CGDENOISER_H
#define CGDENOISER_H

#include <DDImage/PlanarIop.h>
#include "denoiserData.h"

#include "oidnDenoiser.h"
#if OPTIX
#include "optixDenoiser.h"
#endif

static const char* const Engine[] = {
    "OpenImageDenoiser (OIDN)",
    
    #if OPTIX
    "OptiX",
    #endif
    
    0};

class CGDenoiser: public DD::Image::PlanarIop
{
    unsigned int m_width;
    unsigned int m_height;

    bool m_albedo_connected = false;
    bool m_normal_connected = false;
    bool m_motion_connected = false;

    bool m_deviceDirty = false;
    bool m_filterDirty = false;

    int m_engine = 0; // 0 = OIDN, 1 = OptiX

    std::unique_ptr<OIDNDenoiser> m_oidn;

    #if OPTIX
    std::unique_ptr<OptiXDenoiser> m_optix;
    #endif

    DenoiserData m_denoiserData;

    public:
        CGDenoiser(Node* node): PlanarIop(node)
        {
            inputs(4);

            m_width = 0;
            m_height = 0;

            if (!m_oidn)
                m_oidn = std::make_unique<OIDNDenoiser>();

            #if OPTIX
            if (!m_optix)
                m_optix = std::make_unique<OptiXDenoiser>();
            #endif
        }

        const char* input_label(int n, char*) const override;

        void _validate(bool) override;

        void getRequests(const DD::Image::Box& box, const DD::Image::ChannelSet& channels, int count, DD::Image::RequestOutput &reqData) const override;

        void renderStripe(DD::Image::ImagePlane& outputPlane) override;

        void knobs(DD::Image::Knob_Callback) override;
        int knob_changed(DD::Image::Knob* k) override;

        virtual bool useStripes() const override;
        virtual bool renderFullPlanes() const override;

        const char* Class() const override { return "CGDenoiser"; }
        const char* node_help() const override { return "CGDenoiser for Nuke."; }
        static const Iop::Description description;
};


#endif // CGDENOISER_H