#ifndef OPTIXDENOISER_H
#define OPTIXDENOISER_H

#include <iostream>
#include "DDImage/PlanarIop.h"

#include <optix.h>
#include <optix_stubs.h>


using namespace DD::Image;


class OptiXDenoiser {
    public:
        OptiXDenoiser();
        ~OptiXDenoiser();

        void render(ImagePlane &plane, ImagePlane &inputPlane, Box box);

    private:
        void setupDevice();
        void setupDenoiser(int w, int h);
        void cleanup();


        int m_width;
        int m_height;

        int m_defaultNumChannels;
        int m_deviceDirty;
        int m_filterDirty;

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

        
}; // OPTIXDENOISER_H

#endif