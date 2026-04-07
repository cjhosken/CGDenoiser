#include "denoiser.h"

static Iop* build(Node* node) {return new CGDenoiser(node); }

const Iop::Description CGDenoiser::desc("CGDenoiser", build);

void CGDenoiser::knobs(Knob_Callback f) {
    Enumeration_KnobI(f, &_engine, "engine", "Denoise Engine");
    ToolTip(f, "OIDN (CPU/GPU) or OptiX (NVIDIA GPU)");
}

void CGDenoiser::_validate(bool for_real) {
    copy_info();
}

void CGDenoiser::renderStripe(ImagePlane& plane) {
    input0().fetchPlane(plane);

    float* pixelData = plane.writable();
    int width = plane.bounds().w();
    int height = plane.bounds().h();

    if (_engine == 0) {
        runOIDN(pixelData, width, height);
    } else {
        runOptiX(pixelData, width, height);
    }
}

void CGDenoiser::runOIDN(float* data, int w, int h) {
    static oidn::DeviceRef device = oidn::newDevice();
    device.commit()

    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color", data, oidn::Format::Float3, w, h)
    filter.setImage("output", data, oidn::Format:Float3, w, h);
    filter.commit();

    filter.execute();
    const char* errorMessage;
    if (device.getError(errorMessage) != oidn::Error::None) {
        std::cerr << "OIDN Error: " << errorMessage << std::endl;
    }
}

void CGDenoiser::runOptiX(float* data, int w, int h) {

}