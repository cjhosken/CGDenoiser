#ifndef OIDNDENOISER_H
#define OIDNDENOISER_H

#include <iostream>
#include "DDImage/PlanarIop.h"
#include <OpenImageDenoise/oidn.hpp>

using namespace DD::Image;

static const char* const OIDN_Filter[] = {"RT", "RTLightmap", 0};
static const char* const OIDN_Quality[] = {"default", "fast", "balanced", "high", 0};

class OIDNDenoiser {
    public:
        OIDNDenoiser();
        ~OIDNDenoiser();

        void setupDevice();
        void setupFilter();

        void render(ImagePlane &plane, ImagePlane &inputPlane, Box box);

        int filter_type; // 0 = RT, 1 = RTLightmap

        bool filter_hdr;
        bool filter_srgb;
        float filter_inputScale;
        bool filter_cleanAux;
        int filter_quality; // 0 = Default, 1 = Fast, 2 = Balanced, 3 = High

        bool filter_directional; // Only for RTLightmap

    private:
        oidn::DeviceRef m_device;
        oidn::FilterRef m_filter;

        oidn::BufferRef m_colorBuffer;
        oidn::BufferRef m_outputBuffer;
        oidn::BufferRef m_albedoBuffer;
        oidn::BufferRef m_normalBuffer;

        int m_width;
        int m_height;

        int m_defaultNumChannels;
        int m_deviceDirty;
        int m_filterDirty;
        
        
}; // OIDNDENOISER_H

#endif