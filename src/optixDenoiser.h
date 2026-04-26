#ifndef OPTIXDENOISER_H
#define OPTIXDENOISER_H

#include <iostream>

#include "denoiserData.h"

#ifdef _WIN32
    #define _WIN32_WINNT 0x0A00
    #define WINVER 0x0A00

    #include <windows.h>
    #include <cfgmgr32.h>
#endif

#include <optix.h>
#include <optix_stubs.h>

static const char* const OptiX_MODEL[] = {
    "LDR", "HDR", "AOV", "Temporal", "Temporal AOV", 
    // "Upscale (2x)", "Temporal Upscale (2x)", 
    0};

class OptiXDenoiser {
    public:
        OptiXDenoiser();
        ~OptiXDenoiser();

        void run(DenoiserData& data, bool deviceDirty, bool filterDirty);

        int model; // 0 = HDR; 1 = LDR; 2 = TEMPORAL
        float blend;

    private:
        void setupDevice();
        void setupDenoiser(int w, int h);
        void cleanup();

        int m_width;
        int m_height;

        OptixDeviceContext m_context = nullptr;
        OptixDenoiser m_denoiser = nullptr;
        CUstream m_stream = 0;
        
        CUcontext m_cuCtx = nullptr;

        CUdevice m_device = 0;

        CUdeviceptr m_dColor = 0;
        CUdeviceptr m_dOutput = 0;
        CUdeviceptr m_dAlbedo = 0;
        CUdeviceptr m_dNormal = 0;
        CUdeviceptr m_dMotion = 0;
        CUdeviceptr m_prevOutput = 0;

        CUdeviceptr m_dState = 0;
        CUdeviceptr m_dScratch = 0;
        CUdeviceptr m_dIntensity = 0;

        int m_stateSize;
        int m_scratchSize;

        bool m_deviceDirty;
        bool m_denoiserDirty;

        bool m_initialized = false;
        bool m_hasPrev = false;

}; // OPTIXDENOISER_H

#endif