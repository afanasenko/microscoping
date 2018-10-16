// mrxconvert.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "openslide.h"
#include <string>
#include <fstream>
#include <vector>
#include <Windows.h>
#include "writer.h"
#include "mirax_wrapper.h"

using namespace std;

std::vector<std::string> split(const std::string &s, char delim) {
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> elems;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
		// elems.push_back(std::move(item));
	}
	return elems;
}

std::string join(std::vector<std::string> const & strings, char delim) {
	if (!strings.size())
		return "";
	std::string ret = strings[0];
	for (auto s = strings.begin() + 1; s != strings.end(); ++s)
		ret += std::string(1, delim) + *s;
	return ret;
}

std::string insert_filename_suffix(std::string filename, std::string suffix) {
	auto parts = split(filename, '.');
	auto n = parts.size();
	if (n > 1)
		parts.insert(parts.end() - 1, suffix);
	return join(parts, '.');
}

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
	std::string dicom_filename;

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

	if (argc > 2)
	{
		dicom_filename = utf8_encode(argv[2]);
	}
	else
	{
		printf("Output file not specified\n");
		return 1;
	}
	
	MiraxWrapper mrx;
	if (!mrx.Open(filename.c_str()))
	{
		printf("File not opened\n");
		return 1;
	}

	int start_from_level = 4;

	for (int level = start_from_level; level < mrx.GetLevelCount(); ++level)
	{
		std::string level_file = insert_filename_suffix(dicom_filename, std::to_string(level));
		if (!mrx.WriteDicomFile(level_file.c_str(), level))
		{
			printf("File %s not written\n", level_file.c_str());
			return 1;
		}
	}

	return 0;
}

