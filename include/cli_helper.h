#ifndef CLI_HELPER_H
#define CLI_HELPER_H

#include <string>
#include <map>
#include <optional>

namespace CLI {
	/// Class for easier interaction with CLI
	/// Paramaters beggining with - or -- treated as paramater name
	/// Everything without - or -- treated as value for name before or set for "" key
	/// Paramaters written without value will have "" value
	class CCLIHelper
	{
	public:
		CCLIHelper(int iArgc, char *const cszArgv[]);

		bool CheckIfParameterExist(const std::string& csParameterToCheck) const noexcept;
		std::optional<std::string> GetParameterValue(const std::string &csParameterToGet) const noexcept;

		inline std::string GetName() const noexcept
		{
			return m_sExecutableName;
		}

	private:
		std::map<std::string, std::string> m_parameters;
		std::string m_sExecutableName;
	};
} // CLI



#endif //CLI_HELPER_H
