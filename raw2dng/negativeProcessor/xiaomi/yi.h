/* Copyright (C) 2018 Dmitrii Nikishov

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
*/

#pragma once


#include "../rawexiv.h"



class XiaomiYiProcessor : public RawExiv2Processor
{
friend class NegativeProcessor;
public:
    virtual void setDNGPropertiesFromInput() override;
protected:
    XiaomiYiProcessor(AutoPtr<dng_host> &host, std::string filename, Exiv2::Image::AutoPtr &inputImage);
    libraw_image_sizes_t* getSizeInfo() override;
    libraw_iparams_t* getImageParams() override;
    libraw_colordata_t* getColorData() override;
    unsigned short* getRawBuffer() override;
    uint32 getInputPlanes() override;
    virtual void loadBayerData();

    std::vector<unsigned short> bayerData;
    libraw_image_sizes_t image_sizes;
    libraw_iparams_t image_params;
    libraw_colordata_t image_colordata;
};
