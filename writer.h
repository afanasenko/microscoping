#pragma once

#include "gdcmPixelFormat.h"
#include "gdcmPhotometricInterpretation.h"

bool ConvertArgbToRgb(char const *argb, int width, int height, std::vector<char> &ret, size_t maxlen);

bool IsEmptyBuf(std::vector<char> const & buf);

void WriteRawHeader(std::ostream * of);

bool WriteRawSlice(
	const char * buf,
	const std::size_t & buf_size,
	std::ofstream * of,
	gdcm::PixelFormat pixelInfo,
	const gdcm::TransferSyntax & ts,
	unsigned int slice_width,
	unsigned int slice_height,
	gdcm::PhotometricInterpretation const & pi
);

