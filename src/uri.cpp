#include <algorithm>
#include <exception>
#include <stdexcept>

#include "uri.h"

const std::string CURI::sProtocolSeparator{"://"};

CURI::CURI(const std::string& cStringToSet) : m_originalString{cStringToSet}
{
}

std::optional<std::string> CURI::GetProtocol() const noexcept
{
	const std::size_t cnProtocolEnd = m_originalString.find(sProtocolSeparator);

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
	// Checking for protocol start
	auto addressStart = m_originalString.begin(); 
	const std::size_t protocolStart{m_originalString.find(sProtocolSeparator)};
	if(protocolStart != std::string::npos)
	{
		std::advance(addressStart, protocolStart + sProtocolSeparator.size());
	}

	// Checking for address end
	auto addressEnd = std::find_first_of(addressStart, m_originalString.end(), 
		POSSIBLE_ADDRESS_END.begin(), POSSIBLE_ADDRESS_END.end());

	return std::string{addressStart, addressEnd};
}

std::optional<int> CURI::GetPort() const noexcept
{
	// Port specified between ':' and '#' '?' '/' chars
	// Also we should omit protocol specifier ://
	auto portStartPos = m_originalString.begin();
	// Omiting ://
	if(const std::size_t cnProtocolStart = m_originalString.find(sProtocolSeparator); 
		cnProtocolStart != std::string::npos)
	{
		std::advance(portStartPos, cnProtocolStart + sProtocolSeparator.size());
	}

	// Looking for port start
	portStartPos = std::find(portStartPos, m_originalString.end(), ':');

	// If we didn't find port, assuming it's 80
	if(portStartPos == m_originalString.end())
	{
		return 80;
	}
	// If we find port, skip ':'
	++portStartPos;

	// Now looking for port end
	auto portEndPos = std::find_first_of(portStartPos, m_originalString.end(), 
		POSSIBLE_PORT_END.begin(), POSSIBLE_PORT_END.end());

	const std::string csStringPort{portStartPos, portEndPos};
	// Checking if given port is valid
	try {
		std::size_t nLastParsedCharIndex{0};
		const int ciParsedNumber = std::stoi(csStringPort.c_str(), &nLastParsedCharIndex);
	
		// This is largest port number
		// For better safety checking if last parsed char is last in string, if not we have string like: "1234asd"
		if(ciParsedNumber > 65535 || 
			nLastParsedCharIndex != csStringPort.size()) 
		{
			return std::nullopt;
		}
		return ciParsedNumber;
	}
	// std::stoi can throw invalid_argument or out_of_range, 
	// So catching any exceptions
	catch(...)
	{
		return std::nullopt;
	}
}


std::optional<std::filesystem::path> CURI::GetPath() const noexcept
{
	auto pathStartPos = m_originalString.begin();
	// Omiting ://
	if(const std::size_t cnProtocolStart = m_originalString.find(sProtocolSeparator); 
		cnProtocolStart != std::string::npos)
	{
		std::advance(pathStartPos, cnProtocolStart + sProtocolSeparator.size());
	}

	// Checking for path start
	pathStartPos = std::find(pathStartPos, m_originalString.end(), '/');
	if(pathStartPos == m_originalString.end())
	{
		return std::nullopt;
	}
	
	auto pathEndPos = std::find_first_of(pathStartPos, m_originalString.end(), 
		POSSIBLE_PATH_END.begin(), POSSIBLE_PATH_END.end());

	// taking in mind zero path(proto://some.page.com/)
	if(std::distance(pathStartPos, pathEndPos) <= 1)
	{
		return std::nullopt;
	}

	return std::filesystem::path(std::string{pathStartPos, pathEndPos});
}

std::optional<std::string> CURI::GetQuery() const noexcept
{
	auto queryStart = std::find(m_originalString.begin(), m_originalString.end(), '?');

	if(queryStart == m_originalString.end())
	{
		return std::nullopt;
	}
	//
	// Skipping '?'
	++queryStart;

	auto queryEnd = std::find(queryStart, m_originalString.end(), '#');

	// Checking if it's not empty
	if(std::distance(queryStart, queryEnd) == 0)
	{
		return std::nullopt;
	}

	return std::string{queryStart, queryEnd};
}

std::optional<std::string> CURI::GetFragment() const noexcept
{
	auto fragmentStart = std::find(m_originalString.begin(), m_originalString.end(), '#');

	if(fragmentStart == m_originalString.end())
	{
		return std::nullopt;
	}
	// Skipping '#'
	++fragmentStart;

	if(std::distance(fragmentStart, m_originalString.end()) == 0)
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
