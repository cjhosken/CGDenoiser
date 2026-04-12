#ifndef CGDENOISER_H
#define CGDENOISER_H

#include "DDImage/PlanarIop.h"
#include "DDImage/Knobs.h"

#include "oidnDenoiser.h"

#if USE_OPTIX
#include "optixDenoiser.h"
#endif

using namespace DD::Image;

class CGDenoiser : public PlanarIop
{

public:
    // Constructor
    CGDenoiser(Node *node);
    ~CGDenoiser() override;
    void _validate(bool for_real) override;
    void renderStripe(ImagePlane &plane) override;

    void knobs(DD::Image::Knob_Callback) override;
    int knob_changed(Knob *k) override;
    const char *input_label(int n, char *) const override;

    static const Iop::Description desc;
    const char *Class() const override { return desc.name; }
    const char *node_help() const override { return "AI Denoiser using OIDN or OptiX"; }

private:
    // State tracking
    int m_filterW, m_filterH;
    bool m_deviceDirty;
    bool m_filterDirty;

    // --- Knob Variables ---
    int m_engine; // 0 = OIDN, 1 = OptiX

    OIDNDenoiser m_oidn;

#if USE_OPTIX
    OptiXDenoiser m_optix;
#endif
};

#endif // CGDENOISER_H