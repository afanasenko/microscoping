#pragma once

#include "gdcmPixelFormat.h"
#include "gdcmPhotometricInterpretation.h"

bool ConvertArgbToRgb(const uint8_t * argb, int width, int height, std::vector<uint8_t> & ret, uint32_t fill_color);

void WriteRawHeader(std::ostream * of);

bool WriteRawSlice(
	const uint8_t * buf,
	const std::size_t & buf_size,
	std::ofstream * of,
	gdcm::PixelFormat pixelInfo,
	const gdcm::TransferSyntax & ts,
	unsigned int slice_width,
	unsigned int slice_height,
	gdcm::PhotometricInterpretation const & pi
);

