
#pragma once
#include "raw.h"
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


RawProcessor::RawProcessor(AutoPtr<dng_host> &host, std::string filename) : NegativeProcessor(host, filename) {}


ColorKeyCode colorKey(const char color) {
    switch (color) {
        case 'R': return colorKeyRed;
        case 'G': return colorKeyGreen;
        case 'B': return colorKeyBlue;
        case 'C': return colorKeyCyan;
        case 'M': return colorKeyMagenta;
        case 'Y': return colorKeyYellow;
        case 'E': return colorKeyCyan; // only in the Sony DSC-F828. 'Emerald' - like cyan according to Sony
        case 'T':                      // what is 'T'??? LibRaw reports that only for the Leaf Catchlight, so I guess we're not compatible with early '90s tech...
        default:  return colorKeyMaxEnum;
    }
}


void RawProcessor::setDNGPropertiesFromInput()
{
    libraw_image_sizes_t *sizes   = getSizeInfo();
    libraw_iparams_t *iparams = getImageParams();
    libraw_colordata_t *colors  = getColorData();

    // Set original file name
    std::string file = m_inputFileName;
    size_t found = std::min(file.rfind("\\"), file.rfind("/"));
    if (found != std::string::npos) file = file.substr(found + 1, file.length() - found - 1);
    m_negative->SetOriginalRawFileName(file.c_str());

    // Set the manufacturer and model
    dng_string makeModel;
    makeModel.Append(iparams->make);
    makeModel.Append(" ");
    makeModel.Append(iparams->model);
    m_negative->SetModelName(makeModel.Get());

    // Set orientation 

    switch (sizes->flip) {
        case 180:
        case 3:  m_negative->SetBaseOrientation(dng_orientation::Rotate180()); break;
        case 270:
        case 5:  m_negative->SetBaseOrientation(dng_orientation::Rotate90CCW()); break;
        case 90:
        case 6:  m_negative->SetBaseOrientation(dng_orientation::Rotate90CW()); break;
        default: m_negative->SetBaseOrientation(dng_orientation::Normal()); break;
    }

    // Set colorkey information
    // Note: this needs to happen before Mosaic - how about documenting that in the SDK???
    m_negative->SetColorChannels(iparams->colors);
    m_negative->SetColorKeys(colorKey(iparams->cdesc[0]), colorKey(iparams->cdesc[1]), 
                             colorKey(iparams->cdesc[2]), colorKey(iparams->cdesc[3]));

    // Mosaic
    // TODO: add foveon support?
    if (iparams->colors == 4) m_negative->SetQuadMosaic(iparams->filters);
    else switch(iparams->filters) {
            case 0xe1e1e1e1:  m_negative->SetBayerMosaic(0); break;
            case 0xb4b4b4b4:  m_negative->SetBayerMosaic(1); break;
            case 0x1e1e1e1e:  m_negative->SetBayerMosaic(2); break;
            case 0x4b4b4b4b:  m_negative->SetBayerMosaic(3); break;
            default: break;  // not throwing error, because this might be set in a sub-class (e.g., Fuji)
        }

    // Default scale and crop/active area
    m_negative->SetDefaultScale(dng_urational(sizes->iwidth, sizes->width), dng_urational(sizes->iheight, sizes->height));
    m_negative->SetActiveArea(dng_rect(sizes->top_margin, sizes->left_margin,
                                       sizes->top_margin + sizes->height, sizes->left_margin + sizes->width));

    //uint32 cropWidth, cropHeight;
    //if (!getInputExifTag("Exif.Photo.PixelXDimension", 0, &cropWidth) ||
    //    !getInputExifTag("Exif.Photo.PixelYDimension", 0, &cropHeight) && sizes->left_margin) {
    //    cropWidth = sizes->width - 16;
    //    cropHeight = sizes->height - 16;
    //}

    //int cropLeftMargin = (cropWidth > sizes->width ) ? 0 : (sizes->width  - cropWidth) / 2;
    //int cropTopMargin = (cropHeight > sizes->height) ? 0 : (sizes->height - cropHeight) / 2;

    m_negative->SetDefaultCropOrigin(0, 0);
    m_negative->SetDefaultCropSize(sizes->width, sizes->height);

    // Set white balance (CameraNeutral)
    dng_vector cameraNeutral(iparams->colors);
    for (int i = 0; i < iparams->colors; i++)
        cameraNeutral[i] = 1.0 / colors->cam_mul[i];
    m_negative->SetCameraNeutral(cameraNeutral);

    // BlackLevel & WhiteLevel
    for (int i = 0; i < 4; i++)
        m_negative->SetWhiteLevel(static_cast<uint32>(colors->maximum), i);

    if ((m_negative->GetMosaicInfo() != NULL) && (m_negative->GetMosaicInfo()->fCFAPatternSize == dng_point(2, 2)))
        m_negative->SetQuadBlacks(colors->black + colors->cblack[0],
                                  colors->black + colors->cblack[1],
                                  colors->black + colors->cblack[2],
                                  colors->black + colors->cblack[3]);
    else 
    	m_negative->SetBlackLevel(colors->black + colors->cblack[0], 0);

    // Fixed properties
    m_negative->SetBaselineExposure(0.0);                       // should be fixed per camera
    m_negative->SetBaselineNoise(1.0);
    m_negative->SetBaselineSharpness(1.0);

    // default
    m_negative->SetAntiAliasStrength(dng_urational(100, 100));  // = no aliasing artifacts
    m_negative->SetLinearResponseLimit(1.0);                    // = no non-linear sensor response
    m_negative->SetAnalogBalance(dng_vector_3(1.0, 1.0, 1.0));
    m_negative->SetShadowScale(dng_urational(1, 1));
}


void RawProcessor::setCameraProfile(const char *dcpFilename) {
    AutoPtr<dng_camera_profile> prof(new dng_camera_profile);

    if (strlen(dcpFilename) > 0) {
        dng_file_stream profStream(dcpFilename);
        if (!prof->ParseExtended(profStream))
            throw std::runtime_error("Could not parse supplied camera profile file!");
    }
    else {
        // Build our own minimal profile, based on one colour matrix provided by LibRaw
        dng_string profName;
        libraw_iparams_t* idata = getImageParams();
        libraw_colordata_t* colordata = getColorData();
        profName.Append(idata->make);
        profName.Append(" ");
        profName.Append(idata->model);

        prof->SetName(profName.Get());
        prof->SetCalibrationIlluminant1(lsD65);

        int colors = idata->colors;
        if ((colors == 3) || (colors = 4)) {
	        dng_matrix *colormatrix1 = new dng_matrix(colors, 3);
                dng_matrix *colormatrix2;

        	for (int i = 0; i < colors; i++)
        		for (int j = 0; j < 3; j++)
        			(*colormatrix1)[i][j] = colordata->cam_xyz[i][j];

            if (colormatrix1->MaxEntry() == 0.0) {
                printf("Warning, camera XYZ Matrix is null\n");
                delete colormatrix1;
                if (colors == 3)
                {
                    colormatrix1 = new dng_matrix_3by3(0.9490, -0.3814, -0.0225,
                                                       -0.6649, 1.3741, 0.3236,
                                                       -0.0627, 0.0796, 0.7550);
                    colormatrix2 = new dng_matrix_3by3(0.8489, -0.2583, -0.1036,
                                                       -0.8051, 1.5583, 0.2643,
                                                       -0.1307, 0.1407, 0.7354);
                }
                else
                // TODO: not specified atm
                {
                    colormatrix1 = new dng_matrix_4by3(0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
                    colormatrix2 = new dng_matrix_4by3(0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
                }
            }
            prof->SetColorMatrix1(*colormatrix1);
            prof->SetColorMatrix2(*colormatrix2);
        }
        prof->SetProfileCalibrationSignature("com.fimagena.raw2dng");
    }

    m_negative->AddProfile(prof);
}


void RawProcessor::setExifFromInput(const dng_date_time_info &dateTimeNow, const dng_string &appNameVersion) {
    dng_exif *negExif = m_negative->GetExif();

    // TIFF 6.0 "D. Other Tags"
    getInputExifTag("Exif.Image.DateTime", &negExif->fDateTime);
    getInputExifTag("Exif.Image.ImageDescription", &negExif->fImageDescription);
    getInputExifTag("Exif.Image.Make", &negExif->fMake);
    getInputExifTag("Exif.Image.Model", &negExif->fModel);
    getInputExifTag("Exif.Image.Software", &negExif->fSoftware);
    getInputExifTag("Exif.Image.Artist", &negExif->fArtist);
    getInputExifTag("Exif.Image.Copyright", &negExif->fCopyright);

    // Exif 2.3 "A. Tags Relating to Version" (order as in spec)
    getInputExifTag("Exif.Photo.ExifVersion", 0, &negExif->fExifVersion);
    // Exif.Photo.FlashpixVersion - fFlashPixVersion : ignoring this here

    // Exif 2.3 "B. Tags Relating to Image data Characteristics" (order as in spec)
    getInputExifTag("Exif.Photo.ColorSpace", 0, &negExif->fColorSpace);
    // Gamma : Supported by DNG SDK (fGamma) but not Exiv2 (v0.24)

    // Exif 2.3 "C. Tags Relating To Image Configuration" (order as in spec)
    getInputExifTag("Exif.Photo.ComponentsConfiguration", 0, &negExif->fComponentsConfiguration);
    getInputExifTag("Exif.Photo.CompressedBitsPerPixel", 0, &negExif->fCompresssedBitsPerPixel);  // nice typo in DNG SDK...
    getInputExifTag("Exif.Photo.PixelXDimension", 0, &negExif->fPixelXDimension);
    getInputExifTag("Exif.Photo.PixelYDimension", 0, &negExif->fPixelYDimension);

    // Exif 2.3 "D. Tags Relating to User Information" (order as in spec)
    // MakerNote: We'll deal with that below
    getInputExifTag("Exif.Photo.UserComment", &negExif->fUserComment);

    // Exif 2.3 "E. Tags Relating to Related File Information" (order as in spec)
    // RelatedSoundFile : DNG SDK doesn't support this

    // Exif 2.3 "F. Tags Relating to Date and Time" (order as in spec)
    getInputExifTag("Exif.Photo.DateTimeOriginal", &negExif->fDateTimeOriginal);
    getInputExifTag("Exif.Photo.DateTimeDigitized", &negExif->fDateTimeDigitized);
    // SubSecTime          : DNG SDK doesn't support this
    // SubSecTimeOriginal  : DNG SDK doesn't support this
    // SubSecTimeDigitized : DNG SDK doesn't support this

    // Exif 2.3 "G. Tags Relating to Picture-Taking Conditions" (order as in spec)
    getInputExifTag("Exif.Photo.ExposureTime", 0, &negExif->fExposureTime);
    getInputExifTag("Exif.Photo.FNumber", 0, &negExif->fFNumber);
    getInputExifTag("Exif.Photo.ExposureProgram", 0, &negExif->fExposureProgram);
    // SpectralSensitivity : DNG SDK doesn't support this
    getInputExifTag("Exif.Photo.ISOSpeedRatings", negExif->fISOSpeedRatings, 3); // PhotographicSensitivity in Exif 2.3
    // OECF : DNG SDK doesn't support this
    getInputExifTag("Exif.Photo.SensitivityType", 0, &negExif->fSensitivityType);
    getInputExifTag("Exif.Photo.StandardOutputSensitivity", 0, &negExif->fStandardOutputSensitivity);
    getInputExifTag("Exif.Photo.RecommendedExposureIndex", 0, &negExif->fRecommendedExposureIndex);
    getInputExifTag("Exif.Photo.ISOSpeed", 0, &negExif->fISOSpeed);
    getInputExifTag("Exif.Photo.ISOSpeedLatitudeyyy", 0, &negExif->fISOSpeedLatitudeyyy);
    getInputExifTag("Exif.Photo.ISOSpeedLatitudezzz", 0, &negExif->fISOSpeedLatitudezzz);
    getInputExifTag("Exif.Photo.ShutterSpeedValue", 0, &negExif->fShutterSpeedValue);
    getInputExifTag("Exif.Photo.ApertureValue", 0, &negExif->fApertureValue);
    getInputExifTag("Exif.Photo.BrightnessValue", 0, &negExif->fBrightnessValue);
    getInputExifTag("Exif.Photo.ExposureBiasValue", 0, &negExif->fExposureBiasValue);
    getInputExifTag("Exif.Photo.MaxApertureValue", 0, &negExif->fMaxApertureValue);
    getInputExifTag("Exif.Photo.SubjectDistance", 0, &negExif->fSubjectDistance);
    getInputExifTag("Exif.Photo.MeteringMode", 0, &negExif->fMeteringMode);
    getInputExifTag("Exif.Photo.LightSource", 0, &negExif->fLightSource);
    getInputExifTag("Exif.Photo.Flash", 0, &negExif->fFlash);
    getInputExifTag("Exif.Photo.FocalLength", 0, &negExif->fFocalLength);
    negExif->fSubjectAreaCount = getInputExifTag("Exif.Photo.SubjectArea", negExif->fSubjectArea, 4);
    // FlashEnergy : DNG SDK doesn't support this
    // SpatialFrequencyResponse : DNG SDK doesn't support this
    getInputExifTag("Exif.Photo.FocalPlaneXResolution", 0, &negExif->fFocalPlaneXResolution);
    getInputExifTag("Exif.Photo.FocalPlaneYResolution", 0, &negExif->fFocalPlaneYResolution);
    getInputExifTag("Exif.Photo.FocalPlaneResolutionUnit", 0, &negExif->fFocalPlaneResolutionUnit);
    // SubjectLocation : DNG SDK doesn't support this
    getInputExifTag("Exif.Photo.ExposureIndex", 0, &negExif->fExposureIndex);
    getInputExifTag("Exif.Photo.SensingMethod", 0, &negExif->fSensingMethod);
    getInputExifTag("Exif.Photo.FileSource", 0, &negExif->fFileSource);
    getInputExifTag("Exif.Photo.SceneType", 0, &negExif->fSceneType);
    // CFAPattern: we write it manually from raw data further below
    getInputExifTag("Exif.Photo.CustomRendered", 0, &negExif->fCustomRendered);
    getInputExifTag("Exif.Photo.ExposureMode", 0, &negExif->fExposureMode);
    getInputExifTag("Exif.Photo.WhiteBalance", 0, &negExif->fWhiteBalance);
    getInputExifTag("Exif.Photo.DigitalZoomRatio", 0, &negExif->fDigitalZoomRatio);
    getInputExifTag("Exif.Photo.FocalLengthIn35mmFilm", 0, &negExif->fFocalLengthIn35mmFilm);
    getInputExifTag("Exif.Photo.SceneCaptureType", 0, &negExif->fSceneCaptureType);
    getInputExifTag("Exif.Photo.GainControl", 0, &negExif->fGainControl);
    getInputExifTag("Exif.Photo.Contrast", 0, &negExif->fContrast);
    getInputExifTag("Exif.Photo.Saturation", 0, &negExif->fSaturation);
    getInputExifTag("Exif.Photo.Sharpness", 0, &negExif->fSharpness);
    // DeviceSettingsDescription : DNG SDK doesn't support this
    getInputExifTag("Exif.Photo.SubjectDistanceRange", 0, &negExif->fSubjectDistanceRange);

    // Exif 2.3 "H. Other Tags" (order as in spec)
    // ImageUniqueID : DNG SDK doesn't support this
    getInputExifTag("Exif.Photo.CameraOwnerName", &negExif->fOwnerName);
    getInputExifTag("Exif.Photo.BodySerialNumber", &negExif->fCameraSerialNumber);
    getInputExifTag("Exif.Photo.LensSpecification", negExif->fLensInfo, 4);
    getInputExifTag("Exif.Photo.LensMake", &negExif->fLensMake);
    getInputExifTag("Exif.Photo.LensModel", &negExif->fLensName);
    getInputExifTag("Exif.Photo.LensSerialNumber", &negExif->fLensSerialNumber);

    // Exif 2.3 GPS "A. Tags Relating to GPS" (order as in spec)
    uint32 gpsVer[4];  gpsVer[0] = gpsVer[1] = gpsVer[2] = gpsVer[3] = 0;
    getInputExifTag("Exif.GPSInfo.GPSVersionID", gpsVer, 4);
    negExif->fGPSVersionID = (gpsVer[0] << 24) + (gpsVer[1] << 16) + (gpsVer[2] <<  8) + gpsVer[3];
    getInputExifTag("Exif.GPSInfo.GPSLatitudeRef", &negExif->fGPSLatitudeRef);
    getInputExifTag("Exif.GPSInfo.GPSLatitude", negExif->fGPSLatitude, 3);
    getInputExifTag("Exif.GPSInfo.GPSLongitudeRef", &negExif->fGPSLongitudeRef);
    getInputExifTag("Exif.GPSInfo.GPSLongitude", negExif->fGPSLongitude, 3);
    getInputExifTag("Exif.GPSInfo.GPSAltitudeRef", 0, &negExif->fGPSAltitudeRef);
    getInputExifTag("Exif.GPSInfo.GPSAltitude", 0, &negExif->fGPSAltitude);
    getInputExifTag("Exif.GPSInfo.GPSTimeStamp", negExif->fGPSTimeStamp, 3);
    getInputExifTag("Exif.GPSInfo.GPSSatellites", &negExif->fGPSSatellites);
    getInputExifTag("Exif.GPSInfo.GPSStatus", &negExif->fGPSStatus);
    getInputExifTag("Exif.GPSInfo.GPSMeasureMode", &negExif->fGPSMeasureMode);
    getInputExifTag("Exif.GPSInfo.GPSDOP", 0, &negExif->fGPSDOP);
    getInputExifTag("Exif.GPSInfo.GPSSpeedRef", &negExif->fGPSSpeedRef);
    getInputExifTag("Exif.GPSInfo.GPSSpeed", 0, &negExif->fGPSSpeed);
    getInputExifTag("Exif.GPSInfo.GPSTrackRef", &negExif->fGPSTrackRef);
    getInputExifTag("Exif.GPSInfo.GPSTrack", 0, &negExif->fGPSTrack);
    getInputExifTag("Exif.GPSInfo.GPSImgDirectionRef", &negExif->fGPSImgDirectionRef);
    getInputExifTag("Exif.GPSInfo.GPSImgDirection", 0, &negExif->fGPSImgDirection);
    getInputExifTag("Exif.GPSInfo.GPSMapDatum", &negExif->fGPSMapDatum);
    getInputExifTag("Exif.GPSInfo.GPSDestLatitudeRef", &negExif->fGPSDestLatitudeRef);
    getInputExifTag("Exif.GPSInfo.GPSDestLatitude", negExif->fGPSDestLatitude, 3);
    getInputExifTag("Exif.GPSInfo.GPSDestLongitudeRef", &negExif->fGPSDestLongitudeRef);
    getInputExifTag("Exif.GPSInfo.GPSDestLongitude", negExif->fGPSDestLongitude, 3);
    getInputExifTag("Exif.GPSInfo.GPSDestBearingRef", &negExif->fGPSDestBearingRef);
    getInputExifTag("Exif.GPSInfo.GPSDestBearing", 0, &negExif->fGPSDestBearing);
    getInputExifTag("Exif.GPSInfo.GPSDestDistanceRef", &negExif->fGPSDestDistanceRef);
    getInputExifTag("Exif.GPSInfo.GPSDestDistance", 0, &negExif->fGPSDestDistance);
    getInputExifTag("Exif.GPSInfo.GPSProcessingMethod", &negExif->fGPSProcessingMethod);
    getInputExifTag("Exif.GPSInfo.GPSAreaInformation", &negExif->fGPSAreaInformation);
    getInputExifTag("Exif.GPSInfo.GPSDateStamp", &negExif->fGPSDateStamp);
    getInputExifTag("Exif.GPSInfo.GPSDifferential", 0, &negExif->fGPSDifferential);
    // GPSHPositioningError : Supported by DNG SDK (fGPSHPositioningError) but not Exiv2 (v0.24)

    // Exif 2.3, Interoperability IFD "A. Attached Information Related to Interoperability"
    getInputExifTag("Exif.Iop.InteroperabilityIndex", &negExif->fInteroperabilityIndex);
    getInputExifTag("Exif.Iop.InteroperabilityVersion", 0, &negExif->fInteroperabilityVersion); // this is not in the Exif standard but in DNG SDK and Exiv2

    /*
      Fields in the DNG SDK Exif structure that we are ignoring here.
      Some could potentially be read through Exiv2 but are not part of
      the Exif standard so we leave them out:
      - fBatteryLevelA, fBatteryLevelR
      - fSelfTimerMode
      - fTIFF_EP_StandardID, fImageNumber
      - fApproxFocusDistance
      - fFlashCompensation, fFlashMask
      - fFirmware 
      - fLensID
      - fLensNameWasReadFromExif (--> available but not used by SDK)
      - fRelatedImageFileFormat, fRelatedImageWidth, fRelatedImageLength
    */

    // Write CFAPattern (section G) manually from mosaicinfo

    const dng_mosaic_info* mosaicinfo = m_negative->GetMosaicInfo();
    if (mosaicinfo != NULL) {
        negExif->fCFARepeatPatternCols = mosaicinfo->fCFAPatternSize.v;
        negExif->fCFARepeatPatternRows = mosaicinfo->fCFAPatternSize.h;
        for (uint16 c = 0; c < negExif->fCFARepeatPatternCols; c++)
            for (uint16 r = 0; r < negExif->fCFARepeatPatternRows; r++)
                negExif->fCFAPattern[r][c] = mosaicinfo->fCFAPattern[c][r];
    }

    // Reconstruct LensName from LensInfo if not present

    if (negExif->fLensName.IsEmpty()) {
        dng_urational *li = negExif->fLensInfo;
        std::stringstream lensName; lensName.precision(1); lensName.setf(std::ios::fixed, std::ios::floatfield);

        if (li[0].IsValid())      lensName << li[0].As_real64();
        if (li[1] != li[2])       lensName << "-" << li[1].As_real64();
        if (lensName.tellp() > 0) lensName << " mm";
        if (li[2].IsValid())      lensName << " f/" << li[2].As_real64();
        if (li[3] != li[2])       lensName << "-" << li[3].As_real64();

        negExif->fLensName.Set_ASCII(lensName.str().c_str());
    }

    // Overwrite date, software, version - these are now referencing the DNG-creation

    negExif->fDateTime = dateTimeNow;
    negExif->fSoftware = appNameVersion;
    negExif->fExifVersion = DNG_CHAR4 ('0','2','3','0'); 
}


void RawProcessor::backupProprietaryData() {
    AutoPtr<dng_memory_stream> DNGPrivateTag(createDNGPrivateTag());

    if (DNGPrivateTag.Get()) {
        AutoPtr<dng_memory_block> blockPriv(DNGPrivateTag->AsMemoryBlock(m_host->Allocator()));
        m_negative->SetPrivateData(blockPriv);
    }
}


dng_memory_stream* RawProcessor::createDNGPrivateTag() {
    uint32 mnOffset = 0;
    dng_string mnByteOrder;
    long mnLength = 0;
    unsigned char* mnBuffer = 0;

    if (getInputExifTag("Exif.MakerNote.Offset", 0, &mnOffset) &&
        getInputExifTag("Exif.MakerNote.ByteOrder", &mnByteOrder) &&
        getInputExifTag("Exif.Photo.MakerNote", &mnLength, &mnBuffer)) {
        bool padding = (mnLength & 0x01) == 0x01;

        dng_memory_stream *streamPriv = new dng_memory_stream(m_host->Allocator());
        streamPriv->SetBigEndian();

        // Use Adobe vendor-format for encoding MakerNotes in DNGPrivateTag
        streamPriv->Put("Adobe", 5);
        streamPriv->Put_uint8(0x00);

        streamPriv->Put("MakN", 4);
        streamPriv->Put_uint32(mnLength + mnByteOrder.Length() + 4 + (padding ? 1 : 0));
        streamPriv->Put(mnByteOrder.Get(), mnByteOrder.Length());
        streamPriv->Put_uint32(mnOffset);
        streamPriv->Put(mnBuffer, mnLength);
        if (padding) streamPriv->Put_uint8(0x00);

        delete[] mnBuffer;

        return streamPriv;
    }

    return NULL;
}


void RawProcessor::buildDNGImage() {
    libraw_image_sizes_t *sizes = getSizeInfo();

    // Select right data source from LibRaw
    unsigned short *rawBuffer = getRawBuffer();
    uint32 inputPlanes = getInputPlanes();
    libraw_iparams_t* idata = getImageParams();

    uint32 outputPlanes = (inputPlanes == 1) ? 1 : idata->colors;

    // Create new dng_image and copy data
    dng_rect bounds = dng_rect(sizes->raw_height, sizes->raw_width);
    dng_simple_image *image = new dng_simple_image(bounds, outputPlanes, ttShort, m_host->Allocator());

    dng_pixel_buffer buffer; image->GetPixelBuffer(buffer);
    unsigned short *imageBuffer = (unsigned short*)buffer.fData;

    if (inputPlanes == outputPlanes)
        memcpy(imageBuffer, rawBuffer, sizes->raw_height * sizes->raw_width * outputPlanes * sizeof(unsigned short));
    else {
        for (int i = 0; i < (sizes->raw_height * sizes->raw_width); i++) {
            memcpy(imageBuffer, rawBuffer, outputPlanes * sizeof(unsigned short));
            imageBuffer += outputPlanes;
            rawBuffer += inputPlanes;
        }
    }

    AutoPtr<dng_image> castImage(dynamic_cast<dng_image*>(image));
    m_negative->SetStage1Image(castImage);
}
