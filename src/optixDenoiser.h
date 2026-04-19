#ifndef OPTIXDENOISER_H
#define OPTIXDENOISER_H

#include <iostream>
#include "DDImage/PlanarIop.h"

#include "denoiserData.h"

#include <optix.h>
#include <optix_stubs.h>

using namespace DD::Image;

static const char* const OptiX_MODEL[] = {"HDR", "AOV", "Temporal", 0};

class OptiXDenoiser {
    public:
        OptiXDenoiser();
        ~OptiXDenoiser();

        void run(DenoiserData& data, bool deviceDirty, bool filterDirty);

        int model; // 0 = HDR; 1 = LDR; 2 = TEMPORAL
        float blend;

    private:
        void setupDevice();
        void setupDenoiser(int w, int h, bool dirty);
        void cleanup();

        int m_width;
        int m_height;

        OptixDeviceContext m_context = nullptr;
        OptixDenoiser m_denoiser = nullptr;
        CUstream m_stream = 0;
        
        CUcontext m_cuCtx = nullptr;

        CUdeviceptr m_dState = 0;
        CUdeviceptr m_dScratch = 0;
        CUdeviceptr m_dIntensity = 0;

        int m_stateSize;
        int m_scratchSize;

        bool m_initialized = false;

        // Temporal
        CUdeviceptr m_prevOutput = 0;
        bool m_hasPrev = false;
        
}; // OPTIXDENOISER_H

#endif