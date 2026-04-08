#include "denoiser.h"
#include <OpenImageDenoise/oidn.hpp>
#include <mutex>

static std::mutex g_oidn_mutex;

static Iop* build(Node* node) {return new CGDenoiser(node); }

const Iop::Description CGDenoiser::desc("CGDenoiser", "Filter/CGDenoiser", build);

void CGDenoiser::knobs(Knob_Callback f) {
    const char* engine_names[] = {
        "OpenImageDenoise (OIDN)",
#ifdef USE_OPTIX
        "NVIDIA OptiX",
#endif
        nullptr
    };
    Enumeration_knob(f, &_engine, engine_names, "engine", "Engine");
}

void CGDenoiser::_validate(bool for_real) {
    copy_info();
    set_out_channels(Mask_All);
    info_.set(info_.x(), info_.y(), info_.r(), info_.t());
}

void CGDenoiser::renderStripe(ImagePlane& plane) {
    input0().fetchPlane(plane);

    const Box& box = plane.bounds();
    int w = box.w();
    int h = box.h();

    const float* baseData = plane.readable();
    auto cStride = plane.chanStride();
    auto rStride = plane.rowStride();


    int nChans = (int)plane.channels().size();
    int processCount = std::min(nChans, 3);

    std::vector<float> interleaved(w * h * 3, 0.0f);

    for (int z = 0; z < processCount; ++z) {
        
        const float* chanPtr = baseData + (z * cStride);

        for (int y = 0; y < h; ++y) {
            const float* rowPtr = chanPtr + (y * rStride);

            for (int x = 0; x < w; ++x) {
                int aiIdx = (y * w + x) * 3 + z; 
                interleaved[aiIdx] = rowPtr[x];
            }
        }
    }

    if (_engine == 0) {
        runOIDN(interleaved.data(), w, h);
    }
#ifdef USE_OPTIX
    else if (_engine == 1) {
        runOptiX(interleaved.data(), w, h);
    }
#endif

    float *writeBase = plane.writable();
    for (int z = 0; z < processCount; ++z) {
        float* chanPtr = writeBase + (z * cStride);

        for (int y = 0; y < h; ++y) {
            float* rowPtr = chanPtr + (y * rStride);
            for (int x = 0; x < w; ++x) {
                int aiIdx = (y * w + x) * 3 + z;
                rowPtr[x] = interleaved[aiIdx];
            }
        }
    }
}

void CGDenoiser::runOIDN(float* data, int w, int h) {
    std::lock_guard<std::mutex> lock(g_oidn_mutex);

    static oidn::DeviceRef device = oidn::newDevice();
    static bool deviceInitialized = false;

    if (!deviceInitialized) {
        device.commit();
        deviceInitialized = true;
    }

    if (!_oidnFilter || _filterW != w || _filterH != h) {
        _oidnFilter = device.newFilter("RT");
        _filterW = w;
        _filterH = h;

        _oidnFilter.setImage("color", data, oidn::Format::Float3, w, h, 0, sizeof(float)*3, sizeof(float)*3*w);
        _oidnFilter.setImage("output", data, oidn::Format::Float3, w, h, 0, sizeof(float)*3, sizeof(float)*3*w);
        _oidnFilter.set("hdr", true);
    } else {
        _oidnFilter.setImage("color", data, oidn::Format::Float3, w, h, 0, sizeof(float)*3, sizeof(float)*3*w);
        _oidnFilter.setImage("output", data, oidn::Format::Float3, w, h, 0, sizeof(float)*3, sizeof(float)*3*w);
    }

    _oidnFilter.commit();
    _oidnFilter.execute();
    const char* errorMessage;
    if (device.getError(errorMessage) != oidn::Error::None) {
        std::cerr << "OIDN Error: " << errorMessage << std::endl;
    }
}
#ifdef USE_OPTIX
void CGDenoiser::runOptiX(float* data, int w, int h) {

}
#endif