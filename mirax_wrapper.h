#pragma once
#include <stdint.h>

#include "gdcmDataSet.h"
#include "gdcmUIDGenerator.h"

struct Point
{
	int x;
	int y;

	Point(int x, int y) { this->x = x; this->y = y; }
};

struct Pointf
{
	double x;
	double y;

	Pointf() { x = 0; y = 0; }
	Pointf(double x, double y) { this->x = x; this->y = y; }
};

struct TileInfo
{
	int left;
	int top;
	int right;
	int bottom;
	unsigned int row;
	unsigned int col;
	Pointf slide_offset;

	TileInfo(int left, int top, int right, int bottom, unsigned int row, unsigned int col) 
	{ 
		this->left = left;
		this->top = top;
		this->right = right;
		this->bottom = bottom;
		this->row = row;
		this->col = col;
	}

	int Width() { return right - left + 1; }
	int Height() { return bottom - top + 1; }
};

struct PyramidLevel
{
	int width;
	int height;
	double downsample;
	double pixel_spacing_x;
	double pixel_spacing_y;
	Pointf total_matrix_origin;
	unsigned int fill_color_bgr;

	PyramidLevel(int x, int y, double down) 
	{ 
		width = x; 
		height = y; 
		downsample = down;
		// row spacing mm
		pixel_spacing_y = 0.0181159414;
		// col spacing mm
		pixel_spacing_x = 0.00374999992;
		// total pixel matrix origin mm
		total_matrix_origin = Pointf(20.0, 40.0);
		fill_color_bgr = 0x00ffffff;
	}
	
	std::vector<TileInfo> GetTiles(int tile_w, int tile_h)
	{
		std::vector<TileInfo> tiles;
		for (int y = 0, row = 0; y < height; y += tile_h, ++row)
			for (int x = 0, col = 0; x < width; x += tile_w, ++col)
			{
				TileInfo tile(x, y, x + tile_w - 1, y + tile_h - 1, row, col);
				tile.slide_offset.x = total_matrix_origin.x - col*tile_w*pixel_spacing_x;
				tile.slide_offset.y = total_matrix_origin.y - row*tile_h*pixel_spacing_y;
				tiles.push_back(tile);
			}


		return tiles;
	}
};

class MiraxWrapper
{
public:
	MiraxWrapper();
	~MiraxWrapper();

	bool Open(const char * filename);

	uint16_t GetTileW() { return tile_width; }
	uint16_t GetTileH() { return tile_height; }
	int GetLevelCount() { return level_count; }
	int GetTileCount(int level);

	bool WriteDicomFile(const char * filename, int level);
	bool FillWholslideImageModule(gdcm::DataSet & ds, int level);
	bool FillVariables(gdcm::DataSet & ds);
	bool GetTileData(std::vector<uint8_t> & outbuf, int level, int tile);
	bool Multiframe(gdcm::DataSet & ds);
	bool MiraxWrapper::AddPerFrameFunctionalGroups(gdcm::DataSet & ds, int level);

private:
	uint16_t tile_width;
	uint16_t tile_height;
	int level_count;
	openslide_t * openslide_handle;
	std::vector<PyramidLevel> pyramid;
	gdcm::UIDGenerator uidgen;
	int slice_thickness;
	gdcm::PixelFormat pixel_format;
	gdcm::TransferSyntax transfer_syntax;
	int x_offset_slide;
	int y_offset_slide;
	// Для каждой mirax-картинки назначается пара Series/Instance uid
	std::string study_instance_uid;
	std::string series_instance_uid;

};

