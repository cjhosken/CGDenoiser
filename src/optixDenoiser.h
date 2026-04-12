#ifndef OPTIXDENOISER_H
#define OPTIXDENOISER_H

#include <iostream>
#include "DDImage/PlanarIop.h"

#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <optix_function_table_definition.h>


using namespace DD::Image;


class OptiXDenoiser {
    public:
        OptiXDenoiser();
        ~OptiXDenoiser();

        void render(ImagePlane &plane, ImagePlane &inputPlane, Box box);

    private:
        int m_width;
        int m_height;

        int m_defaultNumChannels;
        int m_deviceDirty;
        int m_filterDirty;

        CUcontext cuCtx = 0;
        CUstream m_stream = 0;
        OptixDeviceContext m_optixContext = nullptr;
        OptixDenoiser m_denoiser = nullptr;
        
}; // OPTIXDENOISER_H

#endif