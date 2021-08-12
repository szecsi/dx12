#pragma once

#include <fstream>
#include <map>
#include <vector>
#include <string>

namespace Egg11 {

	class SystemEnvironment
	{
		typedef std::map<std::string, std::vector<std::string> > ArgumentMap;
		ArgumentMap arguments;

		std::ofstream logStream;
	public:
		SystemEnvironment(void);
		~SystemEnvironment(void);

		void logWarning(std::string message, std::string filename, unsigned int lineNumber);

		template<class T>
		T		getArgument(std::string argumentName);
		bool	getBoolArgument(std::string argumentName);
		std::string getStringArgument(std::string argumentName);
		const std::vector<std::string>& getStringListArgument(std::string argumentName);

		std::string resolveMediaPath(std::string filename);
		std::string resolveShaderPath(std::string filename);
		
		std::vector<std::string> getTokens(std::string commandLine);
		char* strmbtok(char *input, char *delimit, char *openblock, char *closeblock);
	};

} // namespace Egg11
