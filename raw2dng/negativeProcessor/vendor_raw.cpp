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


#include "processor.h"


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
