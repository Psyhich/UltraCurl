#include <algorithm>

#include "response.h"

bool HTTP::CHTTPResponse::LoadStatusLine(size_t &nIndex, 
	const std::vector<char> &cDataToParse) noexcept
{
	m_iCode = 0;
	std::string sCode;
	sCode.reserve(3);

	bool bIsFoundWhiteSpace = false;
	bool bIsFoundCode = false;
	for(; nIndex < cDataToParse.size(); nIndex++)
	{
		const char& cchCurrentChar = cDataToParse[nIndex];
		if(nIndex > 0 && 
			cDataToParse[nIndex - 1] == '\r' && cchCurrentChar == '\n')
		{
			nIndex += 1;
			break;
		}
		else if(cchCurrentChar == ' ' && !bIsFoundCode)
		{
			bIsFoundCode = bIsFoundWhiteSpace;
			bIsFoundWhiteSpace = true;
		}
		else if(bIsFoundWhiteSpace && !bIsFoundCode)
		{
			sCode.push_back(cchCurrentChar);
		}
	}

	try
	{
		m_iCode = std::stoi(sCode);
	}
	catch(std::exception &err)
	{
		return false;
	}

	return true;
}

bool HTTP::CHTTPResponse::LoadHeaders(size_t &nIndex, const std::vector<char> &cDataToParse) noexcept
{
	m_headers.clear();
	
	bool bIsNewLine = false;
	bool bIsReadingValue = false;

	std::string sReadKey;
	std::string sReadValue;

	// Should split key and value with :
	for(; nIndex < cDataToParse.size() - 2; nIndex++)
	{
		const char &cchCurrentChar = cDataToParse[nIndex];
		// if we got CRLF and we had header ended before with CRLF or LF so breaking
		if(cchCurrentChar == '\r' && 
			cDataToParse[nIndex + 1] == '\n' &&
			bIsNewLine)
		{
			break;
		}
		// Checking for CRLF or at least LF
		else if((cchCurrentChar == '\r' && 
			cDataToParse[nIndex + 1] == '\n') ||
			cchCurrentChar == '\n')
		{
			// Trimming both key and value from ' ' and '\t'
			const auto trimmer = [](char chChar) {
				return !std::isspace(chChar);
			};

			sReadKey.erase(sReadKey.begin(), 
				std::find_if(sReadKey.begin(), sReadKey.end(), trimmer));
			sReadValue.erase(sReadValue.begin(), 
				std::find_if(sReadValue.begin(), sReadValue.end(), trimmer));

			sReadKey.shrink_to_fit();
			sReadValue.shrink_to_fit();

			// Key cannot be empty
			if(sReadKey.empty())
			{
				return false;
			}

			m_headers[sReadKey] = std::move(sReadValue);

			sReadKey = "";
			sReadValue = "";
			bIsReadingValue = false;
			bIsNewLine = true;
			// If we had CRLF we should skip 2, if only LF the only 1
			nIndex += cchCurrentChar == '\r';
			continue;
		}
		else if(cchCurrentChar == ':' && !bIsReadingValue)
		{
			bIsReadingValue = true;
			continue;
		}
		if(bIsReadingValue)
		{
			sReadValue += cchCurrentChar;
		} else
		{
			sReadKey += cchCurrentChar;
		}
		bIsNewLine = false;
	}
	return true;
}

void HTTP::CHTTPResponse::LoadData(size_t &nIndex, 
	const std::vector<char> &cDataToLoad) noexcept
{
	m_data.clear();
	if(nIndex > cDataToLoad.size())
	{
		return;
	}

	m_data.reserve(cDataToLoad.size() - nIndex);
	for(; nIndex < cDataToLoad.size(); nIndex++)
	{
		m_data.push_back(cDataToLoad[nIndex]);
	}
}

bool HTTP::CHTTPResponse::LoadAll(const std::vector<char> &cDataToParse) noexcept
{
	size_t nCurrentIndex = 0;
	
	// Loading status line
	if(!LoadStatusLine(nCurrentIndex, cDataToParse))
	{
		return false;
	}

	if(!LoadHeaders(nCurrentIndex, cDataToParse))
	{
		return false;
	}

	LoadData(nCurrentIndex, cDataToParse);

	return true;
}

bool HTTP::CHTTPResponse::LoadHeaders(const std::vector<char> &cDataToParse) noexcept
{
	size_t nCurrentIndex = 0;

	// Loading status line and if it wont fail loading headers
	if(!LoadStatusLine(nCurrentIndex, cDataToParse))
	{
		return false;
	}
	return LoadHeaders(nCurrentIndex, cDataToParse);
}

void HTTP::CHTTPResponse::LoadData(const std::vector<char> &cDataToLoad) noexcept
{
	size_t nIndex = 0;
	LoadData(nIndex, cDataToLoad);
}
