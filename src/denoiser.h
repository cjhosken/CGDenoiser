#include "DDImage/PlanarIop.h"
#include "DDImage/Knobs.h"
#include <OpenImageDenoise/oidn.h>
#include <optix.h>
#include <cuda_runtime.h>

using namespace DD::Image;

class CGDenoiser : public PlanarIop {
    int _engine;

public:
    static const Iop::Description desc;
    const char* Class() const override {return desc.name;}
    const char* node_help() const override { return "AI Denoiser using OIDN or OptiX"; }

    CGDenoiser(Node* node) : PlanarIop(node), _engine(0) {}

    void knobs(Knob_Callback f) override;
    void _validate(bool for_real) override;
    void renderStripe(ImagePlane& plane) override;

    void runOIDN(float* colorPtr, int width, int height);
    void runOptiX(float* colorPtr, int width, int height);

}