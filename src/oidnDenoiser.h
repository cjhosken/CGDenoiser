#ifndef OIDNDENOISER_H
#define OIDNDENOISER_H


#include <OpenImageDenoise/oidn.hpp>
#include "denoiserData.h"

static const char* const kOIDNDevices[] = {
    "Default", 
#if OIDN_CPU
    "CPU", 
#endif
#if OIDN_CUDA
    "CUDA (NVIDIA)",
#endif   
#if OIDN_HIP
    "HIP (AMD)",
#endif
#if OIDN_METAL
    "Metal (Apple)", 
#endif
#if OIDN_SYCL
    "SYCL (Intel)", 
#endif
    nullptr
};

static const char* const kOIDNFilters[] = { "RT", "RTLightmap", nullptr };
static const char* const kOIDNQualities[] = { "Fast", "Balanced", "High", nullptr };
static const char* const kOIDNModes[] = { "None", "sRGB", "HDR", nullptr };

class OIDNDenoiser {
    public:
        OIDNDenoiser() = default;
        ~OIDNDenoiser() = default;

        OIDNDenoiser(const OIDNDenoiser&) = delete;
        OIDNDenoiser& operator=(const OIDNDenoiser&) = delete;

        OIDNDenoiser(OIDNDenoiser&&) noexcept = default;
        OIDNDenoiser& operator=(OIDNDenoiser&&) noexcept = default;

        void run(DenoiserData& data, bool deviceDirty, bool filterDirty);

        void setupDevice();
        void setupFilter();

        int device_types = 0; // maps to kOIDNDevices
        int filter_type = 0; // RT / RTLightmap

        float filter_inputScale = 0.0f;
        bool filter_cleanAux = false;

        int filter_quality = 1; // Fast / Balanced / High
        int filter_mode = 2; // None / sRGB / HDR

        bool filter_directional = false; // RTLightmap only

    private:

        void rebuildDevice();
        void rebuildFilter();

        void allocateBuffers(size_t colorBytes);

        oidn::DeviceRef m_device;
        oidn::FilterRef m_filter;

        oidn::BufferRef m_colorBuffer;
        oidn::BufferRef m_outputBuffer;
        oidn::BufferRef m_albedoBuffer;
        oidn::BufferRef m_normalBuffer;

        int m_width = 0;
        int m_height = 0;

        bool m_hasAlbedo = false;
        bool m_hasNormal = false;

        int m_lastDeviceType = -1;
        int m_lastFilterType = -1;

        bool m_filterDirty = true;
        bool m_deviceDirty = true;
};

#endif // OIDNDENOISER_H
