#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <optix_function_table_definition.h> // Keep it ONLY here
#include "optixDenoiser.h"


OptiXDenoiser::OptiXDenoiser() {}

OptiXDenoiser::~OptiXDenoiser() {cleanup();}

void OptiXDenoiser::setupDevice() {
    if (m_initialized) return;

    cudaFree(0);
    cuCtxGetCurrent(&m_cuCtx);
    if (!m_cuCtx) {
        cuDevicePrimaryCtxRetain(&m_cuCtx, 0);
        cuCtxPushCurrent(m_cuCtx);
    }

    cuStreamCreate(&m_stream, CU_STREAM_DEFAULT);

    optixInit();
    OptixDeviceContextOptions options = {};
    options.logCallbackLevel = 4;
    optixDeviceContextCreate(m_cuCtx, &options, &m_context);

    cudaMalloc((void**)&m_dIntensity, sizeof(float));
    m_initialized = true;
}


void OptiXDenoiser::setupDenoiser(int w, int h) {
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
}

void OptiXDenoiser::render(ImagePlane &plane, ImagePlane &inputPlane, Box box)
{
    std::cerr << "Running OptiX\n";

    cuCtxSetCurrent(m_cuCtx);

    int W = box.w();
    int H = box.h();

    setupDevice();
    setupDenoiser(W, H);

    size_t pixelSize = sizeof(float) * 3;
    size_t bufferSize = W * H * pixelSize;

    CUdeviceptr d_input, d_output;
    cudaMalloc((void**)&d_input, bufferSize);
    cudaMalloc((void**)&d_output, bufferSize);

    std::vector<float> hostInput(W * H * 3);

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            size_t idx = (y * W + x) * 3;

            hostInput[idx + 0] = inputPlane.at(x, y, 0);
            hostInput[idx + 1] = inputPlane.at(x, y, 1);
            hostInput[idx + 2] = inputPlane.at(x, y, 2);
        }
    }

    cudaMemcpy((void*)d_input, hostInput.data(), bufferSize, cudaMemcpyHostToDevice);

    OptixImage2D inputImage = {};
    inputImage.data = d_input;
    inputImage.width = W;
    inputImage.height = H;
    inputImage.rowStrideInBytes = W * pixelSize;
    inputImage.pixelStrideInBytes = pixelSize;
    inputImage.format = OPTIX_PIXEL_FORMAT_FLOAT3;

    OptixImage2D outputImage = inputImage;
    outputImage.data = d_output;

    OptixDenoiserParams params = {};
    params.hdrIntensity = m_dIntensity; // Valid GPU address
    params.blendFactor = 0.0f;

    OptixDenoiserLayer layer = {};
    layer.input  = inputImage;
    layer.output = outputImage;

    OptixDenoiserGuideLayer guideLayer = {};

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

    std::vector<float> hostOutput(W * H * 3);
    cudaMemcpy(hostOutput.data(), (void*)d_output, bufferSize, cudaMemcpyDeviceToHost);

    // --- Write back to Nuke ---
    plane.writable();

    int r = plane.chanNo(DD::Image::Chan_Red);
    int g = plane.chanNo(DD::Image::Chan_Green);
    int b = plane.chanNo(DD::Image::Chan_Blue);

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            size_t idx = (y * W + x) * 3;

            plane.writableAt(x, y, r) = hostOutput[idx + 0];
            plane.writableAt(x, y, g) = hostOutput[idx + 1];
            plane.writableAt(x, y, b) = hostOutput[idx + 2];
        }
    }

    cudaFree((void*)d_input);
    cudaFree((void*)d_output);

    std::cerr << "Running OptiX\n";
}

void OptiXDenoiser::cleanup() {
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
}