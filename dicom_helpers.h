#pragma once

#include "gdcmDataElement.h"
#include "gdcmAttribute.h"

std::string EvenPad(const char * s)
{
	std::string v(s);
	if (v.size() % 2 == 1)
		v += " ";
	return v;
}

gdcm::DataElement DicomAnyString(uint16_t group, uint16_t elem, gdcm::VR datatype, const char * s, bool padding = true)
{
	gdcm::DataElement dicomel(gdcm::Tag(group, elem));
	dicomel.SetVR(datatype);
	std::string v = padding ? EvenPad(s) : s;
	dicomel.SetByteValue(v.c_str(), (uint32_t)v.size());
	return dicomel;
}

gdcm::DataElement DicomAttribTag(uint16_t group, uint16_t elem, uint16_t first, uint16_t second)
{
	gdcm::DataElement dicomel(gdcm::Tag(group, elem));
	dicomel.SetVR(gdcm::VR::AT);
	uint16_t val[2] = { first, second };
	dicomel.SetByteValue((char*)val, 2 * sizeof(uint16_t));
	return dicomel;
}

gdcm::DataElement DicomCodeString(uint16_t group, uint16_t elem, const char * s)
{
	return DicomAnyString(group, elem, gdcm::VR::CS, s);
}

gdcm::DataElement DicomIntegerString(uint16_t group, uint16_t elem, int val)
{
	char s[13];
	sprintf(s, "%d", val);
	return DicomAnyString(group, elem, gdcm::VR::IS, s);
}

gdcm::DataElement DicomDecimalString(uint16_t group, uint16_t elem, int val)
{
	char s[17];
	sprintf(s, "%d", val);
	return DicomAnyString(group, elem, gdcm::VR::DS, s);
}

gdcm::DataElement DicomDecimalString(uint16_t group, uint16_t elem, double val)
{
	char s[17];
	sprintf(s, "%f", val);
	return DicomAnyString(group, elem, gdcm::VR::DS, s);
}

gdcm::DataElement DicomDecimalString(uint16_t group, uint16_t elem, const char * s)
{
	//max 16 chars per element
	return DicomAnyString(group, elem, gdcm::VR::DS, s);
}
