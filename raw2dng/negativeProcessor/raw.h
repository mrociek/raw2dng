/* Copyright (C) 2015 Fimagena

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

#include "processor.h"

#include <dng_host.h>
#include <dng_negative.h>
#include <dng_exif.h>
#include <exiv2/image.hpp>
#include <libraw/libraw_types.h>


/*
  Class for raw files that might not have metadata in a format that
  can be parsed with libexiv2. To allow for custom metadata source,
  helper functions will have to be overriden in the derived class.
*/
class RawProcessor : public NegativeProcessor {
public:
   virtual void setDNGPropertiesFromInput() override;
   virtual void setCameraProfile(const char *dcpFilename) override;
   virtual void setExifFromInput(const dng_date_time_info &dateTimeNow, const dng_string &appNameVersion) override;
   virtual void backupProprietaryData() override;
   virtual void buildDNGImage() override;

protected:
   RawProcessor(AutoPtr<dng_host> &host, std::string filename);

   virtual dng_memory_stream* createDNGPrivateTag();

   // helper functions
   virtual bool getInterpretedInputExifTag(const char* exifTagName, int32 component, uint32* value) = 0;

   virtual bool getInputExifTag(const char* exifTagName, dng_string* value) = 0;
   virtual bool getInputExifTag(const char* exifTagName, dng_date_time_info* value) = 0;
   virtual bool getInputExifTag(const char* exifTagName, int32 component, dng_srational* rational) = 0;
   virtual bool getInputExifTag(const char* exifTagName, int32 component, dng_urational* rational) = 0;
   virtual bool getInputExifTag(const char* exifTagName, int32 component, uint32* value) = 0;

   virtual int  getInputExifTag(const char* exifTagName, uint32* valueArray, int32 maxFill) = 0;
   virtual int  getInputExifTag(const char* exifTagName, int16* valueArray, int32 maxFill) = 0;
   virtual int  getInputExifTag(const char* exifTagName, dng_urational* valueArray, int32 maxFill) = 0;

   virtual bool getInputExifTag(const char* exifTagName, long* size, unsigned char** data) = 0;

   // used internally by conversion procedures, must be re-defined
   // in derived classes

   // libraw_image_sizes_t. following fields must be set:
   // - width, height -- visible rectangle dimensions
   // - iwidth, iheight -- output image dimensions (see libraw docs)
   // - raw_height, raw_width -- full rectangle
   // - left_margin, top_margin -- top left corner of the visible rectangle
   // - flip -- orientation
   virtual libraw_image_sizes_t* getSizeInfo() = 0;

   // libraw_iparams_t. following fields must be set:
   // - make
   // - model
   // - colors (number of colors: 3 for traditional RGGB)
   // - cdesc
   // - filters
   virtual libraw_iparams_t* getImageParams() = 0;

   // libraw_colordata_t. following fields must be set:
   // - black
   // - cam_mul
   // - cblack
   // - maximum
   // - cam_xyz
   virtual libraw_colordata_t* getColorData() = 0;
   virtual unsigned short* getRawBuffer() = 0;
   virtual uint32 getInputPlanes() = 0;
};
