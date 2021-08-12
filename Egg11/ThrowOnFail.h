#pragma once
#include <exception>
#include <sstream>

namespace Egg11 {

class HrException : public std::exception
{
public:
	std::string errorMessage;
	std::string filename;
	int lineNumber;
	HRESULT hr;

	HrException(HRESULT hr, std::string errorMessage, std::string filename, int lineNumber)
		:hr(hr), errorMessage(errorMessage), filename(filename), lineNumber(lineNumber)
	{
	}

	const char* what() const
	{
		return errorMessage.c_str();
	}

	void fail()
	{
		std::stringstream errorMessage;
		errorMessage << this->filename << "(" << this->lineNumber << "): " << this->errorMessage;
		MessageBoxA(NULL, errorMessage.str().c_str(), "Error!", MB_ICONSTOP | MB_OK);
		exit(-1);
	}
};

class ThrowOnFail
{
	std::string errorMessage;
	std::string filename;
	int lineNumber;
public:
	ThrowOnFail(std::string errorMessage, std::string filename, int lineNumber)
		:errorMessage(errorMessage), filename(filename), lineNumber(lineNumber)
	{
	}

	// Throw exception.
	void operator=(HRESULT hr)
	{
		if (FAILED(hr))
			throw HrException(hr, errorMessage, filename, lineNumber);
	}

	// Throw message.
	void operator^(HRESULT hr)
	{
		if (FAILED(hr))
			HrException(hr, errorMessage, filename, lineNumber).fail();
	}
};

} // namespace Egg11
