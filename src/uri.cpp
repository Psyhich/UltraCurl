#include <algorithm>
#include <exception>
#include <stdexcept>

#include "uri.h"

CURI::CURI(const std::string& cStringToSet) : m_originalString{cStringToSet}
{
}

std::optional<std::string> CURI::GetProtocol() const noexcept
{
	const std::size_t cnProtocolEnd = m_originalString.find("://");

	if(cnProtocolEnd == std::string::npos ||
		!std::all_of(m_originalString.begin(), m_originalString.begin() + cnProtocolEnd, 
		CanBeUsedInProtocol))
	{
		return std::nullopt;
	}

	return m_originalString.substr(0, cnProtocolEnd);
}

std::optional<std::string> CURI::GetPureAddress() const noexcept
{
	// We should take in mind protocol and port, ideally address can end with / ? #
	std::size_t nAddressStart = 0;
	std::size_t nAddressEnd = 0;
	
	// Checking for protocol start
	for (std::size_t nIndex = 2; nIndex < m_originalString.size(); nIndex++) {
		const char &cchCurrentChar = m_originalString[nIndex];
		if(cchCurrentChar == '/' && m_originalString[nIndex - 1] == '/' &&
			m_originalString[nIndex - 2] == ':')
		{
			nAddressStart = nIndex + 1;
			break;
		}
	}
	// Checking for address end(: # ? / chars) adfter protocol(if found)
	for(std::size_t nIndex = nAddressStart; nIndex < m_originalString.size() + 1; nIndex++)
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
	std::size_t nPortStartPos = 0;
	std::size_t nPortEndPos = 0;

	// Checking for port start and excluding protocol
	// Creating only one loop would produce a lot of ifs
	for (std::size_t nIndex = 0; nIndex < m_originalString.size(); nIndex++) {
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
	for(std::size_t nIndex = nPortStartPos; nIndex < m_originalString.size() + 1; nIndex++)
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
	std::size_t nPathStart = 0;
	for(std::size_t nIndex = 0; nIndex < m_originalString.size(); nIndex++)
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
	std::size_t nPathEnd = 0;
	for(std::size_t nIndex = nPathStart; nIndex < m_originalString.size() + 1; nIndex++)
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

std::optional<std::string> CURI::GetQuery() const noexcept
{
	auto queryStart = std::find(m_originalString.begin(), m_originalString.end(), '?');

	if(queryStart == m_originalString.end())
	{
		return std::nullopt;
	}

	auto queryEnd = std::find(queryStart, m_originalString.end(), '#');

	return std::string{queryStart, queryEnd};
}

std::optional<std::string> CURI::GetFragment() const noexcept
{
	auto fragmentStart = std::find(m_originalString.begin(), m_originalString.end(), '#');

	if(fragmentStart == m_originalString.end())
	{
		return std::nullopt;
	}

	return std::string{fragmentStart, m_originalString.end()};
}


bool operator<(const CURI& cLURIToCompare, const CURI &cRURIToCompare) noexcept
{
	return cLURIToCompare.m_originalString < cRURIToCompare.m_originalString;
}
bool operator==(const CURI& cLURIToCompare, const CURI &cRURIToCompare) noexcept
{
	return cLURIToCompare.m_originalString == cRURIToCompare.m_originalString;
}
