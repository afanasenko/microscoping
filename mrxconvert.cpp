// mrxconvert.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "openslide.h"
#include <string>
#include <fstream>
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

	WriterTest();
	
	return 0;
}

