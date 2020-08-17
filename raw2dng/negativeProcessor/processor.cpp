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

   This file uses code from dngconvert from Jens Mueller and others
   (https://github.com/jmue/dngconvert) - Copyright (C) 2011 Jens 
   Mueller (tschensinger at gmx dot de)
*/

#include "processor.h"
#include "dng_input.h"
#include "dng_merge_input.h"
#include "sony/ILCE7.h"
#include "fuji/common.h"
#include "xiaomi/yi.h"
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

const char* getDngErrorMessage(int errorCode) {
    switch (errorCode) {
        default:
        case 100000: return "Unknown error";
        case 100003: return "Processing stopped by user (or host application) request";
        case 100004: return "Necessary host functionality is not present";
        case 100005: return "Out of memory";
        case 100006: return "File format is not valid";
        case 100007: return "Matrix has wrong shape, is badly conditioned, or similar problem";
        case 100008: return "Could not open file";
        case 100009: return "Error reading file";
        case 100010: return "Error writing file";
        case 100011: return "Unexpected end of file";
        case 100012: return "File is damaged in some way";
        case 100013: return "Image is too big to save as DNG";
        case 100014: return "Image is too big to save as TIFF";
        case 100015: return "DNG version is unsupported";
    }
}


NegativeProcessor::NegativeProcessor(AutoPtr<dng_host> &host, std::string filename):
    m_host(host),
    m_inputFileName(filename)
{
    m_negative.Reset(m_host->Make_dng_negative());
}


NegativeProcessor* NegativeProcessor::createProcessor(AutoPtr<dng_host> &host, std::string& filename, std::string& jpgFilename)
{
    Exiv2::Image::AutoPtr inputImage;
    try {
        inputImage = Exiv2::ImageFactory::open(jpgFilename);
        inputImage->readMetadata();
    } 
    catch (Exiv2::Error& e) {
        std::stringstream error; error << "Exiv2-error while opening/parsing rawFile (code " << e.code() << "): " << e.what();
        throw std::runtime_error(error.str());
    }

    return new XiaomiYiProcessor(host, filename, inputImage);
}

NegativeProcessor* NegativeProcessor::createProcessor(AutoPtr<dng_host> &host, std::string& filename, std::string& greenFilename, std::string& blueFilename)
{
    try {
        return new DNGMergeProcessor(host, filename, greenFilename, blueFilename);
    }
    catch (dng_exception &e) {
        std::stringstream error; error << "Cannot parse source DNG-file (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
        throw std::runtime_error(error.str());
    }
}


NegativeProcessor* NegativeProcessor::createProcessor(AutoPtr<dng_host> &host, std::string& filename) {
    // Open and parse rawfile with libraw...
    AutoPtr<LibRaw> rawProcessor(new LibRaw());

    int ret = rawProcessor->open_file(filename.c_str());
    if (ret != LIBRAW_SUCCESS) {
        rawProcessor->recycle();
        std::stringstream error; error << "LibRaw-error while opening rawFile: " << libraw_strerror(ret);
        throw std::runtime_error(error.str());
    }

    ret = rawProcessor->unpack();
    if (ret != LIBRAW_SUCCESS) {
        rawProcessor->recycle();
        std::stringstream error; error << "LibRaw-error while unpacking rawFile: " << libraw_strerror(ret);
        throw std::runtime_error(error.str());
    }

    // ...and libexiv2
    Exiv2::Image::AutoPtr rawImage;
    try {
        rawImage = Exiv2::ImageFactory::open(filename);
        rawImage->readMetadata();
    } 
    catch (Exiv2::Error& e) {
        std::stringstream error; error << "Exiv2-error while opening/parsing rawFile (code " << e.code() << "): " << e.what();
        throw std::runtime_error(error.str());
    }

    // Identify and create correct processor class
    if (rawProcessor->imgdata.idata.dng_version != 0) {
        try {return new DNGprocessor(host, filename);}
        catch (dng_exception &e) {
            std::stringstream error; error << "Cannot parse source DNG-file (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
            throw std::runtime_error(error.str());
        }
    }
    else if (!strcmp(rawProcessor->imgdata.idata.model, "ILCE-7"))
        return new ILCE7processor(host, filename, rawImage, rawProcessor.Release());
    else if (!strcmp(rawProcessor->imgdata.idata.make, "FUJIFILM"))
        return new FujiProcessor(host, filename, rawImage, rawProcessor.Release());

    return new VariousVendorProcessor(host, filename, rawImage, rawProcessor.Release());
}


void NegativeProcessor::embedOriginalFile(const char *rawFilename) {
    #define BLOCKSIZE 65536 // as per spec

    // Open input/output streams and write header with empty indices
    dng_file_stream rawDataStream(rawFilename);
    rawDataStream.SetReadPosition(0);

    uint32 rawFileSize = static_cast<uint32>(rawDataStream.Length());
    uint32 numberRawBlocks = static_cast<uint32>(floor((rawFileSize + 65535.0) / 65536.0));

    dng_memory_stream embeddedRawStream(m_host->Allocator());
    embeddedRawStream.SetBigEndian(true);
    embeddedRawStream.Put_uint32(rawFileSize);
    for (uint32 block = 0; block < numberRawBlocks; block++) 
        embeddedRawStream.Put_uint32(0);  // indices for the block-offsets
    embeddedRawStream.Put_uint32(0);  // index to next data fork

    uint32 indexOffset = 1 * sizeof(uint32);
    uint32 dataOffset = (numberRawBlocks + 1 + 1) * sizeof(uint32);

    for (uint32 block = 0; block < numberRawBlocks; block++) {

        // Read and compress one 64k block of data
        z_stream zstrm;
        zstrm.zalloc = Z_NULL;
        zstrm.zfree = Z_NULL;
        zstrm.opaque = Z_NULL;
        if (deflateInit(&zstrm, Z_DEFAULT_COMPRESSION) != Z_OK) 
            throw std::runtime_error("Error initialising ZLib for embedding raw file!");

        unsigned char inBuffer[BLOCKSIZE], outBuffer[BLOCKSIZE * 2];
        uint32 currentRawBlockLength = 
            static_cast<uint32>(std::min(static_cast<uint64>(BLOCKSIZE), rawFileSize - rawDataStream.Position()));
        rawDataStream.Get(inBuffer, currentRawBlockLength);
        zstrm.avail_in = currentRawBlockLength;
        zstrm.next_in = inBuffer;
        zstrm.avail_out = BLOCKSIZE * 2;
        zstrm.next_out = outBuffer;
        if (deflate(&zstrm, Z_FINISH) != Z_STREAM_END)
            throw std::runtime_error("Error compressing chunk for embedding raw file!");

        uint32 compressedBlockLength = zstrm.total_out;
        deflateEnd(&zstrm);

        // Write index and data
        embeddedRawStream.SetWritePosition(indexOffset);
        embeddedRawStream.Put_uint32(dataOffset);
        indexOffset += sizeof(uint32);

        embeddedRawStream.SetWritePosition(dataOffset);
        embeddedRawStream.Put(outBuffer, compressedBlockLength);
        dataOffset += compressedBlockLength;
    }

    embeddedRawStream.SetWritePosition(indexOffset);
    embeddedRawStream.Put_uint32(dataOffset);

    // Write 7 "Mac OS forks" as per spec - empty for us
    embeddedRawStream.SetWritePosition(dataOffset);
    embeddedRawStream.Put_uint32(0);
    embeddedRawStream.Put_uint32(0);
    embeddedRawStream.Put_uint32(0);
    embeddedRawStream.Put_uint32(0);
    embeddedRawStream.Put_uint32(0);
    embeddedRawStream.Put_uint32(0);
    embeddedRawStream.Put_uint32(0);

    AutoPtr<dng_memory_block> block(embeddedRawStream.AsMemoryBlock(m_host->Allocator()));
    m_negative->SetOriginalRawFileData(block);
    m_negative->FindOriginalRawFileDigest();
}
