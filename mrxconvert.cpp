// mrxconvert.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "openslide.h"
#include <string>
#include <fstream>
#include <vector>
#include <Windows.h>
#include "writer.h"

using namespace std;

// Проверка на существование файла
bool fexists(const char *filename) {
  std::ifstream ifile(filename);
  return ifile ? true : false;
}

// Прeобразование Unicode в UTF-8
std::string utf8_encode(const std::wstring &wstr)
{
    if( wstr.empty() ) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Прeобразование UTF-8 в Unicode
std::wstring utf8_decode(const std::string &str)
{
    if( str.empty() ) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

int _tmain(int argc, wchar_t* argv[])
{
	std::string filename;

	bool test_mode = false;
	if (argc > 2)
	{
		std::wstring s(argv[2]);
		if (s == L"-t")
			test_mode = true;
		printf("Test mode on\n");
	}

	if (argc > 1)
	{
		filename = utf8_encode(argv[1]);
		if (!fexists(filename.c_str()))
		{
			printf("File %s does not exist\n", filename.c_str());
			return 1;
		}
	}
	else
	{
		printf("Input file not specified\n");
		return 1;
	}
	
	auto datablock = openslide_open(filename.c_str());
	
	if (!datablock)
	{
		printf("Error reading file %s\n", filename.c_str());
		return 1;
	}

	int num_levels = openslide_get_level_count(datablock);
	printf("Levels: %d\n", num_levels);

	for (int lvl = 0; lvl < num_levels; ++lvl)
	{
		int64_t level_width, level_height;
		openslide_get_level_dimensions(datablock, lvl, &level_width, &level_height);
		printf("Level %d: %d x %d\n", lvl, (int)level_width, (int)level_height);

		if (min(level_width, level_height) < 1000)
		{
			std::vector<uint32_t> buf(level_width*level_height);
			openslide_read_region(
				datablock,
				buf.data(),
				0, 0,
				lvl,
				level_width, level_height
			);

			std::vector<char>rgb_buf;
			printf("writer test\n");
			ConvertArgbToRgb((const char*)buf.data(), level_width, level_height, rgb_buf);
			printf("Converted buffer: %zd bytes\n", rgb_buf.size());

			WriterTest((const uint32_t*)rgb_buf.data(), level_width, level_height);
			break;
		}
	}
	
	return 0;
}

