#include "data_path.h"

#include <Windows.h>

std::string data_path(std::string const suffix)
{
	//windows
	//thanks Jim McCann and https://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe

	CHAR buffer[MAX_PATH];
	DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	std::string path = buffer;
	path = path.substr(0, path.rfind('\\'));
	return path + "/" + suffix;
}