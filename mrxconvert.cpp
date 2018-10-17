// mrxconvert.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "openslide.h"
#include <string>
#include <fstream>
#include <vector>
#include <Windows.h>
#include "writer.h"
#include "wsiconvert.h"

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

struct MrxConvertArgs
{
	std::string mrx_filename;
	std::string dicom_dir;
	std::string server_hostname;
	uint16_t port;
	std::string aetitle;

	MrxConvertArgs()
	{
		server_hostname = "localhost";
		port = 104;
	}
};

void ParseArgs(MrxConvertArgs & args, int argc, wchar_t* argv[])
{
	for (int narg = 1; narg < argc -1; ++narg)
	{
		if (std::wstring(argv[narg]) == L"-i")
			args.mrx_filename = utf8_encode(argv[narg+1]);
		else if (std::wstring(argv[narg]) == L"-o")
			args.dicom_dir = utf8_encode(argv[narg + 1]);
		else if (std::wstring(argv[narg]) == L"--hostname")
			args.server_hostname = utf8_encode(argv[narg + 1]);
		else if (std::wstring(argv[narg]) == L"--port")
			args.port = atoi(utf8_encode(argv[narg + 1]).c_str());
		else if (std::wstring(argv[narg]) == L"--aetitle")
			args.aetitle = atoi(utf8_encode(argv[narg + 1]).c_str());
	}
}

int _tmain(int argc, wchar_t* argv[])
{
	MrxConvertArgs args;
	ParseArgs(args, argc, argv);

	DicomMetadata md;

	auto errorcode = WsiConvert(
		args.mrx_filename,
		args.dicom_dir,
		args.server_hostname,
		args.port,
		args.aetitle,
		md
	);

	switch (errorcode)
	{
	case WSICONV_MISSING_INPUT:
		printf("Input file %s does not exist\n", args.mrx_filename.c_str());
		break;
	case WSICONV_INVALID_OUTPUT:
		printf("Output directory %s does not exist\n", args.dicom_dir.c_str());
		break;
	case WSICONV_INVALID_INPUT:
		printf("File %s is corrupted\n", args.mrx_filename.c_str());
		break;
	case WSICONV_CONVERSION_ERROR:
		printf("Conversion error\n");
		break;
	case WSICONV_NO_SERVER:
		printf("Could not connect to server %s\n", args.server_hostname.c_str());
		break;
	}

	return 0;
}
