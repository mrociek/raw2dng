

#pragma once
#include "yi.h"
#include <vector>
#include <iostream>

XiaomiYiProcessor::XiaomiYiProcessor(AutoPtr<dng_host> &host, std::string filename, Exiv2::Image::AutoPtr &inputImage):
    RawExiv2Processor(host, filename, inputImage)
{
    // bootstrapping libraw_.*_t with fixed values
    image_sizes.width = 4608;
    image_sizes.height = 3456;
    image_sizes.iwidth = image_sizes.width;
    image_sizes.iheight = image_sizes.height;
    image_sizes.raw_width = image_sizes.width;
    image_sizes.raw_height = image_sizes.height;
    image_sizes.left_margin = 0;
    image_sizes.top_margin = 0;
    image_sizes.flip = 0;

    image_params.make[0] = 'X';
    image_params.make[1] = 'I';
    image_params.make[2] = 'A';
    image_params.make[3] = 'O';
    image_params.make[4] = 'Y';
    image_params.make[5] = 'I';
    image_params.model[0] = 'Y';
    image_params.model[1] = 'X';
    image_params.model[2] = 'D';
    image_params.model[3] = 'J';
    image_params.model[4] = ' ';
    image_params.model[5] = '1';
    image_params.colors = 3;
    image_params.cdesc[0] = 'R';
    image_params.cdesc[1] = 'G';
    image_params.cdesc[2] = 'B';
    image_params.cdesc[3] = 'G';
    image_params.filters = 0xb4b4b4b4;

    image_colordata.black = 0;
    image_colordata.maximum = 16383;
    //image_colordata.cblack = {0, 0, 0, 0, 0};
    //image_colordata.cam_mul = {1.0, 1.0, 1.0};
    //image_colordata.cam_xyz = ;

    loadBayerData();
}


libraw_image_sizes_t* XiaomiYiProcessor::getSizeInfo()
{
    return &image_sizes;
}


libraw_iparams_t* XiaomiYiProcessor::getImageParams()
{
    return &image_params;
}


libraw_colordata_t* XiaomiYiProcessor::getColorData()
{
    return &image_colordata;
}


unsigned short* XiaomiYiProcessor::getRawBuffer() 
{
    return bayerData.data();
}


uint32 XiaomiYiProcessor::getInputPlanes()
{
    return 1;
}


void XiaomiYiProcessor::loadBayerData()
{
    std::ifstream file(m_inputFileName, std::ios::binary | std::ios::ate);
    std::streamsize filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    bayerData.clear();
    unsigned short pixelVal;
    while (file.read((char*) &pixelVal, sizeof(unsigned short)))
    {
        bayerData.push_back(pixelVal);
    }
    printf("Successfully loaded %d pixel values", bayerData.size());
}


void XiaomiYiProcessor::setDNGPropertiesFromInput() {
    RawExiv2Processor::setDNGPropertiesFromInput();
    //printf("Resetting crop\n");
    //m_negative->SetDefaultCropOrigin(0, 0);
    //m_negative->SetDefaultCropSize(image_sizes.width, image_sizes.height);
}
