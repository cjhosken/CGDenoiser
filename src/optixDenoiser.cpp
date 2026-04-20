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

void OptiXDenoiser::setupDevice()
{
    if (m_initialized)
        return;

    std::cout << "[OptiX] Setting up Device..." << std::endl;

    // -------------------------
    // 1. Init CUDA driver
    // -------------------------
    cudaFree(0);
    cuCtxGetCurrent(&m_cuCtx);
    if (!m_cuCtx) {
        cuDevicePrimaryCtxRetain(&m_cuCtx, 0);
        cuCtxPushCurrent(m_cuCtx);
    }

    // -------------------------
    // 4. Stream
    // -------------------------
    cuStreamCreate(&m_stream, CU_STREAM_DEFAULT);

    // -------------------------
    // 5. OptiX init
    // -------------------------
    OptixResult optixRes = optixInit();
    if (optixRes != OPTIX_SUCCESS) {
        std::cerr << "[OptiX] optixInit failed\n";
        return;
    }

    OptixDeviceContextOptions options = {};
    options.logCallbackLevel = 4;

    OptixResult ctxRes = optixDeviceContextCreate(
        m_cuCtx,
        &options,
        &m_context
    );

    if (ctxRes != OPTIX_SUCCESS) {
        std::cerr << "[OptiX] Context creation failed\n";
        return;
    }

    // -------------------------
    // 6. Intensity buffer
    // -------------------------
    cudaMalloc((void**)&m_dIntensity, sizeof(float));

    m_initialized = true;

    std::cout << "[OptiX] Device Created OK" << std::endl;
}

void OptiXDenoiser::setupDenoiser(int w, int h, bool dirty) {
    std::cout << "[OptiX] Setting up Denoiser..." << std::endl;
    

    cuCtxSetCurrent(m_cuCtx);

    if (!dirty && m_denoiser && w == m_width && h == m_height) return;

    if (m_denoiser)
    {
        optixDenoiserDestroy(m_denoiser);
        m_denoiser = nullptr;
    }

    m_width = w;
    m_height = h;
    m_hasPrev = false; // IMPORTANT: reset history only on resize/model change

    OptixDenoiserOptions options = {};
    OptixDenoiserModelKind kind;
    if (model == 0)      kind = OPTIX_DENOISER_MODEL_KIND_HDR;
    else if (model == 1) kind = OPTIX_DENOISER_MODEL_KIND_AOV;
    else                 kind = OPTIX_DENOISER_MODEL_KIND_TEMPORAL;

    optixDenoiserCreate(m_context, kind, &options, &m_denoiser);

    OptixDenoiserSizes sizes;
    optixDenoiserComputeMemoryResources(m_denoiser, w, h, &sizes);

    if (m_dState) cudaFree((void*)m_dState);
    if (m_dScratch) cudaFree((void*)m_dScratch);

    m_stateSize = sizes.stateSizeInBytes;
    m_scratchSize = sizes.withoutOverlapScratchSizeInBytes;

    cudaMalloc((void**)&m_dState, m_stateSize);
    cudaMalloc((void**)&m_dScratch, m_scratchSize);

    optixDenoiserSetup(m_denoiser, m_stream, w, h, m_dState,
                    sizes.stateSizeInBytes, m_dScratch,
                    sizes.withoutOverlapScratchSizeInBytes
    );

    std::cout << "[OptiX] Denoiser Created!" << std::endl;

}

void OptiXDenoiser::run(DenoiserData& data, bool deviceDirty, bool filterDirty)
{
    std::cout << "[OptiX] Rendering..." << std::endl;

    cuCtxSetCurrent(m_cuCtx);

    setupDevice();
    setupDenoiser(data.getWidth(), data.getHeight(), deviceDirty || filterDirty);
    
    int w = data.getWidth();
    int h = data.getHeight();

    std::cout << "[OptiX] Creating Buffers and Loading Data..." << std::endl;

    size_t pixelSize3 = sizeof(float) * 3;
    size_t pixelSize2 = sizeof(float) * 2;

    size_t sizeColor = (size_t)w * h * pixelSize3;
    size_t sizeFlow  = (size_t)w * h * pixelSize2;

    if (!m_dColor)
        cudaMalloc((void**)&m_dColor, sizeColor);

    if (!m_dOutput)
        cudaMalloc((void**)&m_dOutput, sizeColor);

    if (data.hasAlbedo() && !m_dAlbedo)
        cudaMalloc((void**)&m_dAlbedo, sizeColor);

    if (data.hasNormal() && !m_dNormal)
        cudaMalloc((void**)&m_dNormal, sizeColor);

    if (data.hasMotion() && !m_dMotion)
        cudaMalloc((void**)&m_dMotion, sizeFlow);

    cudaMemcpy((void*)m_dColor, data.getColor(), sizeColor, cudaMemcpyHostToDevice);

    if (data.hasAlbedo())
        cudaMemcpy((void*)m_dAlbedo, data.getAlbedo(), sizeColor, cudaMemcpyHostToDevice);

    if (data.hasNormal())
        cudaMemcpy((void*)m_dNormal, data.getNormal(), sizeColor, cudaMemcpyHostToDevice);

    if (data.hasMotion())
        cudaMemcpy((void*)m_dMotion, data.getMotion(), sizeFlow, cudaMemcpyHostToDevice);


    std::cout << "[OptiX] Got Data! Prepping Denoiser..." << std::endl;

    // ----------------------------
    // Build OptiX images
    // ----------------------------
    OptixImage2D input = {};
    input.data = m_dColor;
    input.width = w;
    input.height = h;
    input.rowStrideInBytes = w * pixelSize3;
    input.pixelStrideInBytes = pixelSize3;
    input.format = OPTIX_PIXEL_FORMAT_FLOAT3;

    OptixImage2D output = {};
    output.data = m_dOutput;
    output.width = w;
    output.height = h;
    output.rowStrideInBytes = w * pixelSize3;
    output.pixelStrideInBytes = pixelSize3;
    output.format = OPTIX_PIXEL_FORMAT_FLOAT3;

    OptixImage2D albedo = {};
    if (data.hasAlbedo())
    {
        albedo.data = m_dAlbedo;
        albedo.width = w;
        albedo.height = h;
        albedo.format = OPTIX_PIXEL_FORMAT_FLOAT3;
        albedo.rowStrideInBytes = w * pixelSize3;
        albedo.pixelStrideInBytes = pixelSize3;
    }

    OptixImage2D normal = {};
    if (data.hasNormal())
    {
        normal.data = m_dNormal;
        normal.width = w;
        normal.height = h;
        normal.format = OPTIX_PIXEL_FORMAT_FLOAT3;
        normal.rowStrideInBytes = w * pixelSize3;
        normal.pixelStrideInBytes = pixelSize3;
    }

    OptixImage2D flow = {};
    if (data.hasMotion())
    {
        flow.data = m_dMotion;
        flow.width = w;
        flow.height = h;
        flow.format = OPTIX_PIXEL_FORMAT_FLOAT2;
        flow.rowStrideInBytes = w * pixelSize2;
        flow.pixelStrideInBytes = pixelSize2;
    }

    OptixDenoiserParams params = {};

    if (model == 0) {

        optixDenoiserComputeIntensity(
            m_denoiser,
            m_stream,
            &input,
            m_dIntensity,
            m_dScratch,
            m_scratchSize
        );

        params.hdrIntensity = m_dIntensity;

    }

    params.blendFactor = 1.0f - blend;

    OptixDenoiserLayer layer = {};
    layer.input  = input;
    layer.output = output;

    OptixDenoiserGuideLayer guide = {};
    if (data.hasAlbedo() && m_dAlbedo)
        guide.albedo = albedo;

    if (data.hasNormal() && m_dNormal)
        guide.normal = normal;

    if (data.hasMotion() && m_dMotion)
        guide.flow = flow;

    // ----------------------------
    // TEMPORAL FIX (CRITICAL)
    // ----------------------------
    if (model == 2)
    {
        if (!m_prevOutput)
        {
            cudaMalloc((void**)&m_prevOutput, sizeColor);
            cudaMemset((void*)m_prevOutput, 0, sizeColor);
        }

        OptixImage2D prev = {};
        prev.data = m_prevOutput;
        prev.width = w;
        prev.height = h;
        prev.rowStrideInBytes = w * pixelSize3;
        prev.pixelStrideInBytes = pixelSize3;
        prev.format = OPTIX_PIXEL_FORMAT_FLOAT3;

        layer.previousOutput = prev;
    }

    std::cout << "[OptiX] Denoiser Prepped! Running Denoiser..." << std::endl;

    optixDenoiserInvoke(
        m_denoiser,
        m_stream,
        &params,
        m_dState,
        m_stateSize,
        &guide,
        &layer,
        1,
        0, 0,
        m_dScratch,
        m_scratchSize
    );

    cudaDeviceSynchronize();

    cudaMemcpy(data.getOutput(), (void*)m_dOutput, sizeColor, cudaMemcpyDeviceToHost);

    if (model == 2)
    {
        cudaMemcpy(
            (void*)m_prevOutput,
            (void*)m_dOutput,
            sizeColor,
            cudaMemcpyDeviceToDevice
        );
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