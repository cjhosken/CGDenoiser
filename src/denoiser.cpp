#include "denoiser.h"
#include "DDImage/Black.h"


void CGDenoiser::renderStripe(DD::Image::ImagePlane& outputPlane)
{
    if (aborted() || cancelled()) return;

    // Check connections
    m_albedo_connected = !(dynamic_cast<DD::Image::Black*>(input(1)));
    m_normal_connected = !(dynamic_cast<DD::Image::Black*>(input(2)));

    DD::Image::Format imageFormat = input0().format();
    m_width = imageFormat.width();
    m_height = imageFormat.height();

    auto bufferSize = m_width * m_height * 3 * sizeof(float);

    m_denoiserData.allocate(m_width, m_height, false, false);
    
    std::cout << "[CGDenoiser] Starting to copy color data..." << std::endl;
    
    DD::Image::Box imageBounds = input0().format();
    DD::Image::Iop* colorInput = dynamic_cast<DD::Image::Iop*>(input(0));
    
    if (colorInput == nullptr) {
        std::cerr << "[CGDenoiser] Error: no color input!" << std::endl;
        return;
    }
    
    if (!colorInput->tryValidate(true)) {
        std::cerr << "[CGDenoiser] Error: color input validation failed!" << std::endl;
        return;
    }
    
    std::cout << "[CGDenoiser] Requesting color data..." << std::endl;
    colorInput->request(imageBounds.x(), imageBounds.y(), imageBounds.r(), imageBounds.t(), 
                       DD::Image::Mask_RGB, 0);
    
    std::cout << "[CGDenoiser] Creating image plane..." << std::endl;
    DD::Image::ImagePlane colorPlane(imageBounds, false, DD::Image::Mask_RGB, 3);
    
    std::cout << "[CGDenoiser] Fetching plane..." << std::endl;
    colorInput->fetchPlane(colorPlane);
    
    std::cout << "[CGDenoiser] Getting readable data..." << std::endl;
    const float* srcData = static_cast<const float*>(colorPlane.readable());
    
    if (!srcData) {
        std::cerr << "[CGDenoiser] Error: srcData is null!" << std::endl;
        return;
    }
    
    float* dstBuffer = m_denoiserData.getColor();
    if (!dstBuffer) {
        std::cerr << "[CGDenoiser] Error: dstBuffer is null!" << std::endl;
        return;
    }
    
    std::cout << "[CGDenoiser] Copying color data..." << std::endl;
    auto chanStride = colorPlane.chanStride();
    int pixelsPerRow = m_width * 3;
    
    // Use pointer arithmetic for faster copying
    #pragma omp parallel for
    for (int c = 0; c < 3; c++) {
        const float* srcChan = &srcData[chanStride * c];
        float* dstPtr = dstBuffer + c;
        
        for (int y = 0; y < m_height; y++) {
            int dstRowStart = ((m_height - y - 1) * m_width) * 3;
            for (int x = 0; x < m_width; x++) {
                *(dstPtr + dstRowStart + x * 3) = srcChan[y * m_width + x];
            }
        }
    }
    
    std::cout << "[CGDenoiser] Color data copied successfully!" << std::endl;

    if (aborted() || cancelled()) return;

    // Write data back
    std::cout << "[CGDenoiser] Writing output..." << std::endl;
    outputPlane.writable();
    const float* outputData = m_denoiserData.getColor();

    const DD::Image::Channel channels[] = {
        DD::Image::Channel::Chan_Red,
        DD::Image::Channel::Chan_Green,
        DD::Image::Channel::Chan_Blue
    };
    
    #pragma omp parallel for
    for (int chanNo = 0; chanNo < 3; chanNo++)
    {
        int c = outputPlane.chanNo(channels[chanNo]);
        if (c < 0) continue;
        
        const float* srcPtr = outputData + chanNo;

        for (int j = 0; j < m_height; j++) {
            int srcRowStart = ((m_height - j - 1) * m_width) * 3;
            for (int i = 0; i < m_width; i++) {
                outputPlane.writableAt(i, j, c) = *(srcPtr + srcRowStart + i * 3);
            }
        }
    }
    
    std::cout << "[CGDenoiser] Output written successfully!" << std::endl;
}



void CGDenoiser::knobs(DD::Image::Knob_Callback f) {}
int CGDenoiser::knob_changed(DD::Image::Knob* k) { return 0; }

const char* CGDenoiser::input_label(int n, char*) const
{
    switch (n)
    {
        case 1: return "albedo";
        case 2: return "normal";
        default: return "color";
    }
}

void CGDenoiser::_validate(bool for_real) { copy_info(); }

void CGDenoiser::getRequests(const DD::Image::Box& box, const DD::Image::ChannelSet& channels, int count, DD::Image::RequestOutput &reqData) const
{
    int nInputs = (int)getInputs().size();
    for (int i = 0; i < nInputs; i++) {
        const DD::Image::ChannelSet channels = input(i)->info().channels();
        input(i)->request(channels, count);
    }
}

bool CGDenoiser::useStripes() const { return false; }

bool CGDenoiser::renderFullPlanes() const { return true; }

static DD::Image::Iop* build(Node* node)
{
    return new CGDenoiser(node);
}

const DD::Image::Iop::Description CGDenoiser::description("CGDenoiser", nullptr, build);