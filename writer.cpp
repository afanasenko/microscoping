// writer.cpp : 
//
#include "stdafx.h"

#include <time.h>
#include <algorithm>

#include <gdcmFile.h>
#include <gdcmImage.h>
#include <gdcmTag.h>
#include "gdcmDataElement.h"
#include "gdcmAttribute.h"
#include "gdcmByteValue.h"
#include "gdcmPixelFormat.h"
#include "gdcmImageWriter.h"
#include "gdcmAnonymizer.h"

#include "gdcmImage.h"
#include "gdcmImageWriter.h"
#include "gdcmFileDerivation.h"
#include "gdcmUIDGenerator.h"
#include "gdcmTag.h"
#include "gdcmMediaStorage.h"
#include "gdcmImageHelper.h"
#include "gdcmRAWCodec.h"
#include "gdcmBasicOffsetTable.h"

#include "writer.h"

//-----------------------------------------------------------------------------
// Примитивный конвертер из ARGB32 в RGB24
bool ConvertArgbToRgb(const uint8_t * argb, int width, int height, std::vector<uint8_t> & ret, uint32_t fill_color )
{
	int bpp = 3;
	ret.resize(width*height*bpp);

	uint8_t * bgrfill = (uint8_t*)&fill_color;

	uint8_t * rgb = ret.data();
	for (int i = 0; i < width*height; ++i)
	{
		uint8_t b = argb[0];
		uint8_t g = argb[1];
		uint8_t r = argb[2];
		argb += 4;

		if (b | g | r)
		{
			*rgb++ = b;
			*rgb++ = g;
			*rgb++ = r;
		}
		else
		{
			*rgb++ = bgrfill[0];
			*rgb++ = bgrfill[1];
			*rgb++ = bgrfill[2];
		}

	}
	return true;
}

/// when writing a raw file, we know the full extent, and can just write the first
/// 12 bytes out (the tag, the VR, and the size)
/// when we do compressed files, we'll do it in chunks, as described in
/// 2009-3, part 5, Annex A, section 4.
/// Pass the raw codec so that in the rare case of a bigendian explicit raw,
/// the first 12 bytes written out should still be kosher.
/// returns -1 if there's any failure, or the complete offset (12 bytes)
/// if it works.  Those 12 bytes are then added to the position in order to determine
/// where to write.
void WriteRawHeader(std::ostream * of)
{
	uint16_t firstTag = 0x7fe0;
	uint16_t secondTag = 0x0010;
	uint16_t thirdTag = 0x424f; // OB
	uint16_t fourthTag = 0x0000;
	uint32_t fifthTag = 0xffffffff;

	//basic offset table (empty)
	uint16_t sixthTag = 0xfffe;
	uint16_t seventhTag = 0xe000;
	uint32_t botsize = 0;


	const int theBufferSize = 4 * sizeof(uint16_t) + sizeof(uint32_t) + 2 * sizeof(uint16_t) + sizeof(uint32_t) + botsize;
	char* tmpBuffer1 = new char[theBufferSize];

	memcpy(&(tmpBuffer1[0]), &firstTag, sizeof(uint16_t));
	memcpy(&(tmpBuffer1[sizeof(uint16_t)]), &secondTag, sizeof(uint16_t));
	memcpy(&(tmpBuffer1[2 * sizeof(uint16_t)]), &thirdTag, sizeof(uint16_t));
	memcpy(&(tmpBuffer1[3 * sizeof(uint16_t)]), &fourthTag, sizeof(uint16_t));

	//Addition by Manoj
	memcpy(&(tmpBuffer1[4 * sizeof(uint16_t)]), &fifthTag, sizeof(uint32_t));// Data Element Length 4 bytes

																				// Basic OffSet Tabl with 16 Item Value
	memcpy(&(tmpBuffer1[4 * sizeof(uint16_t) + sizeof(uint32_t)]), &sixthTag, sizeof(uint16_t)); //fffe
	memcpy(&(tmpBuffer1[5 * sizeof(uint16_t) + sizeof(uint32_t)]), &seventhTag, sizeof(uint16_t));//e000
	memcpy(&(tmpBuffer1[6 * sizeof(uint16_t) + sizeof(uint32_t)]), &botsize, sizeof(uint32_t));

	of->write(tmpBuffer1, theBufferSize);
	of->flush();
}

bool WriteRawSlice(
	const uint8_t * buf, 
	const std::size_t & buf_size, 
	std::ofstream * of, 
	gdcm::PixelFormat pixelInfo,
	const gdcm::TransferSyntax & ts,
	unsigned int slice_width,
	unsigned int slice_height,
	gdcm::PhotometricInterpretation const & pi
	)
{
	int bytesPerPixel = pixelInfo.GetPixelSize();

	//set up the codec prior to resetting the file, just in case that affects the way that
	//files are handled by the ImageHelper

	gdcm::RAWCodec theCodec;
	if (!theCodec.CanDecode(ts) || ts == gdcm::TransferSyntax::ExplicitVRBigEndian)
	{
		gdcmErrorMacro("Only RAW for now");
		return false;
	}

	unsigned int d[3] = { slice_height, slice_width, 1 };

	theCodec.SetNeedByteSwap(ts == gdcm::TransferSyntax::ImplicitVRBigEndianPrivateGE);
	theCodec.SetDimensions(d);
	// color - by - pixel
	theCodec.SetPlanarConfiguration(0);
	theCodec.SetPhotometricInterpretation(pi);


	//have to reset the stream to the proper position
	//first, reopen the stream,then the loop should set the right position
	//MM: you have to reopen the stream, by default, the writer closes it each time it writes.
	//std::ostream* theStream = mWriter.GetStreamPtr();//probably going to need a copy of this
													 //to ensure thread safety; if the stream ptr handler gets used simultaneously by different threads,
													 //that would be BAD
													 //tmpBuffer is for a single raster
	assert(of && *of);


	uint16_t NinthTag = 0xfffe;
	uint16_t TenthTag = 0xe000;

	uint32_t sizeTag = slice_width * slice_height * bytesPerPixel;

	const int theBufferSize1 = 2 * sizeof(uint16_t) + sizeof(uint32_t);

	char* tmpBuffer3 = new char[theBufferSize1];
	char* tmpBuffer4 = new char[theBufferSize1];

	try {

		memcpy(&(tmpBuffer3[0]), &NinthTag, sizeof(uint16_t)); //fffe
		memcpy(&(tmpBuffer3[sizeof(uint16_t)]), &TenthTag, sizeof(uint16_t)); //e000

		memcpy(&(tmpBuffer3[2 * sizeof(uint16_t)]), &sizeTag, sizeof(uint32_t));//Item Length
																				//run that through the codec
		if (!theCodec.DecodeBytes(tmpBuffer3, theBufferSize1,
			tmpBuffer4, theBufferSize1)) {
			delete[] tmpBuffer3;
			gdcmErrorMacro("Problems in Header");
			delete[] tmpBuffer4;
			return false;
		}

		of->write(tmpBuffer4, theBufferSize1);
		of->flush();
	}
	catch (...) {
		delete[] tmpBuffer3;
		delete[] tmpBuffer4;
		return false;
	}
	delete[] tmpBuffer3;
	delete[] tmpBuffer4;

	int slice_pitch = slice_width*bytesPerPixel;
	char* tmpBuffer = new char[slice_pitch];
	char* tmpBuffer2 = new char[slice_pitch];
	try {
			for (unsigned int y = 0; y < slice_height; ++y) {
				memcpy(tmpBuffer, buf + y*slice_pitch, slice_pitch);

				if (!theCodec.DecodeBytes(tmpBuffer, slice_pitch,
					tmpBuffer2, slice_pitch)) {
					delete[] tmpBuffer;
					delete[] tmpBuffer2;
					return false;
				}

				//should be appending

				of->write(tmpBuffer2, slice_pitch);
			}
			of->flush();
	}
	catch (std::exception & ex) {
		(void)ex;
		gdcmWarningMacro("Failed to write with ex:" << ex.what());
		delete[] tmpBuffer;
		delete[] tmpBuffer2;
		return false;
	}
	catch (...) {
		gdcmWarningMacro("Failed to write with unknown error.");
		delete[] tmpBuffer;
		delete[] tmpBuffer2;
		return false;
	}
	delete[] tmpBuffer;
	delete[] tmpBuffer2;
	return true;
}

