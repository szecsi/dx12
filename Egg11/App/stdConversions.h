#pragma once

#include <sstream>

namespace Egg11
{

	/** Converts T into string.
		* @param t The value that should be converted.
		* @return The string representation.
		*/
	template<class T>
	inline std::string toString(const T& t)
	{
		std::ostringstream ss;
		ss << t;
		return ss.str();
	}

	/** Converts the string into T.
		* @param s The string that should be converted.
		* @return The T value created from the string.
		*/
	template<class T>
	inline T fromString(const std::string& s)
	{
		std::istringstream ss(s);
		T t;
		ss >> t;
		return t;
	}

	/** Converts T into string.
		* @param t The value that should be converted.
		* @return The string representation.
		*/
	template<class T>
	inline std::wstring toWString(const T& t)
	{
		std::wostringstream ss;
		ss << t;
		return ss.str();
	}

	/** Converts the string into T.
		* @param s The string that should be converted.
		* @return The T value created from the string.
		*/
	template<class T>
	inline T fromWString(const std::wstring& s)
	{
		std::wistringstream ss(s);
		T t;
		ss >> t;
		return t;
	}

} // namespace Egg11
