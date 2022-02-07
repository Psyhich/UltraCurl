#include <algorithm>

#include "zstd.h"

#include "response.h"

bool HTTP::CHTTPResponse::LoadStatusLine(std::size_t &nIndex, 
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

bool HTTP::CHTTPResponse::LoadHeaders(std::size_t &nIndex, const std::vector<char> &cDataToParse) noexcept
{
	m_headers.clear();

	bool bIsFoundEnd{false};
	
	bool bIsNewLine{false};
	bool bIsReadingValue{false};

	std::string sReadKey;
	std::string sReadValue;

	// Should split key and value with :
	for(; nIndex <= cDataToParse.size() - 2; nIndex++)
	{
		const char &cchCurrentChar = cDataToParse[nIndex];
		// if we got CRLF and we had header ended before with CRLF or LF so breaking
		if(cchCurrentChar == '\r' && 
			cDataToParse[nIndex + 1] == '\n' &&
			bIsNewLine)
		{
			// Skipping CRLF, so index will be placed on first byte of data
			nIndex += 2;
			bIsFoundEnd = true;
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
				break;
			}

			// All key values should have lowercase letters 
			std::transform(sReadKey.begin(), sReadKey.end(), sReadKey.begin(), 
				[](char c) -> char{ return std::tolower(c); });

			m_headers[sReadKey] = sReadValue;

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

	return bIsFoundEnd;
}

void HTTP::CHTTPResponse::LoadData(std::size_t &nIndex, 
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

void HTTP::CHTTPResponse::LoadData(std::vector<char> &&dataToLoad) noexcept
{
	m_data = std::move(dataToLoad);
}

bool HTTP::CHTTPResponse::LoadAll(const std::vector<char> &cDataToParse) noexcept
{
	std::size_t nCurrentIndex = 0;
	
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
	std::size_t nCurrentIndex = 0;

	// Loading status line and if it wont fail loading headers
	if(!LoadStatusLine(nCurrentIndex, cDataToParse))
	{
		return false;
	}
	return LoadHeaders(nCurrentIndex, cDataToParse);
}

bool HTTP::CHTTPResponse::DecompressBody() noexcept
{
	if(m_data.empty())
	{
		return false;
	}

	const size_t cnDecompressedSize = 
		ZSTD_getFrameContentSize(m_data.data(), m_data.size());

	if(cnDecompressedSize == ZSTD_CONTENTSIZE_ERROR)
	{
		fprintf(stderr, "Couldn't determine size of decompressed file: %s", ZSTD_getErrorName(cnDecompressedSize));
		return false;
	}

	// We have split in here: we can decompress file using streams or using full decompress function
	// This will be based on fact if we can determine size of decompressed file
	std::vector<char> decompressedData;
	if(cnDecompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
	{
		// Context object for decompression
		ZSTD_DCtx* const decompressionContext = ZSTD_createDCtx();

		// if have't determined the size, we should use streams
		// We should feed data from m_data into ZSTD_decompressStream by chunk specified in ZSTD_DStreamInSize
		// And monitor how much data we can write to decompressedData at once
		const size_t cnInputStreamSize = ZSTD_DStreamInSize();
		const size_t cnOutputStreamSize = ZSTD_DStreamInSize();

		size_t nCurrentReadPosition{0};
		size_t nLastReturn{0};
		size_t nLastWritePosition{0};

		while(nCurrentReadPosition < m_data.size())
		{
			decompressedData.resize(decompressedData.size() + cnOutputStreamSize + 1);

			ZSTD_inBuffer inputBuffer = 
				{ 
					m_data.data() + nCurrentReadPosition, // Position to read from
					nCurrentReadPosition + cnInputStreamSize >= m_data.size() ? 
						m_data.size() - nCurrentReadPosition : cnInputStreamSize, // Length of chunk
					0 // Current position in array
				};
			while(inputBuffer.pos < inputBuffer.size)
			{
				ZSTD_outBuffer outputBuffer = 
					{ 
						decompressedData.data(),
						cnOutputStreamSize, 
						decompressedData.size() - cnOutputStreamSize
					};

				const size_t nResult = 
					ZSTD_decompressStream(decompressionContext, &outputBuffer, &inputBuffer);
				if(ZSTD_isError(nResult))
				{
					fprintf(stderr, "Failed to process decompressing\n");
					return false;
				}
				nLastReturn = nResult;
				nLastWritePosition = outputBuffer.pos;
			}
			nCurrentReadPosition += cnInputStreamSize;
		}
		
		// Truncating data if we allocated to much
		decompressedData.resize(nLastWritePosition);
		

		// Checking if last return was 0 which means 
		// that there are no need to load  more data
		if(nLastReturn != 0)
		{
			fprintf(stderr, "Given input is truncated\n");
			return false;
		}

		ZSTD_freeDCtx(decompressionContext);
	}
	// decompressing using one function if we determined size
	else
	{
		decompressedData.reserve(cnDecompressedSize);

		const size_t nDecompressResult = 
			ZSTD_decompress(decompressedData.data(), decompressedData.size(), m_data.data(), m_data.size());

		if(ZSTD_isError(nDecompressResult))
		{
			fprintf(stderr, "Couldn't decompress file: %s", ZSTD_getErrorName(nDecompressResult));
			return false;
		}
	}
	m_data.swap(decompressedData);

	return true;
}

