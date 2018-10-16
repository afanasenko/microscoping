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

// �������� �� ������������� �����
bool fexists(const char *filename) {
  std::ifstream ifile(filename);
  return ifile ? true : false;
}

// ��e����������� Unicode � UTF-8
std::string utf8_encode(const std::wstring &wstr)
{
    if( wstr.empty() ) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// ��e����������� UTF-8 � Unicode
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

	if (!mrx.WriteDicomFile(dicom_filename.c_str(), 6))
	{
		printf("File not written\n");
		return 1;
	}

	return 0;
}

