#pragma once

#include "dng_input.h"

class DNGMergeProcessor : public DNGprocessor {
friend class NegativeProcessor;

public:

protected:
    DNGMergeProcessor(AutoPtr<dng_host> &host, std::string filename, std::string& greenFilename, std::string& blueFilename);
    void replaceChannelWithFile(dng_pixel_buffer& destBuffer, std::string& filename, ColorKeyCode color);
};
