#include <exception>
#include <stdexcept>

#include "uri.h"

std::optional<std::string> URI::GetProtocol() const noexcept
{
	size_t nProtocolLength = 0;
	// Looking for :// pattern
	// Starting from 2 because length of :// pattern is 3
	for(size_t nIndex = 2; nIndex < m_originalString.size(); nIndex++)
	{
		if(m_originalString[nIndex] == '/' && m_originalString[nIndex - 1] == '/' &&
			m_originalString[nIndex - 2] == ':')
		{
			nProtocolLength = nIndex - 2;
			break;
		}
	}

	if(nProtocolLength == 0)
	{
		return std::nullopt;
	} else 
	{
		return m_originalString.substr(0, nProtocolLength);
	}
}

std::optional<std::string> URI::GetPureAddress() const noexcept
{
	// We should take in mind protocol and port, ideally address can end with / ? #
	size_t nAddressStart = 0;
	size_t nAddressEnd = 0;
	
	// Checking for protocol start
	for (size_t nIndex = 2; nIndex < m_originalString.size(); nIndex++) {
		const char &cchCurrentChar = m_originalString[nIndex];
		if(cchCurrentChar == '/' && m_originalString[nIndex - 1] == '/' &&
			m_originalString[nIndex - 2] == ':')
		{
			nAddressStart = nIndex + 1;
			break;
		}
	}
	// Checking for address end(: # ? / chars) adfter protocol(if found)
	for(size_t nIndex = nAddressStart; nIndex < m_originalString.size() + 1; nIndex++)
	{
		const char &cchCurrentChar = m_originalString.c_str()[nIndex];
		if(cchCurrentChar == '/' || cchCurrentChar == '#' ||
			cchCurrentChar == '?' || cchCurrentChar == ':' ||
			cchCurrentChar == '\0')
		{
			nAddressEnd = nIndex - 1;
			break;
		}
	}

	if(nAddressEnd <= nAddressStart)
	{
		return std::nullopt;
	}

	return m_originalString.substr(nAddressStart, nAddressEnd - nAddressStart);
}

std::optional<int> URI::GetPort() const noexcept
{
	// Port specified between ':' and '#' '?' '/' chars
	// Also we should omit protocol specifier ://
	size_t nPortStartPos = 0;
	size_t nPortEndPos = 0;

	// Checking for port start and excluding protocol
	// Creating only one loop would produce a lot of ifs
	for (size_t nIndex = 0; nIndex < m_originalString.size(); nIndex++) {
		const char &cchCurrentChar = m_originalString[nIndex];
		if(nIndex < m_originalString.size() - 4 && 
			cchCurrentChar == ':' && 
			m_originalString[nIndex + 1] != '/' &&
			m_originalString[nIndex + 2] != '/')
		{
			nPortStartPos = nIndex + 1;
			break;
		} else if(cchCurrentChar == ':')
		{
			nPortStartPos = nIndex + 1;
			break;
		}
	}

	// Now looking for port end
	for(size_t nIndex = nPortStartPos; nIndex < m_originalString.size() + 1; nIndex++)
	{
		const char &cchCurrentChar = m_originalString.c_str()[nIndex];
		if(cchCurrentChar == '/' || cchCurrentChar == '#' ||
			cchCurrentChar == '?' || cchCurrentChar == '\0')
		{
			nPortEndPos = nIndex - 1;
			break;
		}
	}
	if(nPortEndPos != nPortStartPos)
	{
		return std::nullopt;
	}
	std::string cStringPort = m_originalString.substr(nPortStartPos, nPortEndPos - nPortStartPos);
	
	try {
		const int ciParsedNumber = std::stoi(cStringPort.c_str());
		return ciParsedNumber;
	} catch(std::invalid_argument &err)
	{
		return std::nullopt;
	}
}
