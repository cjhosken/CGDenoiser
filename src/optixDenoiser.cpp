#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <optix_function_table_definition.h> // Keep it ONLY here
#include "optixDenoiser.h"
#include <cstring>

OptiXDenoiser::OptiXDenoiser() {
    model = 0;
    blend = 1.0f;

    m_deviceDirty = true;
    m_denoiserDirty = true;
}

OptiXDenoiser::~OptiXDenoiser() {
    if (m_initialized || m_context || m_denoiser)
        cleanup();
}

void OptiXDenoiser::setupDevice()
{
    if (!m_deviceDirty)
        return;

    CUresult cuRes;

    if (!m_initialized) {
        cuRes = cuInit(0);

        if (cuRes != CUDA_SUCCESS)
        {
            std::cerr << "ERROR: cuInit() failed: " << cuRes << "\n";
            return;
        }

        m_device = 0;

        cuRes = cuDeviceGet(&m_device, 0);
        if (cuRes != CUDA_SUCCESS)
        {
            std::cerr << "ERROR: cuDeviceGet() failed: " << cuRes << "\n";
            return;
        }

        #if CUDA_VERSION <= 12080
        cuCtxCreate(&m_cuCtx, CU_CTX_SCHED_SPIN, m_device); // DEBUG What is the best CU_CTX_SCHED_* setting here.
        #else
        cuCtxCreate_v4(&m_cuCtx, nullptr, CU_CTX_SCHED_SPIN, m_device);
        #endif

        cuCtxSetCurrent(m_cuCtx);

        cuRes = cuStreamCreate(&m_stream, CU_STREAM_DEFAULT);
        if (cuRes != CUDA_SUCCESS)
        {
            std::cerr << "ERROR: cuStreamCreate() failed: " << cuRes << "\n";
            return;
        }

        OptixResult res = optixInit();
        if (res != OPTIX_SUCCESS)
        {
            std::cerr << "ERROR: initOptixFunctionTable() failed: " << res << "\n";
            return;
        }

        OptixDeviceContextOptions options = {};
        
        res = optixDeviceContextCreate(m_cuCtx, &options, &m_context);
        if (res != OPTIX_SUCCESS)
        {
            std::cerr << "ERROR: optixDeviceContextCreate() failed: " << res << "\n";
            return;
        }


        m_initialized = true;
    }
     
    m_deviceDirty = false;
    m_denoiserDirty = true;
    std::cout << "[OptiX] Device Created!" << std::endl;
}

void OptiXDenoiser::setupDenoiser(int w, int h) {
    std::cout << "[OptiX] Setting up Denoiser..." << std::endl;

    cuCtxSetCurrent(m_cuCtx);

    if (m_denoiser) optixDenoiserDestroy(m_denoiser);

    m_width = w;
    m_height = h;
    m_hasPrev = false; // IMPORTANT: reset history only on resize/model change

    OptixDenoiserOptions options = {};
    
    OptixDenoiserModelKind kind;
    switch(model)
    {
        case 0: kind = OPTIX_DENOISER_MODEL_KIND_LDR; break;
        case 1: kind = OPTIX_DENOISER_MODEL_KIND_HDR; break;
        case 2: kind = OPTIX_DENOISER_MODEL_KIND_AOV; break;
        case 3: kind = OPTIX_DENOISER_MODEL_KIND_TEMPORAL; break;
        case 4: kind = OPTIX_DENOISER_MODEL_KIND_TEMPORAL_AOV; break;
        case 5: kind = OPTIX_DENOISER_MODEL_KIND_UPSCALE2X; break;
        case 6: kind = OPTIX_DENOISER_MODEL_KIND_TEMPORAL_UPSCALE2X; break;
        default: kind = OPTIX_DENOISER_MODEL_KIND_HDR;
    }

    optixDenoiserCreate(m_context, kind, &options, &m_denoiser);

    OptixDenoiserSizes sizes;
    optixDenoiserComputeMemoryResources(m_denoiser, w, h, &sizes);

    m_stateSize = sizes.stateSizeInBytes;
    m_scratchSize = sizes.withoutOverlapScratchSizeInBytes;

    if (m_dState) cudaFree((void*)m_dState);
    if (m_dScratch) cudaFree((void*)m_dScratch);

    cudaMalloc((void**)&m_dState, m_stateSize);
    cudaMalloc((void**)&m_dScratch, m_scratchSize);

    optixDenoiserSetup(m_denoiser, m_stream, w, h, m_dState,
                    sizes.stateSizeInBytes, m_dScratch,
                    sizes.withoutOverlapScratchSizeInBytes
    );

    if (!m_dIntensity)
        cudaMalloc((void**)&m_dIntensity, sizeof(float));

    m_denoiserDirty = false;

    std::cout << "[OptiX] Denoiser Created!" << std::endl;

}

void OptiXDenoiser::run(DenoiserData& data, bool deviceDirty, bool filterDirty)
{
    std::cout << "[OptiX] Rendering..." << std::endl;

    cuCtxSetCurrent(m_cuCtx);

    m_deviceDirty = deviceDirty || m_deviceDirty;
    m_denoiserDirty = filterDirty || m_denoiserDirty;

    if (m_deviceDirty) {
        setupDevice();
    }

    if (m_deviceDirty || !m_context)
    {
        std::cerr << "[OptiX] Skipping run (device not ready)\n";
        return;
    }
    
    int w = data.inWidth();
    int h = data.inHeight();

    if (m_denoiserDirty) {
        setupDenoiser(data.inWidth(), data.inHeight());
    }
    
    int outW = data.outWidth();
    int outH = data.outHeight();
    
    size_t pixelSize3 = sizeof(float) * 3;
    size_t pixelSize2 = sizeof(float) * 2;

    size_t sizeInColor = (size_t)w * h * pixelSize3;
    size_t sizeOutColor = (size_t)outW *outH * pixelSize3;
    size_t sizeFlow  = (size_t)w * h * pixelSize2;

    if (!m_dColor)
        cudaMalloc((void**)&m_dColor, sizeInColor);

    if (!m_dOutput || m_width != outW || m_height != outH)
    {
        if (m_dOutput) cudaFree((void*)m_dOutput);
        cudaMalloc((void**)&m_dOutput, sizeOutColor);
    }

    if (data.hasAlbedo() && !m_dAlbedo)
        cudaMalloc((void**)&m_dAlbedo, sizeInColor);

    if (data.hasNormal() && !m_dNormal)
        cudaMalloc((void**)&m_dNormal, sizeInColor);

    if (data.hasMotion() && !m_dMotion)
        cudaMalloc((void**)&m_dMotion, sizeFlow);

    cudaMemcpy((void*)m_dColor, data.color(), sizeInColor, cudaMemcpyHostToDevice);

    if (data.hasAlbedo())
        cudaMemcpy((void*)m_dAlbedo, data.albedo(), sizeInColor, cudaMemcpyHostToDevice);

    if (data.hasNormal())
        cudaMemcpy((void*)m_dNormal, data.normal(), sizeInColor, cudaMemcpyHostToDevice);

    if (data.hasMotion())
        cudaMemcpy((void*)m_dMotion, data.motion(), sizeFlow, cudaMemcpyHostToDevice);


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
    output.width = outW;
    output.height = outH;
    output.rowStrideInBytes = outW * pixelSize3;
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


    bool isTemporal = (model == 2 || model == 3 || model == 4 || model == 6);

    // ----------------------------
    // TEMPORAL FIX (CRITICAL)
    // ----------------------------
    if (isTemporal)
    {
        if (!m_prevOutput)
        {
            cudaMalloc((void**)&m_prevOutput, sizeOutColor);
            cudaMemset((void*)m_prevOutput, 0, sizeOutColor);
            m_hasPrev = false;
        }

        OptixImage2D prev = {};
        prev.data = m_prevOutput;
        prev.width = outW;
        prev.height = outH;
        prev.rowStrideInBytes = outW * pixelSize3;
        prev.pixelStrideInBytes = pixelSize3;
        prev.format = OPTIX_PIXEL_FORMAT_FLOAT3;

        if (!m_hasPrev)
        {
            // first frame: still pass valid struct, but empty history
            layer.previousOutput = {};
            layer.previousOutput.data = 0;
            layer.previousOutput.width = 0;
            layer.previousOutput.height = 0;
            layer.previousOutput.rowStrideInBytes = 0;
            layer.previousOutput.pixelStrideInBytes = 0;
            layer.previousOutput.format = OPTIX_PIXEL_FORMAT_FLOAT3;

            m_hasPrev = true;
        }
        else
        {
            layer.previousOutput = prev;
        }
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

    std::cout << "[OptiX] Denoised! Cleaning up..." << std::endl;

    cudaStreamSynchronize(m_stream);

    cudaMemcpy(
        data.output(), 
        (void*)m_dOutput,
        sizeOutColor, 
        cudaMemcpyDeviceToHost
    );

    if (isTemporal)
    {
        cudaMemcpy(
            (void*)m_prevOutput,
            (void*)m_dOutput,
            sizeOutColor,
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