#ifndef CGDENOISER_H
#define CGDENOISER_H

#include "DDImage/PlanarIop.h"
#include "DDImage/Knobs.h"
#include <OpenImageDenoise/oidn.hpp>

#ifdef USE_OPTIX
#include <optix.h>
#include <cuda_runtime.h>
#endif

using namespace DD::Image;

class CGDenoiser : public PlanarIop {
    // --- OIDN Member Variables ---
    oidn::DeviceRef m_device;
    oidn::FilterRef m_filter;

    // Pointers for OIDN buffers
    oidn::BufferRef m_colorBuffer;
    oidn::BufferRef m_outputBuffer;
    oidn::BufferRef m_albedoBuffer;
    oidn::BufferRef m_normalBuffer;

    // --- Knob Variables ---
    int _engine; // 0 = OIDN, 1 = OptiX

    // --- Internal State ---
    unsigned int m_filterW, m_filterH;
    ChannelSet m_defaultChannels;
    int m_defaultNumberOfChannels;

    size_t m_allocatedSize;

    // --- Layer Passes ---
    ChannelSet m_albedoLayer;
    ChannelSet m_normalLayer;

public:
    // Constructor
    CGDenoiser(Node* node);

    // Destructor
    virtual ~CGDenoiser() {};

    bool useStripes() const override { return false; };
    bool renderFullPlanes() const override { return true; };

    int minimum_inputs() const override { return 1; }
    int maximum_inputs() const override { return 3; }
    const char* input_label(int input, char* buffer) const override {
    switch (input) {
        case 0: return "color";
        case 1: return "albedo";
        case 2: return "normal";
        default: return nullptr;
    }
}

    // --- Nuke Iop Virtual Functions ---
    void _validate(bool for_real) override;
    void knobs(Knob_Callback f) override;
    virtual void renderStripe(ImagePlane& plane) override;
    const char* Class() const override {return desc.name;}
    const char* node_help() const override { return "AI Denoiser using OIDN or OptiX"; }

    static const Iop::Description desc;

    // --- Internal Logic ---
    void setupDevice();
    void setupFilter(bool hasAlbedo, bool hasNormal);

    void runOIDN();
#ifdef USE_OPTIX
    void runOptiX();
#endif
};

#endif // CGDENOISER_H