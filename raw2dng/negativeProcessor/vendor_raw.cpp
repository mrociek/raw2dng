/* Copyright (C) 2018 Dmitry Nikishov

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

   This file uses code from dngconvert from Jens Mueller and others
   (https://github.com/jmue/dngconvert) - Copyright (C) 2011 Jens 
   Mueller (tschensinger at gmx dot de)
*/


#include "vendor_raw.h"

#include <stdexcept>

#include <dng_simple_image.h>
#include <dng_camera_profile.h>
#include <dng_file_stream.h>
#include <dng_memory_stream.h>
#include <dng_xmp.h>

#include <zlib.h>

#include <exiv2/xmp.hpp>
#include <libraw/libraw.h>


VendorRawProcessor::VendorRawProcessor(AutoPtr<dng_host> &host, std::string filename, Exiv2::Image::AutoPtr &inputImage, LibRaw *rawProcessor):
    RawExiv2Processor(host, filename, inputImage),
    m_RawProcessor(rawProcessor)
{
    m_negative.Reset(m_host->Make_dng_negative());
}


VendorRawProcessor::~VendorRawProcessor() {
	m_RawProcessor->recycle();
}


libraw_image_sizes_t* VendorRawProcessor::getSizeInfo()
{
    return &m_RawProcessor->imgdata.sizes;
}


libraw_iparams_t* VendorRawProcessor::getImageParams()
{
    return &m_RawProcessor->imgdata.idata;
}


libraw_colordata_t* VendorRawProcessor::getColorData()
{
    return &m_RawProcessor->imgdata.color;
}


unsigned short* VendorRawProcessor::getRawBuffer() 
{
    unsigned short *rawBuffer = (unsigned short*) m_RawProcessor->imgdata.rawdata.raw_image;

    if (rawBuffer == NULL) {
        rawBuffer = (unsigned short*) m_RawProcessor->imgdata.rawdata.color3_image;
    }
    if (rawBuffer == NULL) {
        rawBuffer = (unsigned short*) m_RawProcessor->imgdata.rawdata.color4_image;
    }
    return rawBuffer;
}


uint32 VendorRawProcessor::getInputPlanes()
{
    if (m_RawProcessor->imgdata.rawdata.raw_image != NULL)
    {
        return 1;
    }
    else if (m_RawProcessor->imgdata.rawdata.raw_image == NULL &&
             m_RawProcessor->imgdata.rawdata.color3_image != NULL)
    {
        return 3;
    }
    else
    {
        return 4;
    }
}
