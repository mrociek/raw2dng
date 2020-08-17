#include "dng_merge_input.h"

#include <stdexcept>

#include <dng_camera_profile.h>
#include <dng_file_stream.h>
#include <dng_xmp.h>
#include <dng_info.h>

#include <exiv2/image.hpp>

DNGMergeProcessor::DNGMergeProcessor(AutoPtr<dng_host> &host, std::string filename, std::string& greenFilename, std::string& blueFilename)
                             : DNGprocessor(host, filename) {
    // Re-read source DNG using DNG SDK - we're ignoring the LibRaw/Exiv2 data structures from now on
    try {
        dng_file_stream stream(m_inputFileName.c_str());

        dng_info info;
        info.Parse(*(m_host.Get()), stream);
        info.PostParse(*(m_host.Get()));
        if (!info.IsValidDNG()) throw dng_exception(dng_error_bad_format);

        m_negative->Parse(*(m_host.Get()), stream, info);
        m_negative->PostParse(*(m_host.Get()), stream, info);

        m_negative->ReadStage1Image(*(m_host.Get()), stream, info);

        const dng_image* rawImage = m_negative->Stage1Image();

        std::vector<unsigned char> buf(rawImage->Width() * rawImage->Height() * rawImage->PixelSize());

        dng_pixel_buffer buffer;
        buffer.fArea = dng_rect(rawImage->Height(), rawImage->Width());
        buffer.fPlane  = 0;
        buffer.fPlanes = rawImage->Planes();
        buffer.fRowStep   = buffer.fPlanes * rawImage->Width();
        buffer.fColStep   = 1;
        buffer.fPlaneStep = 1;
        buffer.fPixelType = rawImage->PixelType();
        buffer.fPixelSize = rawImage->PixelSize();
        buffer.fData = buf.data();

        rawImage->Get(buffer, dng_image::edge_zero);

        if (!greenFilename.empty()) {
            replaceChannelWithFile(buffer, greenFilename, colorKeyGreen);
        }
        if (!blueFilename.empty()) {
            replaceChannelWithFile(buffer, blueFilename, colorKeyBlue);
        }

        AutoPtr<dng_image> dstImage ((*host.Get()).Make_dng_image(rawImage->Bounds(), rawImage->Planes(), rawImage->PixelType()));
        dstImage->Put(buffer);
        m_negative->SetStage1Image(dstImage);


        m_negative->ReadTransparencyMask(*(m_host.Get()), stream, info);
        m_negative->ValidateRawImageDigest(*(m_host.Get()));
    }
    catch (const dng_exception &except) {throw except;}
    catch (...) {throw dng_exception(dng_error_unknown);}
}

void DNGMergeProcessor::replaceChannelWithFile(dng_pixel_buffer& destBuffer, std::string& filename, ColorKeyCode color) {
    try {
        AutoPtr<dng_negative> negative(m_host->Make_dng_negative());
        dng_file_stream stream(filename.c_str());
        dng_info info;
        info.Parse(*(m_host.Get()), stream);
        info.PostParse(*(m_host.Get()));
        if (!info.IsValidDNG()) throw dng_exception(dng_error_bad_format);

        negative->Parse(*(m_host.Get()), stream, info);
        negative->PostParse(*(m_host.Get()), stream, info);
        negative->ReadStage1Image(*(m_host.Get()), stream, info);

        const dng_image* srcImage = negative->Stage1Image();

        std::vector<unsigned char> buf(srcImage->Width() * srcImage->Height() * srcImage->PixelSize());

        dng_pixel_buffer buffer;
        buffer.fArea = dng_rect(srcImage->Height(), srcImage->Width());
        buffer.fPlane  = 0;
        buffer.fPlanes = srcImage->Planes();
        buffer.fRowStep   = buffer.fPlanes * srcImage->Width();
        buffer.fColStep   = 1;
        buffer.fPlaneStep = 1;
        buffer.fPixelType = srcImage->PixelType();
        buffer.fPixelSize = srcImage->PixelSize();
        buffer.fData = buf.data();

        srcImage->Get(buffer, dng_image::edge_zero);

        const dng_mosaic_info *mosaicInfo = m_negative->GetMosaicInfo();

        for (uint32 row = 0; row < srcImage->Height(); ++row) {

            uint16 *dstPtr = destBuffer.DirtyPixel_uint16(row, 0);
            const uint16 *srcPtr = buffer.ConstPixel_uint16(row, 0);

            uint32 patternRow = row % mosaicInfo->fCFAPatternSize.v;

            for (uint32 col = 0; col < srcImage->Width(); ++col) {
                if (mosaicInfo->fCFAPlaneColor[mosaicInfo->fCFAPattern[patternRow][col % mosaicInfo->fCFAPatternSize.h]] == color) {
                    dstPtr[col] = srcPtr[col];
                }
            }
        }
    }
    catch (const dng_exception &except) {throw except;}
    catch (...) {throw dng_exception(dng_error_unknown);}
}
