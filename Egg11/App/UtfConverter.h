#pragma once

#include <string>

namespace Egg11 {

	class UtfConverter
	{
	public:
		static std::wstring utf8to16(const std::string& utf8string);
		static std::string utf16to8(const std::wstring& widestring);
	};
}
