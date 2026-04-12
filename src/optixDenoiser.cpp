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

void OptiXDenoiser::run(float* color, float* albedo, float* normal, float* motion, int w, int h)
{
    std::cout << "[OptiX] Rendering..." << std::endl;

    cuCtxSetCurrent(m_cuCtx);

    setupDevice();
    setupDenoiser(w, h);

    std::cout << "[OptiX] Creating Buffers and Loading Data..." << std::endl;

    size_t pixelSize3 = sizeof(float) * 3;
    size_t pixelSize2 = sizeof(float) * 2;

    size_t bufferSizeColor = (size_t)w * h * pixelSize3;
    size_t bufferSizeFlow  = (size_t)w * h * pixelSize2;

    CUdeviceptr d_color  = 0;
    CUdeviceptr d_output = 0;
    CUdeviceptr d_albedo = 0;
    CUdeviceptr d_normal = 0;
    CUdeviceptr d_motion = 0;

    cudaMalloc((void**)&d_color,  bufferSizeColor);
    cudaMalloc((void**)&d_output, bufferSizeColor);

    if (albedo)
        cudaMalloc((void**)&d_albedo, bufferSizeColor);

    if (normal)
        cudaMalloc((void**)&d_normal, bufferSizeColor);

    if (motion)
        cudaMalloc((void**)&d_motion, bufferSizeFlow);

    cudaMemcpy((void*)d_color, color, bufferSizeColor, cudaMemcpyHostToDevice);

    if (albedo)
        cudaMemcpy((void*)d_albedo, albedo, bufferSizeColor, cudaMemcpyHostToDevice);

    if (normal)
        cudaMemcpy((void*)d_normal, normal, bufferSizeColor, cudaMemcpyHostToDevice);

    if (motion)
        cudaMemcpy((void*)d_motion, motion, bufferSizeFlow, cudaMemcpyHostToDevice);

    std::cout << "[OptiX] Got Data! Prepping Denoiser..." << std::endl;

    OptixImage2D inputImage = {};
    inputImage.data = d_color;
    inputImage.width = w;
    inputImage.height = h;
    inputImage.rowStrideInBytes = w * pixelSize3;
    inputImage.pixelStrideInBytes = pixelSize3;
    inputImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;

    OptixImage2D outputImage = {};
    outputImage.data = d_output;
    outputImage.width = w;
    outputImage.height = h;
    outputImage.rowStrideInBytes = w * pixelSize3;
    outputImage.pixelStrideInBytes = pixelSize3;
    outputImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;

    OptixImage2D albedoImage = {};
    if (albedo)
    {
        albedoImage.data = d_albedo;
        albedoImage.width = w;
        albedoImage.height = h;
        albedoImage.rowStrideInBytes = w * pixelSize3;
        albedoImage.pixelStrideInBytes = pixelSize3;
        albedoImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;
    }

    OptixImage2D normalImage = {};
    if (normal)
    {
        normalImage.data = d_normal;
        normalImage.width = w;
        normalImage.height = h;
        normalImage.rowStrideInBytes = w * pixelSize3;
        normalImage.pixelStrideInBytes = pixelSize3;
        normalImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;
    }

    OptixImage2D flowImage = {};
    if (motion)
    {
        flowImage.data = d_motion;
        flowImage.width = w;
        flowImage.height = h;
        flowImage.rowStrideInBytes = w * pixelSize2;
        flowImage.pixelStrideInBytes = pixelSize2;
        flowImage.format = OPTIX_PIXEL_FORMAT_FLOAT2;
    }

    OptixDenoiserParams params = {};
    params.hdrIntensity = 0;
    params.blendFactor = 0.0f;

    OptixDenoiserLayer layer = {};
    layer.input  = inputImage;
    layer.output = outputImage;

    OptixDenoiserGuideLayer guideLayer = {};
    if (albedo)
        guideLayer.albedo = albedoImage;

    if (normal)
        guideLayer.normal = normalImage;

    if (motion)
        guideLayer.flow = flowImage;

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

    cudaMemcpy(color, (void*)d_output, bufferSizeColor, cudaMemcpyDeviceToHost);

    cudaFree((void*)d_color);
    cudaFree((void*)d_output);

    if (d_albedo) cudaFree((void*)d_albedo);
    if (d_normal) cudaFree((void*)d_normal);
    if (d_motion) cudaFree((void*)d_motion);

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