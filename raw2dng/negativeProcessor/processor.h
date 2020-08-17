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

#include <dng_host.h>
#include <dng_negative.h>
#include <dng_exif.h>
#include <exiv2/image.hpp>
#include <libraw/libraw_types.h>
#include <functional>


class LibRaw;

const char* getDngErrorMessage(int errorCode);

class NegativeProcessor {
public:
   static NegativeProcessor* createProcessor(AutoPtr<dng_host> &host, std::string& filename);
   static NegativeProcessor* createProcessor(AutoPtr<dng_host> &host, std::string& filename, std::string& jpgFilename);
   static NegativeProcessor* createProcessor(AutoPtr<dng_host> &host, std::string& filename, std::string& greenFilename, std::string& blueFilename);
   //virtual ~NegativeProcessor();

   dng_negative* getNegative() {return m_negative.Get();}

   // Different raw/DNG processing stages - usually called in this sequence
   virtual void setDNGPropertiesFromInput() = 0;
   virtual void setCameraProfile(const char *dcpFilename) = 0;
   virtual void setExifFromInput(const dng_date_time_info &dateTimeNow, const dng_string &appNameVersion) = 0;
   virtual void setXmpFromInput(const dng_date_time_info &dateTimeNow, const dng_string &appNameVersion) = 0;
   virtual void backupProprietaryData() = 0;
   virtual void buildDNGImage() = 0;
   virtual void embedOriginalFile(const char *rawFilename);

protected:
   NegativeProcessor(AutoPtr<dng_host> &host, std::string filename);
   // Overloaded builder method to be used with Xiaomi Yi files
   NegativeProcessor(AutoPtr<dng_host> &host, std::string filename, std::string jpgFilename);

   // Target: DNG-file
   AutoPtr<dng_host> &m_host;
   AutoPtr<dng_negative> m_negative;
   std::string m_inputFileName;
};
