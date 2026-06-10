#pragma once
#include <string>
#include <filesystem>
#include <windows.h>
namespace features {
	std::string get_username();
	std::string get_home();
	std::filesystem::path get_temp();
	bool is_admin(); //GetTokenInformation(TokenElevation)
	std::wstring get_version();
}
