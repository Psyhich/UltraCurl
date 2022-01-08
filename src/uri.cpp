#include <exception>
#include <stdexcept>

#include "uri.h"

CURI::CURI(const std::string& cStringToSet) : m_originalString{cStringToSet}
{
}

std::optional<std::string> CURI::GetProtocol() const noexcept
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
	}
	else 
	{


		for(size_t nIndex = 0; nIndex < nProtocolLength; nIndex++)
		{
			if(!CanBeUsedInProtocol(m_originalString[nIndex]))
			{
				return std::nullopt;
			}
		}
		return m_originalString.substr(0, nProtocolLength);
	}
}

std::optional<std::string> CURI::GetPureAddress() const noexcept
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
			nAddressEnd = nIndex;
			break;
		}
	}

	if(nAddressEnd <= nAddressStart)
	{
		return std::nullopt;
	}

	return m_originalString.substr(nAddressStart, nAddressEnd - nAddressStart);
}

std::optional<int> CURI::GetPort() const noexcept
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
		}	
	}

	// If we didn't find port, assuming it's 80
	if(nPortStartPos == 0)
	{
		return 80;
	}

	// Now looking for port end
	for(size_t nIndex = nPortStartPos; nIndex < m_originalString.size() + 1; nIndex++)
	{
		const char &cchCurrentChar = m_originalString.c_str()[nIndex];
		if(cchCurrentChar == '/' || cchCurrentChar == '#' ||
			cchCurrentChar == '?' || cchCurrentChar == '\0')
		{
			nPortEndPos = nIndex;
			break;
		}
	}
	if(nPortEndPos == nPortStartPos)
	{
		return std::nullopt;
	}
	std::string cStringPort = m_originalString.substr(nPortStartPos, nPortEndPos - nPortStartPos);
	
	try {
		const int ciParsedNumber = std::stoi(cStringPort.c_str());
		if(ciParsedNumber > 65535) // This is larges port number
		{
			return std::nullopt;
		}
		return ciParsedNumber;
	// std::stoi can throw invalid_argument or out_of_range, 
	// So catching any exception
	} catch(const std::exception &err)
	{
		return std::nullopt;
	}
}


std::optional<std::filesystem::path> CURI::GetPath() const noexcept
{
	size_t nPathStart = 0;
	for(size_t nIndex = 0; nIndex < m_originalString.size(); nIndex++)
	{
		// We should take in mind that after protocol we also have ://
		const char& cchCurrentChar = m_originalString[nIndex];
		if(cchCurrentChar == '/')
		{
			nPathStart = nIndex;
			break;
		} else if(nIndex < m_originalString.size() - 3 && 
			cchCurrentChar == ':' && 
			m_originalString[nIndex + 1] == '/' &&
			m_originalString[nIndex + 2] == '/')
		{
			// Skipping :// part
			// Will skip :/ and in the end of cycle we will skip last /
			nIndex += 2; 
		}
	}

	// Path cannot begin in 0 index because it should start with '/'
	if(nPathStart == 0)
	{
		return std::nullopt;
	}

	// Looking for path end(\0 ? #)
	size_t nPathEnd = 0;
	for(size_t nIndex = nPathStart; nIndex < m_originalString.size() + 1; nIndex++)
	{
		const char& cchCurrentChar = m_originalString.c_str()[nIndex];
		if(cchCurrentChar == '#' || cchCurrentChar == '?' || cchCurrentChar == '\0')
		{
			nPathEnd = nIndex;
			break;
		}
	}
	// taking in mind zero path(proto://some.page.com/)
	if(nPathEnd - nPathStart <= 1)
	{
		return std::nullopt;
	}

	return 
		std::filesystem::path(m_originalString.substr(nPathStart, nPathEnd - nPathStart));
}

