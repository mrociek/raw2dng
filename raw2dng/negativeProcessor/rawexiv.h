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

#include "raw.h"


/*
  Class for raw files with metadata that should be parsed with libexiv2.
*/
class RawExiv2Processor : public RawProcessor {
protected:
   RawExiv2Processor(AutoPtr<dng_host> &host, std::string filename, Exiv2::Image::AutoPtr &rawImage);

   void setXmpFromInput(const dng_date_time_info &dateTimeNow, const dng_string &appNameVersion) override;

   virtual bool getInterpretedInputExifTag(const char* exifTagName, int32 component, uint32* value) override;

   virtual bool getInputExifTag(const char* exifTagName, dng_string* value) override;
   virtual bool getInputExifTag(const char* exifTagName, dng_date_time_info* value) override;
   virtual bool getInputExifTag(const char* exifTagName, int32 component, dng_srational* rational) override;
   virtual bool getInputExifTag(const char* exifTagName, int32 component, dng_urational* rational) override;
   virtual bool getInputExifTag(const char* exifTagName, int32 component, uint32* value) override;

   virtual int  getInputExifTag(const char* exifTagName, uint32* valueArray, int32 maxFill) override;
   virtual int  getInputExifTag(const char* exifTagName, int16* valueArray, int32 maxFill) override;
   virtual int  getInputExifTag(const char* exifTagName, dng_urational* valueArray, int32 maxFill) override;

   virtual bool getInputExifTag(const char* exifTagName, long* size, unsigned char** data) override;

   Exiv2::Image::AutoPtr m_InputImage;
   Exiv2::ExifData m_InputExif;
   Exiv2::XmpData m_InputXmp;
};
