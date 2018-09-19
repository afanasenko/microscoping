// writer.cpp : 
//
#include "stdafx.h"

#include <gdcmFile.h>
#include <gdcmImage.h>
#include <gdcmTag.h>
#include "gdcmDataElement.h"
#include "gdcmByteValue.h"
#include "gdcmPixelFormat.h"
#include "gdcmImageWriter.h"
#include "gdcmAnonymizer.h"

#include "gdcmImage.h"
#include "gdcmImageWriter.h"
#include "gdcmFileDerivation.h"
#include "gdcmUIDGenerator.h"


bool ConvertArgbToRgb(char const *argb, int width, int height, std::vector<char> &ret)
{
	int bpp = 3;
	ret.resize(width*height*bpp);

	int src_len = width * height * 4;
	printf("%d\n", src_len);
	for (int i = 0, j = 0; i < src_len; i += 4, j += 3)
	{
		ret[j] = argb[1];
		ret[j + 1] = argb[2];
		ret[j + 2] = argb[3];
		argb += 4;
	}
	printf("Done\n");
	return true;
}

bool CreateTestImage(gdcm::SmartPointer<gdcm::Image> im, int width, int height)
{
	im->SetNumberOfDimensions(2);
	im->SetDimension(0, width);
	im->SetDimension(1, height);
	int bpp = 3;

	im->GetPixelFormat().SetSamplesPerPixel(bpp);
	im->SetPhotometricInterpretation(gdcm::PhotometricInterpretation::RGB);

	unsigned long len = im->GetBufferLength();
	if (len != width * height * bpp)
	{
		return false;
	}

	std::vector<char> buffer(width * height * bpp);
	char * p = buffer.data();
	int b = 128;

	int rgb[3];

	for (int r = 0; r < width; ++r)
		for (int g = 0; g < height; ++g)
		{
			rgb[0] = 128;
			rgb[1] = g % 0xff;
			rgb[2] = b;

			*p++ = (char)rgb[0];
			*p++ = (char)rgb[1];
			*p++ = (char)rgb[2];
		}

	gdcm::DataElement pixeldata(gdcm::Tag(0x7fe0, 0x0010));
	pixeldata.SetByteValue((const char*)buffer.data(), (uint32_t)len);

	im->SetDataElement(pixeldata);
	return true;
}

bool CreateImage(gdcm::SmartPointer<gdcm::Image> im, uint32_t const *buffer, int w, int h)
{

	im->SetNumberOfDimensions(2);
	im->SetDimension(0, w);
	im->SetDimension(1, h);

	im->GetPixelFormat().SetSamplesPerPixel(3);
	im->SetPhotometricInterpretation(gdcm::PhotometricInterpretation::RGB);

	unsigned long len = im->GetBufferLength();
	if (len != w * h * 3)
	{
		return false;
	}

	gdcm::DataElement pixeldata(gdcm::Tag(0x7fe0, 0x0010));
	pixeldata.SetByteValue((const char*)buffer, (uint32_t)len);

	im->SetDataElement(pixeldata);

	return true;
}

/*
* This example shows two things:
* 1. How to create an image ex-nihilo
* 2. How to use the gdcm.FileDerivation filter. This filter is meant to create "DERIVED" image
* object. FileDerivation has a simple API where you can reference *all* the input image that have been
* used to generate the image. The API also allows user to specify the purpose of reference (see CID 7202,
* PS 3.16 - 2008), and the image derivation type (CID 7203, PS 3.16 - 2008).
*/
bool WriterTest(uint32_t const *buffer, int width, int height)
{
	gdcm::SmartPointer<gdcm::Image> im = new gdcm::Image;
	if (!CreateImage(im, buffer, width, height))
	//if (!CreateTestImage(im, width, height))
	{
		std::cerr << "Create image failed" << std::endl;
		return false;
	}

	gdcm::UIDGenerator uid; // helper for uid generation

	gdcm::SmartPointer<gdcm::File> file = new gdcm::File; // empty file

														  // Step 2: DERIVED object
	gdcm::FileDerivation fd;
	// For the pupose of this execise we will pretend that this image is referencing
	// two source image (we need to generate fake UID for that).
	const char sopclassuid [] = "1.2.840.10008.5.1.4.1.1.77.1.2"; // VL Microscopic Image Storage
	auto sopinstanceuid = uid.Generate();

	fd.AddReference(sopclassuid, sopinstanceuid);


	// Again for the purpose of the exercise we will pretend that the image is a
	// multiplanar reformat (MPR):
	// CID 7202 Source Image Purposes of Reference
	// {"DCM",121322,"Source image for image processing operation"},
	fd.SetPurposeOfReferenceCodeSequenceCodeValue(121322);
	// CID 7203 Image Derivation
	// { "DCM",113072,"Multiplanar reformatting" },
	fd.SetDerivationCodeSequenceCodeValue(113072);
	fd.SetFile(*file);
	// If all Code Value are ok the filter will execute properly
	if (!fd.Derive())
	{
		std::cerr << "Sorry could not derive using input info" << std::endl;
		return false;
	}

	// We pass both :
	// 1. the fake generated image
	// 2. the 'DERIVED' dataset object
	// to the writer.
	gdcm::ImageWriter w;
	w.SetImage(*im);
	w.SetFile(fd.GetFile());

	// Set the filename:
	w.SetFileName("ybr2.dcm");
	if (!w.Write())
	{
		return true;
	}

	return false;
}