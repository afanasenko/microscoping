// mirax_writer.cpp : 
//

#include "stdafx.h"

#include "openslide.h"
#include <string>
#include <fstream>
#include <vector>
#include <time.h>

#include <gdcmFile.h>
#include <gdcmImage.h>
#include <gdcmTag.h>

#include "gdcmByteValue.h"
#include "gdcmPixelFormat.h"
#include "gdcmImageWriter.h"
#include "gdcmAnonymizer.h"
#include "gdcmStreamImageWriter.h"

#include "gdcmImage.h"
#include "gdcmImageWriter.h"

#include "writer.h"
#include "mirax_wrapper.h"
#include "iccprofile.h"
#include "dicom_helpers.h"

MiraxWrapper::MiraxWrapper()
	: tile_width(512), 
	tile_height(512), 
	level_count(0), 
	openslide_handle(nullptr),
	slice_thickness(1),
	pixel_format(3, 8, 8, 7, 0),
	transfer_syntax(gdcm::TransferSyntax::ExplicitVRLittleEndian),//implicit?
	x_offset_slide(20),
	y_offset_slide(40)
{
	
}

MiraxWrapper::~MiraxWrapper()
{
	if (openslide_handle)
		openslide_close(openslide_handle);
}

bool MiraxWrapper::Open(const char * filename)
{
	openslide_handle = openslide_open(filename);

	if (!openslide_handle)
	{
		printf("Error reading file %s\n", filename);
		return false;
	}

	level_count = openslide_get_level_count(openslide_handle);

	for (int lvl = 0; lvl < level_count; ++lvl)
	{
		int64_t width, height;
		openslide_get_level_dimensions(openslide_handle, lvl, &width, &height);
		double downsample = openslide_get_level_downsample(openslide_handle, lvl);
		/*
		std::map<std::string, std::string> properties;

		auto s = openslide_get_property_names(openslide_handle);
		for (int i = 0; i < 1000; ++i)
		{
			try
			{
				printf("%s\n", s[i]);
			}
			catch (...) 
			{
				break;
			}
		}
		*/
		char propname[128];
		sprintf(propname, "mirax.LAYER_0_LEVEL_%d_SECTION.MICROMETER_PER_PIXEL_X", lvl);
		const char * spacing_x = openslide_get_property_value(openslide_handle, propname);
		sprintf(propname, "mirax.LAYER_0_LEVEL_%d_SECTION.MICROMETER_PER_PIXEL_Y", lvl);
		const char * spacing_y = openslide_get_property_value(openslide_handle, propname);

		PyramidLevel level_info((int)width, (int)height, downsample);
		// micrometers to millimeters
		if (spacing_x)
			level_info.pixel_spacing_x = 0.001 * atof(spacing_x);
		if(spacing_y)
			level_info.pixel_spacing_y = 0.001 * atof(spacing_y);

		sprintf(propname, "mirax.LAYER_0_LEVEL_%d_SECTION.IMAGE_FILL_COLOR_BGR", lvl);
		const char * fill_color = openslide_get_property_value(openslide_handle, propname);
		if (fill_color)
			level_info.fill_color_bgr = atoi(fill_color);

		pyramid.push_back(PyramidLevel((int)width, (int)height, downsample));
	}

	study_instance_uid = uidgen.Generate();
	series_instance_uid = uidgen.Generate();

	return true;
}

bool MiraxWrapper::GetTileData(std::vector<uint8_t> & outbuf, int level, int tile)
{
	auto tiles = pyramid[level].GetTiles(tile_width, tile_height);

	double zoom = pyramid[level].downsample;

	std::vector<uint32_t> argb_buf(tile_width*tile_height);
	openslide_read_region(
		openslide_handle,
		argb_buf.data(),
		static_cast<uint64_t>(zoom * tiles[tile].left),
		static_cast<uint64_t>(zoom * tiles[tile].top),
		level,
		tile_width, tile_height
	);

	ConvertArgbToRgb((uint8_t*)argb_buf.data(), tile_width, tile_height, outbuf, pyramid[level].fill_color_bgr);

	return true;
}

bool InsertSequence(gdcm::DataSet & ds, gdcm::SmartPointer<gdcm::SequenceOfItems> & sq, uint16_t group, uint16_t elem)
{
	sq->SetLengthToUndefined();
	gdcm::DataElement dicomel(gdcm::Tag(group, elem));
	dicomel.SetVR(gdcm::VR::SQ);
	dicomel.SetValue(*sq);
	dicomel.SetVLToUndefined();
	ds.Insert(dicomel);
	return true;
}

bool InsertSequence(gdcm::Item & item, gdcm::SmartPointer<gdcm::SequenceOfItems> & sq, uint16_t group, uint16_t elem)
{
	gdcm::DataSet & ds = item.GetNestedDataSet();
	return InsertSequence(ds, sq, group, elem);
}

bool MiraxWrapper::AddPerFrameFunctionalGroups(gdcm::DataSet & ds, int level)
{
	// shared functional group sequence
	gdcm::SmartPointer<gdcm::SequenceOfItems> sfsq = new gdcm::SequenceOfItems();
	InsertSequence(ds, sfsq, 0x5200, 0x9229);

	gdcm::Item shared_item;
	gdcm::DataSet &shared_item_ds = shared_item.GetNestedDataSet();
	// pixel measure sequence

	gdcm::SmartPointer<gdcm::SequenceOfItems> measureseq = new gdcm::SequenceOfItems();
	InsertSequence(shared_item_ds, measureseq, 0x28, 0x9110);

	gdcm::Item pxmeas_item;
	gdcm::DataSet &pxmeas_item_ds = pxmeas_item.GetNestedDataSet();

	// slice thickness
	pxmeas_item_ds.Insert(DicomDecimalString(0x18, 0x50, slice_thickness));

	// pixel spacing row/col
	char spacing_buf[64];
	sprintf(spacing_buf, "%f\\%f", pyramid[level].pixel_spacing_y, pyramid[level].pixel_spacing_x);
	pxmeas_item_ds.Insert(DicomDecimalString(0x28, 0x30, spacing_buf));
	measureseq->AddItem(pxmeas_item);

	//optical path identifier sequence
	gdcm::SmartPointer<gdcm::SequenceOfItems> opaseq = new gdcm::SequenceOfItems();
	InsertSequence(shared_item_ds, opaseq, 0x48, 0x207);

	gdcm::Item opath_item;
	gdcm::DataSet &opath_item_ds = opath_item.GetNestedDataSet();

	// optical path identifier
	gdcm::DataElement opid(gdcm::Tag(0x48, 0x106));
	opid.SetVR(gdcm::VR::SH);
	std::string v = "1";
	opid.SetByteValue(v.c_str(), (uint32_t)v.size());
	opath_item_ds.Insert(opid);
	
	opaseq->AddItem(opath_item);
	sfsq->AddItem(shared_item);

	auto tiles = pyramid[level].GetTiles(tile_width, tile_height);
	// per-Frame function group sequence

	gdcm::SmartPointer<gdcm::SequenceOfItems> pfsq = new gdcm::SequenceOfItems();
	pfsq->SetLengthToUndefined();
	gdcm::DataElement perframe(gdcm::Tag(0x5200, 0x9230));
	perframe.SetVR(gdcm::VR::SQ);
	perframe.SetValue(*pfsq);
	perframe.SetVLToUndefined();
	ds.Insert(perframe);

	for (auto tile = tiles.begin(); tile != tiles.end(); ++tile)
	{
		gdcm::Item parent_item;
		gdcm::DataSet &pds = parent_item.GetNestedDataSet();

		// Frame content sequence
		gdcm::SmartPointer<gdcm::SequenceOfItems> framecontseq = new gdcm::SequenceOfItems();
		framecontseq->SetLengthToUndefined();
		gdcm::DataElement framecont(gdcm::Tag(0x20, 0x9111));
		framecont.SetVR(gdcm::VR::SQ);
		framecont.SetValue(*framecontseq);
		framecont.SetVLToUndefined();
		pds.Insert(framecont);

		gdcm::Item it;
		gdcm::DataSet &nds = it.GetNestedDataSet();
		// dimension index values
		gdcm::Attribute<0x20, 0x9157> index_val;
		uint32_t pos[2] = { tile->col + 1, tile->row + 1 };
		index_val.SetValues(pos, 2);
		nds.Insert(index_val.GetAsDataElement());
		framecontseq->AddItem(it);

		// Plane position sequence
		gdcm::SmartPointer<gdcm::SequenceOfItems> planeposseq = new gdcm::SequenceOfItems();
		planeposseq->SetLengthToUndefined();

		gdcm::DataElement planepos(gdcm::Tag(0x48, 0x21a));
		planepos.SetVR(gdcm::VR::SQ);
		planepos.SetValue(*planeposseq);
		planepos.SetVLToUndefined();
		pds.Insert(planepos);

		gdcm::Item item2;
		gdcm::DataSet &item2_ds = item2.GetNestedDataSet();

		// X, Y, Z offset in slide coordinate system
		item2_ds.Insert(DicomDecimalString(0x40, 0x72a, tile->slide_offset.x));
		item2_ds.Insert(DicomDecimalString(0x40, 0x73a, tile->slide_offset.y));
		item2_ds.Insert(DicomDecimalString(0x40, 0x74a, 0.0f));

		gdcm::Attribute<0x48, 0x21e> colpos = { tile->left + 1 };
		item2_ds.Insert(colpos.GetAsDataElement());
		gdcm::Attribute<0x48, 0x21f> rowpos = { tile->top + 1 };
		item2_ds.Insert(rowpos.GetAsDataElement());

		planeposseq->AddItem(item2);

		pfsq->AddItem(parent_item);
	}

	// Optical path sequence
	gdcm::SmartPointer<gdcm::SequenceOfItems> opthseq = new gdcm::SequenceOfItems();
	InsertSequence(ds, opthseq, 0x48, 0x105);

	gdcm::Item opth0;
	gdcm::DataSet &opth0_ds = opth0.GetNestedDataSet();

	//Illumination type code sequence
	gdcm::SmartPointer<gdcm::SequenceOfItems> illuseq = new gdcm::SequenceOfItems();
	InsertSequence(opth0_ds, illuseq, 0x22, 0x16);

	gdcm::Item illum0;
	gdcm::DataSet &illum0_ds = illum0.GetNestedDataSet();

	illum0_ds.Insert(DicomAnyString(0x08, 0x100, gdcm::VR::SH, "111744"));
	illum0_ds.Insert(DicomAnyString(0x08, 0x102, gdcm::VR::SH, "DCM"));
	illum0_ds.Insert(DicomAnyString(0x08, 0x104, gdcm::VR::LO, "Brightfield illumination"));

	illuseq->AddItem(illum0);

	// ICC profile
	gdcm::DataElement iccprof(gdcm::Tag(0x28, 0x2000));
	iccprof.SetVR(gdcm::VR::OB);
	iccprof.SetByteValue((char*)icc_profile, sizeof(icc_profile));
	opth0_ds.Insert(iccprof);

	// optical path identifier
	gdcm::DataElement opthid(gdcm::Tag(0x48, 0x106));
	opthid.SetVR(gdcm::VR::SH);
	opthid.SetByteValue(v.c_str(), (uint32_t)v.size());
	opth0_ds.Insert(opthid);
	// optical path description
	gdcm::DataElement opdesc(gdcm::Tag(0x48, 0x107));
	opdesc.SetVR(gdcm::VR::ST);
	char desc[] = "Brightfield";
	opdesc.SetByteValue(desc, (uint32_t)strlen(desc));
	opth0_ds.Insert(opdesc);

	// Illumination Color Code Sequence
	gdcm::SmartPointer<gdcm::SequenceOfItems> colorseq = new gdcm::SequenceOfItems();
	InsertSequence(opth0_ds, colorseq, 0x48, 0x108);

	gdcm::Item color0;
	gdcm::DataSet &color0_ds = color0.GetNestedDataSet();

	color0_ds.Insert(DicomAnyString(0x08, 0x100, gdcm::VR::SH, "R-102C0"));
	color0_ds.Insert(DicomAnyString(0x08, 0x102, gdcm::VR::SH, "SRT"));
	color0_ds.Insert(DicomAnyString(0x08, 0x104, gdcm::VR::LO, "Full Spectrum"));

	colorseq->AddItem(color0);

	opthseq->AddItem(opth0);

	return true;
}

bool MiraxWrapper::Multiframe(gdcm::DataSet & ds)
{
	const char *dimension_uid = uidgen.Generate();

	//Dimension organization sequence
	gdcm::Item it;
	gdcm::DataSet &nds = it.GetNestedDataSet();
	nds.Insert(DicomAnyString(0x20, 0x9164, gdcm::VR::UI, dimension_uid, false));

	gdcm::SmartPointer<gdcm::SequenceOfItems> sq1 = new gdcm::SequenceOfItems();
	sq1->SetLengthToUndefined();

	gdcm::DataElement dimorg(gdcm::Tag(0x20, 0x9221));
	dimorg.SetVR(gdcm::VR::SQ);
	dimorg.SetValue(*sq1);
	dimorg.SetVLToUndefined();
	ds.Insert(dimorg);
	sq1->AddItem(it);

	//Dimension index sequence
	gdcm::Item inditem0;
	gdcm::DataSet &ds21 = inditem0.GetNestedDataSet();
	ds21.Insert(DicomAnyString(0x20, 0x9164, gdcm::VR::UI, dimension_uid, false));

	ds21.Insert(DicomAttribTag(0x20, 0x9165, 0x48, 0x21e));
	ds21.Insert(DicomAttribTag(0x20, 0x9167, 0x48, 0x21a));

	gdcm::Item inditem1;
	gdcm::DataSet &ds22 = inditem1.GetNestedDataSet();
	ds22.Insert(DicomAnyString(0x20, 0x9164, gdcm::VR::UI, dimension_uid, false));

	ds22.Insert(DicomAttribTag(0x20, 0x9165, 0x48, 0x21f));
	ds22.Insert(DicomAttribTag(0x20, 0x9167, 0x48, 0x21a));

	gdcm::SmartPointer<gdcm::SequenceOfItems> sq2 = new gdcm::SequenceOfItems();
	sq2->SetLengthToUndefined();

	gdcm::DataElement dimidx(gdcm::Tag(0x20, 0x9222));
	dimidx.SetVR(gdcm::VR::SQ);
	dimidx.SetValue(*sq2);
	dimidx.SetVLToUndefined();
	ds.Insert(dimidx);
	sq2->AddItem(inditem0);
	sq2->AddItem(inditem1);

	return true;
}

bool MiraxWrapper::FillWholslideImageModule(gdcm::DataSet & ds, int level)
{
	auto tiles = pyramid[level].GetTiles(tile_width, tile_height);

	// SOP Instance UID
	const char *sopuid = uidgen.Generate();
	ds.Insert(DicomAnyString(0x08, 0x18, gdcm::VR::UI, sopuid, false));

	//SOP Class UID 
	gdcm::DataElement de1(gdcm::Tag(0x8, 0x16));
	de1.SetVR(gdcm::VR::UI);
	gdcm::MediaStorage ms(gdcm::MediaStorage::VLWholeSlideMicroscopyImageStorage);
	de1.SetByteValue(ms.GetString(), (uint32_t)strlen(ms.GetString()));
	ds.Insert(de1);

	// Charset - should be ISO_IR 144 for Cyrillic
	ds.Insert(DicomCodeString(0x08, 0x05, "ISO_IR 144"));

	//Modality = General Microscopy
	ds.Insert(DicomCodeString(0x08, 0x60, "SM"));

	//Instance number == level number 
	ds.Insert(DicomIntegerString(0x20, 0x13, level));

	// Volumetric properties (VOLUME or SAMPLED)
	ds.Insert(DicomCodeString(0x08, 0x9206, "VOLUME"));

	//Photometric Interpretation
	ds.Insert(DicomCodeString(0x28, 0x04, "RGB"));

	//Image height
	gdcm::Attribute<0x28, 0x10> rows = { tile_width };
	ds.Insert(rows.GetAsDataElement());

	//Image width
	gdcm::Attribute<0x28, 0x11> cols = { tile_height };
	ds.Insert(cols.GetAsDataElement());

	// Number of frames == number of tiles
	gdcm::Attribute<0x28, 0x08> lvl_e = { (uint16_t)tiles.size() };
	ds.Insert(lvl_e.GetAsDataElement());

	//Samples per pixel
	gdcm::Attribute<0x28, 0x02> spp = { pixel_format.GetSamplesPerPixel() };
	ds.Insert(spp.GetAsDataElement());

	//Bits allocated
	gdcm::Attribute<0x0028, 0x0100> bitsa = { pixel_format.GetBitsAllocated() };
	ds.Insert(bitsa.GetAsDataElement());

	//Bits stored
	gdcm::Attribute<0x0028, 0x0101> bitss = { pixel_format.GetBitsStored() };
	ds.Insert(bitss.GetAsDataElement());

	//High bit
	gdcm::Attribute<0x0028, 0x0102> hbit = { pixel_format.GetHighBit() };
	ds.Insert(hbit.GetAsDataElement());

	//Pixel representation (set to unsigned)
	gdcm::Attribute<0x0028, 0x0103> pxrepr = { pixel_format.GetPixelRepresentation() };
	ds.Insert(pxrepr.GetAsDataElement());

	//Planar configuration (set to color-by-pixel)
	gdcm::Attribute<0x28, 0x06> pcon = { 0 };
	ds.Insert(pcon.GetAsDataElement());

	// Lossy image compression
	ds.Insert(DicomCodeString(0x28, 0x2110, "00"));

	//#############################

	//Image type
	ds.Insert(DicomCodeString(0x08, 0x08, "ORIGINAL\\PRIMARY"));
	//ds.Insert(DicomCodeString(0x08, 0x08, "ORIGINAL\\PRIMARY\\VOLUME\\NONE"));

	//Volume width
	gdcm::Attribute<0x48, 0x01> volw = { 15.0f };
	ds.Insert(volw.GetAsDataElement());

	//Volume height
	gdcm::Attribute<0x48, 0x02> volh = { 15.0f };
	ds.Insert(volh.GetAsDataElement());

	//Volume depth
	gdcm::Attribute<0x48, 0x03> vold = { 1.0f };
	ds.Insert(vold.GetAsDataElement());

	//Total pixel matrix columns
	gdcm::Attribute<0x48, 0x06> totalw = { (uint32_t)pyramid[level].width };
	ds.Insert(totalw.GetAsDataElement());

	//Total pixel matrix rows
	gdcm::Attribute<0x48, 0x07> totalh = { (uint32_t)pyramid[level].height };
	ds.Insert(totalh.GetAsDataElement());

	//Extended depth of field (means multiple focal planes)
	ds.Insert(DicomCodeString(0x48, 0x12, "NO"));

	//Total pixel matrix origin sequence
	gdcm::Item it;
	gdcm::DataSet &nds = it.GetNestedDataSet();
	//x offset in slide coordinate system
	nds.Insert(DicomDecimalString(0x40, 0x72a, x_offset_slide));
	//y offset in slide coordinate system
	nds.Insert(DicomDecimalString(0x40, 0x73a, y_offset_slide));

	gdcm::SmartPointer<gdcm::SequenceOfItems> sq = new gdcm::SequenceOfItems();
	sq->SetLengthToUndefined();

	gdcm::DataElement dicomel(gdcm::Tag(0x48, 0x08));
	dicomel.SetVR(gdcm::VR::SQ);
	dicomel.SetValue(*sq);
	dicomel.SetVLToUndefined();

	ds.Insert(dicomel);
	sq->AddItem(it);

	// Image orientation
	ds.Insert(DicomDecimalString(0x48, 0x102, "0\\-1\\0\\-1\\0\\0"));

	if (!Multiframe(ds))
		return false;

	if (!AddPerFrameFunctionalGroups(ds, level))
		return false;

	return true;
}

bool MiraxWrapper::FillVariables(gdcm::DataSet & ds)
{
	time_t curtime;
	time(&curtime);

	// Patient name
	ds.Insert(DicomAnyString(0x10, 0x10, gdcm::VR::PN, "Ivanov"));
	// Patient ID
	const char patid[] = "803e1b7d-e291-4b40-aee0-cd07368a2fd5";
	ds.Insert(DicomAnyString(0x10, 0x20, gdcm::VR::LO, patid));
	// Patient birthday
	ds.Insert(DicomAnyString(0x10, 0x30, gdcm::VR::DA, "19820423"));
	ds.Insert(DicomCodeString(0x10, 0x40, "M"));
	ds.Insert(DicomCodeString(0x20, 0x20, ""));

	char da_str[64];
	strftime(da_str, 64, "%Y%m%d", gmtime(&curtime));
	char tm_str[64];
	strftime(tm_str, 64, "%H%M%S.000000 ", gmtime(&curtime));

	// Study date, time and ID
	ds.Insert(DicomAnyString(0x08, 0x20, gdcm::VR::DA, da_str));
	ds.Insert(DicomAnyString(0x08, 0x30, gdcm::VR::TM, tm_str));

	// Study Instance UID
	printf("study %s\n", study_instance_uid.c_str());
	printf("series %s\n", series_instance_uid.c_str());
	ds.Insert(DicomAnyString(0x20, 0x0d, gdcm::VR::UI, study_instance_uid.c_str(), false));

	// Series date, time and ID
	ds.Insert(DicomAnyString(0x08, 0x21, gdcm::VR::DA, da_str));
	ds.Insert(DicomAnyString(0x08, 0x31, gdcm::VR::TM, tm_str));

	// Series Instance UID
	ds.Insert(DicomAnyString(0x20, 0x0e, gdcm::VR::UI, series_instance_uid.c_str(), false));

	// Acquisition datetime
	gdcm::DataElement acqdt(gdcm::Tag(0x08, 0x2a));
	acqdt.SetVR(gdcm::VR::DT);
	char dtm_str[64];
	strftime(dtm_str, 64, "%Y%m%d%H%M%S.000000 ", gmtime(&curtime));
	acqdt.SetByteValue(dtm_str, (uint32_t)strlen(dtm_str));
	ds.Insert(acqdt);

	//Content date
	ds.Insert(DicomAnyString(0x08, 0x23, gdcm::VR::DA, da_str));

	return true;
}

bool MiraxWrapper::WriteDicomFile(const char * filename, int level)
{
	gdcm::StreamImageWriter stream_writer;

	std::ofstream of;
	of.open(filename, std::ios::out | std::ios::binary);
	stream_writer.SetStream(of);

	gdcm::Writer w;
	gdcm::File &file = w.GetFile();
	gdcm::DataSet &ds = file.GetDataSet();
	file.GetHeader().SetDataSetTransferSyntax(transfer_syntax);

	if (!FillWholslideImageModule(ds, level))
	{
		printf("FillWholslideImageModule failed\n");
		return false;
	}

	if (!FillVariables(ds))
	{
		printf("FillVariables failed\n");
		return false;
	}

	stream_writer.SetFile(file);

	if (!stream_writer.WriteImageInformation()) {
		printf("WriteImageInformation failed\n");
		return false;
	}

	//запись изображений 
	auto tiles = pyramid[level].GetTiles(tile_width, tile_height);

	//
	WriteRawHeader(&of);
	std::vector<uint8_t> tilebuf;
	for (int ntile = 0; ntile < tiles.size(); ++ntile)
	{
		if (!GetTileData(tilebuf, level, ntile))
		{
			printf("tile %d extraction error\n", ntile);
			return false;
		}

		printf("tile %d, %zd bytes\n", ntile, tilebuf.size());

		if (!WriteRawSlice(
			tilebuf.data(),
			tilebuf.size(),
			&of,
			pixel_format,
			transfer_syntax,
			tile_height,
			tile_width,
			gdcm::PhotometricInterpretation::RGB
			))
		{
			printf("Failed to write tile %d\n", ntile);
			return false;
		}
	}

	uint16_t final_tags[4] = { 0xfffe, 0xe0dd, 0, 0 };

	assert(of && !of.eof() && of.good());
	of.write((char*)final_tags, sizeof(final_tags));
	of.flush();
	assert(of);	

	return true;
}