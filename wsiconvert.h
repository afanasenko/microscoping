#pragma once

enum WsiConvResult
{
	WSICONV_OK = 0,
	WSICONV_MISSING_INPUT,
	WSICONV_INVALID_INPUT,
	WSICONV_INVALID_OUTPUT,
	WSICONV_NO_SERVER,
	WSICONV_CONVERSION_ERROR,
	WSICONV_STORE_ERROR,
	WSICONV_PATIENT_ERROR
};

struct DicomMetadata
{
	std::string patient_firstname;
	std::string patient_lastname;
	std::string patient_middlename;
	std::string patient_sex;
	std::string patient_birthday;
	std::string patient_age;
	std::string physician_firstname;
	std::string physician_lastname;
	std::string physician_middlename;
};

/// �������� ������� ��� �������������� mrxs->dicom � �������� �� ������
/// \param input_filename ���� � ����� .mrxs
/// \param dicom_dir ��� ��������, � ������� ����� �������� dcm-�����
/// \param server_name ��� ��� IP-����� DICOM-������� ��� ��������. ���� ������ - ������ �� ���� �� ������
/// \param port ����� �����, �� ������� �������� DICOM-������
/// \param aetitle �������� DICOM-�������, ����������� � ��� ����������
/// \return ��� ������ (��. WsiConvResult)
WsiConvResult WsiConvert(
	std::string const & input_filename,
	std::string const & dicom_dir,
	std::string const & server_name,
	uint16_t port,
	std::string const & aetitle,
	DicomMetadata const & dicom_metadata
);