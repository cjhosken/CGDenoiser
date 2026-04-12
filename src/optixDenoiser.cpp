#include "optixDenoiser.h"

OptiXDenoiser::OptiXDenoiser() {}

OptiXDenoiser::~OptiXDenoiser() {}

void OptiXDenoiser::render(ImagePlane &plane, ImagePlane &inputPlane, Box box)
{
    std::cerr << "Running OptiX\n";

    int W = box.w();
    int H = box.h();

    cudaFree(0);
    CUresult res = cuCtxGetCurrent(&cuCtx);
    if (res != CUDA_SUCCESS || cuCtx == nullptr) {
        // If no context, you might need to create one:
        cuDevicePrimaryCtxRetain(&cuCtx, 0); 
        cuCtxPushCurrent(cuCtx);
    }

    CUstream stream;
    cuStreamCreate(&stream, CU_STREAM_DEFAULT);

    static bool optixInitialized = false;
    if (!optixInitialized) {
        if (optixInit() == OPTIX_SUCCESS) optixInitialized = true;
    }

    OptixDeviceContext context = nullptr;
    OptixDeviceContextOptions options = {};
    optixDeviceContextCreate(cuCtx, &options, &context);

    OptixDenoiserOptions denoiserOptions = {};

    OptixDenoiser denoiser;
    optixDenoiserCreate(
        context,
        OPTIX_DENOISER_MODEL_KIND_HDR,
        &denoiserOptions,
        &denoiser
    );

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

    OptixDenoiserSizes sizes;
    optixDenoiserComputeMemoryResources(denoiser, W, H, &sizes);

    CUdeviceptr d_state, d_scratch;
    cudaMalloc((void**)&d_state, sizes.stateSizeInBytes);
    cudaMalloc((void**)&d_scratch, sizes.withoutOverlapScratchSizeInBytes);

    optixDenoiserSetup(
        denoiser,
        stream,
        W, H,
        d_state,
        sizes.stateSizeInBytes,
        d_scratch,
        sizes.withoutOverlapScratchSizeInBytes
    );

    CUdeviceptr d_hdrIntensity;
    float h_hdrIntensity = 1.0f; 
    cudaMalloc((void**)&d_hdrIntensity, sizeof(float));
    cudaMemcpy((void*)d_hdrIntensity, &h_hdrIntensity, sizeof(float), cudaMemcpyHostToDevice);

    OptixDenoiserParams params = {};
    params.hdrIntensity = d_hdrIntensity; // Valid GPU address
    params.blendFactor = 0.0f;


    OptixDenoiserLayer layer = {};
    layer.input  = inputImage;
    layer.output = outputImage;

    OptixDenoiserGuideLayer guideLayer = {};

    optixDenoiserInvoke(
        denoiser,
        stream,
        &params,
        d_state,
        sizes.stateSizeInBytes,
        &guideLayer,
        &layer,
        1,
        0, 0,
        d_scratch,
        sizes.withoutOverlapScratchSizeInBytes
    );

    cudaFree((void*)d_hdrIntensity);

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
    cudaFree((void*)d_state);
    cudaFree((void*)d_scratch);
    

    optixDenoiserDestroy(denoiser);
    optixDeviceContextDestroy(context);
    cuStreamDestroy(stream);

    std::cerr << "Running OptiX\n";
}