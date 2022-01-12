#include <stdexcept>

#include "cli_args_helper.h"

CLI::CCLIArgsHelper::CCLIArgsHelper(int iArgc, const char *const cszArgv[])
{

	if(iArgc <= 0 || cszArgv == nullptr)
	{
		return;
	}
	
	m_sExecutableName = cszArgv[0];

	std::string sCurrentKey = "";
	// Skipping 0 value as it is call to program
	for(int iIndex = 1; iIndex < iArgc; iIndex++)
	{
		// String that would be passed to key or value
		std::string sTempString = cszArgv[iIndex];
		// Checking if temp string is key
		if(sTempString.size() > 0 && sTempString[0] == '-')
		{
			const int ciKeyStartPos = 
				sTempString.size() > 1 && sTempString[1] == '-' ? 2 : 1;
			// if key is not empty adding it to map as paramater without value
			if(!sCurrentKey.empty())
			{
				m_parameters.insert({sCurrentKey, std::string("")});
			}
			sCurrentKey = sTempString.substr(ciKeyStartPos);
		}
		else // if this parameter is not key adding it as a value
		{
			m_parameters[std::move(sCurrentKey)] = std::move(sTempString);
			sCurrentKey = "";
		}
	}
	// If we have any other parameters without keys in the end, adding them
	if(!sCurrentKey.empty())
	{
		m_parameters.insert({sCurrentKey, std::string("")});
	}

}

bool CLI::CCLIArgsHelper::CheckIfParameterExist(const std::string& csParameterToCheck) const noexcept
{
	// Trying to extract value
	return m_parameters.find(csParameterToCheck) != m_parameters.end();
}

std::optional<std::string> CLI::CCLIArgsHelper::GetParameterValue(const std::string &csParameterToGet) const noexcept
{
	// Trying to extract value
	if(auto foundValue = m_parameters.find(csParameterToGet); 
		foundValue != m_parameters.end())
	{
		return foundValue->second;
	}
	return std::nullopt;
}
