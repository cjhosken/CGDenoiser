#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <optix_function_table_definition.h> // Keep it ONLY here
#include "optixDenoiser.h"


OptiXDenoiser::OptiXDenoiser() {
    model = 0;
    blend = 1.0f;
}

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

void OptiXDenoiser::setupDenoiser(int w, int h, bool dirty) {
    std::cout << "[OptiX] Setting up Denoiser..." << std::endl;

    cuCtxSetCurrent(m_cuCtx);

    if (!dirty) {
        if (m_denoiser && w == m_width && h == m_height) return;
    }

    if (m_denoiser) optixDenoiserDestroy(m_denoiser);

    m_hasPrev = false;

    m_width = w;
    m_height = h;

    OptixDenoiserOptions options = {};
    OptixDenoiserModelKind kind;
    if (model == 0)      kind = OPTIX_DENOISER_MODEL_KIND_HDR;
    else if (model == 1) kind = OPTIX_DENOISER_MODEL_KIND_AOV;
    else                 kind = OPTIX_DENOISER_MODEL_KIND_TEMPORAL;

    optixDenoiserCreate(m_context, kind, &options, &m_denoiser);

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

void OptiXDenoiser::run(DenoiserData& data, bool deviceDirty, bool filterDirty)
{
    std::cout << "[OptiX] Rendering..." << std::endl;

    int w = data.getWidth();
    int h = data.getHeight();

    cuCtxSetCurrent(m_cuCtx);

    setupDevice();
    setupDenoiser(w, h, deviceDirty || filterDirty);
    

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

    if (!m_prevOutput || w != m_width || h != m_height)
    {
        if (m_prevOutput)
            cudaFree((void*)m_prevOutput);

        cudaMalloc((void**)&m_prevOutput, bufferSizeColor);

        m_hasPrev = false;
    }

    if (data.hasAlbedo())
        cudaMalloc((void**)&d_albedo, bufferSizeColor);

    if (data.hasNormal())
        cudaMalloc((void**)&d_normal, bufferSizeColor);

    if (data.hasMotion())
        cudaMalloc((void**)&d_motion, bufferSizeFlow);

    cudaMemcpy((void*)d_color, data.getColor(), bufferSizeColor, cudaMemcpyHostToDevice);

    if (!m_hasPrev)
    {
        cudaMemcpy((void*)m_prevOutput, (void*)d_color, bufferSizeColor, cudaMemcpyDeviceToDevice);
        m_hasPrev = true;
    }

    if (data.hasAlbedo())
        cudaMemcpy((void*)d_albedo, data.getAlbedo(), bufferSizeColor, cudaMemcpyHostToDevice);

    if (data.hasNormal())
        cudaMemcpy((void*)d_normal, data.getNormal(), bufferSizeColor, cudaMemcpyHostToDevice);
    
    if (data.hasMotion())
        cudaMemcpy((void*)d_motion, data.getMotion(), bufferSizeFlow, cudaMemcpyHostToDevice);


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
    if (data.hasAlbedo())
    {
        albedoImage.data = d_albedo;
        albedoImage.width = w;
        albedoImage.height = h;
        albedoImage.rowStrideInBytes = w * pixelSize3;
        albedoImage.pixelStrideInBytes = pixelSize3;
        albedoImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;
    }

    OptixImage2D normalImage = {};
    if (data.hasNormal())
    {
        normalImage.data = d_normal;
        normalImage.width = w;
        normalImage.height = h;
        normalImage.rowStrideInBytes = w * pixelSize3;
        normalImage.pixelStrideInBytes = pixelSize3;
        normalImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;
    }

    OptixImage2D flowImage = {};
    if (data.hasMotion())
    {
        flowImage.data = d_motion;
        flowImage.width = w;
        flowImage.height = h;
        flowImage.rowStrideInBytes = w * pixelSize2;
        flowImage.pixelStrideInBytes = pixelSize2;
        flowImage.format = OPTIX_PIXEL_FORMAT_FLOAT2;
    }

    optixDenoiserComputeIntensity(
        m_denoiser,
        m_stream,
        &inputImage,
        m_dIntensity,
        m_dScratch,
        m_scratchSize
    );

    OptixDenoiserParams params = {};
    params.hdrIntensity = m_dIntensity;
    params.blendFactor = 1.0f - blend;

    OptixDenoiserLayer layer = {};
    layer.input  = inputImage;
    layer.output = outputImage;

    OptixDenoiserGuideLayer guideLayer = {};
    if (data.hasAlbedo())
        guideLayer.albedo = albedoImage;

    if (data.hasNormal())
        guideLayer.normal = normalImage;

    if (model == 2) {
        OptixImage2D prevOutputImage = {};
        prevOutputImage.data = m_prevOutput;
        prevOutputImage.width = w;
        prevOutputImage.height = h;
        prevOutputImage.rowStrideInBytes = w * pixelSize3;
        prevOutputImage.pixelStrideInBytes = pixelSize3;
        prevOutputImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;

        layer.previousOutput = prevOutputImage;

        if (data.hasMotion())
            guideLayer.flow = flowImage;
        
    }

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

    cudaMemcpy((void*)m_prevOutput, (void*)d_output, bufferSizeColor, cudaMemcpyDeviceToDevice);

    std::cout << "[OptiX] Denoised! Writing Data..." << std::endl;

    cudaMemcpy(data.getOutput(), (void*)d_output, bufferSizeColor, cudaMemcpyDeviceToHost);

    cudaFree((void*)d_color);
    cudaFree((void*)d_output);

    if (d_albedo) cudaFree((void*)d_albedo);
    if (d_normal) cudaFree((void*)d_normal);
    if (d_motion) cudaFree((void*)d_motion);

    if (m_prevOutput)
    {
        cudaFree((void*)m_prevOutput);
        m_prevOutput = 0;
    }

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