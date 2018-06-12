


#pragma once
#include "rawexiv.h"
#include "dng_input.h"
#include "sony/ILCE7.h"
#include "fuji/common.h"
#include "variousVendorProcessor.h"

#include <stdexcept>

#include <dng_simple_image.h>
#include <dng_camera_profile.h>
#include <dng_file_stream.h>
#include <dng_memory_stream.h>
#include <dng_xmp.h>

#include <zlib.h>

#include <exiv2/xmp.hpp>
#include <libraw/libraw.h>


RawExiv2Processor::RawExiv2Processor(AutoPtr<dng_host> &host, std::string filename, Exiv2::Image::AutoPtr &inputImage):
    RawProcessor(host, filename),
    m_InputImage(inputImage),
    m_InputExif(m_InputImage->exifData()),
    m_InputXmp(m_InputImage->xmpData())
{
}


void RawExiv2Processor::setXmpFromInput(const dng_date_time_info &dateTimeNow, const dng_string &appNameVersion) {
    // Copy existing XMP-tags in raw-file to DNG

    AutoPtr<dng_xmp> negXmp(new dng_xmp(m_host->Allocator()));
    for (Exiv2::XmpData::const_iterator it = m_InputXmp.begin(); it != m_InputXmp.end(); it++) {
        try {
            negXmp->Set(Exiv2::XmpProperties::nsInfo(it->groupName())->ns_, it->tagName().c_str(), it->toString().c_str());
        }
        catch (dng_exception& e) {
            // the above will throw an exception when trying to add XMPs with unregistered (i.e., unknown) 
            // namespaces -- we just drop them here.
            std::cerr << "Dropped XMP-entry from raw-file since namespace is unknown: "
                         "NS: "   << Exiv2::XmpProperties::nsInfo(it->groupName())->ns_ << ", "
                         "path: " << it->tagName().c_str() << ", "
                         "text: " << it->toString().c_str() << "\n";
        }
    }

    // Set some base-XMP tags (incl. redundant creation date under Photoshop namespace - just to stay close to Adobe...)

    negXmp->UpdateDateTime(dateTimeNow);
    negXmp->UpdateMetadataDate(dateTimeNow);
    negXmp->SetString(XMP_NS_XAP, "CreatorTool", appNameVersion);
    negXmp->Set(XMP_NS_DC, "format", "image/dng");
    negXmp->SetString(XMP_NS_PHOTOSHOP, "DateCreated", m_negative->GetExif()->fDateTimeOriginal.Encode_ISO_8601());

    m_negative->ResetXMP(negXmp.Release());
}

bool RawExiv2Processor::getInterpretedInputExifTag(const char* exifTagName, int32 component, uint32* value) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if (it == m_InputExif.end()) return false;

    std::stringstream interpretedValue; it->write(interpretedValue, &m_InputExif);

    uint32 tmp;
    for (int i = 0; (i <= component) && !interpretedValue.fail(); i++) interpretedValue >> tmp;
    if (interpretedValue.fail()) return false;

    *value = tmp;
    return true;
}

bool RawExiv2Processor::getInputExifTag(const char* exifTagName, dng_string* value) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if (it == m_InputExif.end()) return false;

    value->Set_ASCII((it->print(&m_InputExif)).c_str()); value->TrimLeadingBlanks(); value->TrimTrailingBlanks();
    return true;
}

bool RawExiv2Processor::getInputExifTag(const char* exifTagName, dng_date_time_info* value) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if (it == m_InputExif.end()) return false;

    dng_date_time dt; dt.Parse((it->print(&m_InputExif)).c_str()); value->SetDateTime(dt);
    return true;
}

bool RawExiv2Processor::getInputExifTag(const char* exifTagName, int32 component, dng_srational* rational) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if ((it == m_InputExif.end()) || (it->count() < (component + 1))) return false;

    Exiv2::Rational exiv2Rational = (*it).toRational(component);
    *rational = dng_srational(exiv2Rational.first, exiv2Rational.second);
    return true;
}

bool RawExiv2Processor::getInputExifTag(const char* exifTagName, int32 component, dng_urational* rational) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if ((it == m_InputExif.end()) || (it->count() < (component + 1))) return false;

    Exiv2::URational exiv2Rational = (*it).toRational(component);
    *rational = dng_urational(exiv2Rational.first, exiv2Rational.second);
    return true;
}

bool RawExiv2Processor::getInputExifTag(const char* exifTagName, int32 component, uint32* value) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if ((it == m_InputExif.end()) || (it->count() < (component + 1))) return false;

    *value = static_cast<uint32>(it->toLong(component));
    return true;
}

int RawExiv2Processor::getInputExifTag(const char* exifTagName, uint32* valueArray, int32 maxFill) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if (it == m_InputExif.end()) return 0;

    int lengthToFill = std::min(maxFill, static_cast<int32>(it->count()));
    for (int i = 0; i < lengthToFill; i++)
        valueArray[i] = static_cast<uint32>(it->toLong(i));
    return lengthToFill;
}

int RawExiv2Processor::getInputExifTag(const char* exifTagName, int16* valueArray, int32 maxFill) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if (it == m_InputExif.end()) return 0;

    int lengthToFill = std::min(maxFill, static_cast<int32>(it->count()));
    for (int i = 0; i < lengthToFill; i++)
        valueArray[i] = static_cast<int16>(it->toLong(i));
    return lengthToFill;
}

int RawExiv2Processor::getInputExifTag(const char* exifTagName, dng_urational* valueArray, int32 maxFill) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if (it == m_InputExif.end()) return 0;

    int lengthToFill = std::min(maxFill, static_cast<int32>(it->count()));
    for (int i = 0; i < lengthToFill; i++) {
        Exiv2::URational exiv2Rational = (*it).toRational(i);
        valueArray[i] = dng_urational(exiv2Rational.first, exiv2Rational.second);
    }
    return lengthToFill;
}

bool RawExiv2Processor::getInputExifTag(const char* exifTagName, long* size, unsigned char** data) {
    Exiv2::ExifData::const_iterator it = m_InputExif.findKey(Exiv2::ExifKey(exifTagName));
    if (it == m_InputExif.end()) return false;

    *data = new unsigned char[(*it).size()]; *size = (*it).size();
    (*it).copy((Exiv2::byte*)*data, Exiv2::bigEndian);
    return true;
}
