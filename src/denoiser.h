#ifndef CGDENOISER_H
#define CGDENOISER_H

#include "oidnDenoiser.h"
#include "denoiserData.h"

class CGDenoiser: public DD::Image::PlanarIop
{
    unsigned int m_width;
    unsigned int m_height;

    bool m_albedo_connected = false;
    bool m_normal_connected = false;

    bool m_deviceDirty = false;
    bool m_filterDirty = false;

    OIDNDenoiser* m_oidn;
    DenoiserData m_denoiserData;

    public:
        CGDenoiser(Node* node): PlanarIop(node)
        {
            inputs(3);

            m_width = 0;
            m_height = 0;

            m_oidn = new OIDNDenoiser();
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