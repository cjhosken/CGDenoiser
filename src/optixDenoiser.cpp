#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <optix_function_table_definition.h> // Keep it ONLY here
#include "optixDenoiser.h"


OptiXDenoiser::OptiXDenoiser() {}

OptiXDenoiser::~OptiXDenoiser() {cleanup();}

void OptiXDenoiser::setupDevice() {
    std::cout << "[OptiX] Setting up Device..." << std::endl;

    if (m_initialized) return;

    std::cout << "[OptiX] Making Context..." << std::endl;

    cudaFree(0);
    cuCtxGetCurrent(&m_cuCtx);
    if (!m_cuCtx) {
        cuDevicePrimaryCtxRetain(&m_cuCtx, 0);
        cuCtxPushCurrent(m_cuCtx);
    }

    cuStreamCreate(&m_stream, CU_STREAM_DEFAULT);

    std::cout << "[OptiX] Context Created! Initialising OptiX..." << std::endl;

    optixInit();
    OptixDeviceContextOptions options = {};
    options.logCallbackLevel = 4;
    optixDeviceContextCreate(m_cuCtx, &options, &m_context);

    cudaMalloc((void**)&m_dIntensity, sizeof(float));
    m_initialized = true;
    std::cout << "[OptiX] Device Created!" << std::endl;

}


void OptiXDenoiser::setupDenoiser(int w, int h) {
    std::cout << "[OptiX] Setting up Denoiser..." << std::endl;

    cuCtxSetCurrent(m_cuCtx);

    if (m_denoiser && w == m_width && h == m_height) return;

    if (m_denoiser) optixDenoiserDestroy(m_denoiser);

    m_width = w;
    m_height = h;

    OptixDenoiserOptions options = {};
    optixDenoiserCreate(m_context, OPTIX_DENOISER_MODEL_KIND_HDR, &options, &m_denoiser);

    OptixDenoiserSizes sizes;
    optixDenoiserComputeMemoryResources(m_denoiser, w, h, &sizes);

    m_stateSize = sizes.stateSizeInBytes;
    m_scratchSize = sizes.withoutOverlapScratchSizeInBytes;

    if (m_dState) cudaFree((void*)m_dState);
    if (m_dScratch) cudaFree((void*)m_dScratch);

    cudaMalloc((void**)&m_dState, sizes.stateSizeInBytes);
    cudaMalloc((void**)&m_dScratch, sizes.withoutOverlapScratchSizeInBytes);

    optixDenoiserSetup(m_denoiser, m_stream, w, h, m_dState,
                    sizes.stateSizeInBytes, m_dScratch,
                    sizes.withoutOverlapScratchSizeInBytes
    );

    std::cout << "[OptiX] Denoiser Created!" << std::endl;

}

void OptiXDenoiser::run(float* data, int w, int h)
{
    std::cout << "[OptiX] Rendering..." << std::endl;

    cuCtxSetCurrent(m_cuCtx);

    setupDevice();
    setupDenoiser(w, h);

    std::cout << "[OptiX] Creating Buffers and Loading Data..." << std::endl;

    size_t pixelSize = sizeof(float) * 3;
    size_t bufferSize = (size_t)w * h * pixelSize;

    CUdeviceptr d_input = 0;
    CUdeviceptr d_output = 0;

    cudaMalloc((void**)&d_input, bufferSize);
    cudaMalloc((void**)&d_output, bufferSize);

    cudaMemcpy((void*)d_input, data, bufferSize, cudaMemcpyHostToDevice);

    std::cout << "[OptiX] Got Data! Prepping Denoiser..." << std::endl;

    

    OptixImage2D inputImage = {};
    inputImage.data = d_input;
    inputImage.width = w;
    inputImage.height = h;
    inputImage.rowStrideInBytes = w * pixelSize;
    inputImage.pixelStrideInBytes = pixelSize;
    inputImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;

    OptixImage2D outputImage = {};
    outputImage.data = d_output;
    outputImage.width = w;
    outputImage.height = h;
    outputImage.rowStrideInBytes = w * pixelSize;
    outputImage.pixelStrideInBytes = pixelSize;
    outputImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;

    OptixDenoiserParams params = {};
    params.hdrIntensity = 0;
    params.blendFactor = 0.0f;

    OptixDenoiserLayer layer = {};
    layer.input  = inputImage;
    layer.output = outputImage;

    OptixDenoiserGuideLayer guideLayer = {};

    std::cout << "[OptiX] Denoiser Prepped! Running Denoiser..." << std::endl;

    optixDenoiserInvoke(
        m_denoiser,
        m_stream,
        &params,
        m_dState,
        m_stateSize,
        &guideLayer,
        &layer,
        1,
        0, 0,
        m_dScratch,
        m_scratchSize
    );

    cudaDeviceSynchronize();

    std::cout << "[OptiX] Denoised! Writing Data..." << std::endl;

    cudaMemcpy(data, (void*)d_output, bufferSize, cudaMemcpyDeviceToHost);

    cudaFree((void*)d_input);
    cudaFree((void*)d_output);

    std::cout << "[OptiX] Finished!" << std::endl;

}

void OptiXDenoiser::cleanup() {
    std::cout << "[OptiX] Cleaning up..." << std::endl;

    // 1. Destroy OptiX specific objects
    if (m_denoiser) {
        optixDenoiserDestroy(m_denoiser);
        m_denoiser = nullptr;
    }

    if (m_context) {
        optixDeviceContextDestroy(m_context);
        m_context = nullptr;
    }

    // 2. Free GPU Buffers
    if (m_dState) {
        cudaFree((void*)m_dState);
        m_dState = 0;
    }
    if (m_dScratch) {
        cudaFree((void*)m_dScratch);
        m_dScratch = 0;
    }
    if (m_dIntensity) {
        cudaFree((void*)m_dIntensity);
        m_dIntensity = 0;
    }

    // 3. Cleanup Stream and Context
    if (m_stream) {
        cuStreamDestroy(m_stream);
        m_stream = 0;
    }

    // Reset initialization flag
    m_initialized = false;

    std::cout << "[OptiX] Cleaning Finished!" << std::endl;

}