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

    // State tracking
    int m_filterW, m_filterH;
    bool m_deviceDirty;
    bool m_filterDirty;

    // --- Knob Variables ---
    int _engine; // 0 = OIDN, 1 = OptiX

    int m_defaultChannels;
 

public:
    // Constructor
    CGDenoiser(Node* node);
    ~CGDenoiser() override;
    void _validate(bool for_real) override;
    void renderStripe(ImagePlane& plane) override;

    void setupDevice();

    static const Iop::Description desc;
    const char* Class() const override {return desc.name;}
    const char* node_help() const override { return "AI Denoiser using OIDN or OptiX"; }
};

#endif // CGDENOISER_H