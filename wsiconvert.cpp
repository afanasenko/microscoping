#include "stdafx.h"
#include "gdcmCompositeNetworkFunctions.h"
#include "mirax_wrapper.h"
#include "wsiconvert.h"

// Проверка на существование файла с заданным именем
bool isfile(std::string const & filename)
{
	struct stat s;
	if (stat(filename.c_str(), &s) == 0)
		if (s.st_mode & S_IFREG)
			return true;
	return false;
}

// Проверка на существование папки с заданным именем
bool isdir(std::string const & pathname)
{
	struct stat s;
	if (stat(pathname.c_str(), &s) == 0)
		if (s.st_mode & S_IFDIR)
			return true;
	return false;
}

void pathsplit(std::string const & path, std::string & parent, std::string & name, std::string & ext)
{
	size_t sep = path.find_last_of("\\/");
	if (sep != std::string::npos)
		parent = path.substr(sep + 1, path.size() - sep - 1);

	size_t dot = path.find_last_of(".");
	if (dot != std::string::npos)
	{
		size_t namestart = sep == std::string::npos ? 0 : sep + 1;
		name = path.substr(sep, dot);
		ext = path.substr(dot, path.size() - dot);
	}
	else
	{
		name = path;
		ext = "";
	}

	//printf("path: %s\nparent: %s\nname: %s\next: %s\n", path.c_str(), parent.c_str(), name.c_str(), ext.c_str());
}

/// Основная функция для преобразования mrxs->dicom и отправки на сервер
/// \param input_filename Путь к файлу .mrxs
/// \param dicom_dir имя каталога, в который будут записаны dcm-файлы
/// \param server_name имя или IP-адрес DICOM-сервера для отправки. Если пустое - ничего не шлем на сервер
/// \param port номер порта, на котором работает DICOM-сервер
/// \param aetitle название DICOM-сервера, прописанное в его настройках
/// \return код ошибки (см. WsiConvResult)
WsiConvResult WsiConvert(
	std::string const & input_filename,
	std::string const & dicom_dir,
	std::string const & server_name,
	uint16_t port,
	std::string const & aetitle,
	DicomMetadata const & dicom_metadata
)
{
	bool dicom_networking = !server_name.empty();

	if (!isfile(input_filename))
		return WSICONV_MISSING_INPUT;

	if (!isdir(dicom_dir))
		return WSICONV_INVALID_OUTPUT;

	std::string input_dir;
	std::string input_basename;
	std::string input_ext;
	pathsplit(input_filename, input_dir, input_basename, input_ext);

	MiraxWrapper mrx;
	if (!mrx.Open(input_filename.c_str()))
		return WSICONV_INVALID_INPUT;

	std::vector<std::string> output_filenames;


	int start_from_level = 4;

	for (int level = start_from_level; level < mrx.GetLevelCount(); ++level)
	{
		char level_filename[256];
		sprintf(level_filename, "%s/%s-%05d.dcm", dicom_dir.c_str(), input_basename.c_str(), level);

		if (mrx.WriteDicomFile(level_filename, level))
			output_filenames.push_back(level_filename);
		else
			return WSICONV_CONVERSION_ERROR;
	}

	if (dicom_networking)
	{
		if (!gdcm::CompositeNetworkFunctions::CEcho(server_name.c_str(), (uint16_t)port, "wsiconv", aetitle.c_str()))
			return WSICONV_NO_SERVER;
	}

	return WSICONV_OK;
}