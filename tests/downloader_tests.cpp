#include <string>

#include <gtest/gtest.h>

#include "http_downloader.h"
#include "sockets.h"

struct BaseParams{
	std::string sDataToUse;
	std::string sRealResponseData;

	std::string sAddress;
	std::string sPath;

	BaseParams(const std::string &_sDataToUse, const std::string &_sRealResponseData, 
		const std::string &_sAddess, const std::string &_sPath) : sDataToUse{_sDataToUse}, 
		sRealResponseData{_sRealResponseData}, sAddress{_sAddess},
		sPath{_sPath}
	{
	}

	bool CheckRequest(const std::string &sRequest)
	{
		// Request should have Host, Accept and Accept-Encoding headers
		// It should end with \r\n\r\n, each header ends with \r\n
		// Host header should have right address
		// Status line should contain valid path, GET method 
		// and HTTP/1.1 version should end with \r\n

		// Checking status line
		size_t nEndPos = sRequest.find(' ');
		if(nEndPos == std::string::npos)
		{
			return false;
		}
		std::string sGotString = sRequest.substr(0, nEndPos);
		if(sGotString != "GET")
		{
			return false;
		}

		size_t nStartPos = nEndPos;
		nEndPos = sRequest.find(' ', nEndPos + 1);
		if(nEndPos == std::string::npos)
		{
			return false;
		}
		sGotString = sRequest.substr(nStartPos + 1, nEndPos - nStartPos - 1);
		if(sGotString != sPath)
		{
			return false;
		}
		nStartPos = nEndPos;
		
		nEndPos = sRequest.find("\r\n", nEndPos);
		if(nEndPos == std::string::npos)
		{
			return false;
		}
		sGotString = sRequest.substr(nStartPos + 1, nEndPos - nStartPos - 1);
		if(sGotString != "HTTP/1.1")
		{
			return false;
		}
		
		// Checking headers
		bool bIsFoundHost{false};
		bool bIsFoundAccept{false};
		bool bIsFoundAcceptEncoding{false};

		nStartPos = nEndPos + 2;
		for(int i = 0; i < 3; i++){
			nEndPos = sRequest.find("\r\n", nStartPos);
			if(nEndPos == std::string::npos)
			{
				return false;
			}
			// Pair should follow convention of usage like this: "key: value"
			std::string sKeyValuPair = sRequest.substr(nStartPos, nEndPos - nStartPos);
			size_t nSeparatorPos = sKeyValuPair.find(':');
			if(nSeparatorPos == std::string::npos)
			{
				return false;
			}
			std::string sKey = sKeyValuPair.substr(0, nSeparatorPos);
			// value should be trimmed
			std::string sValue = sKeyValuPair.substr(nSeparatorPos + 2);
			// key is must be case insensitive
			std::transform(sKey.begin(), sKey.end(), sKey.begin(), 
				[](char chChar) { return std::tolower(chChar); });

			if(sKey == "host")
			{
				if(sValue != sAddress)
				{
					return false;
				}
				bIsFoundHost = true;
			}
			else if(sKey == "accept")
			{
				if(sValue != "*/*")
				{
					return false;
				}
				bIsFoundAccept = true;
			}
			else if(sKey == "accept-encoding")
			{
				if(sValue != "identity")
				{
					return false;
				}
				bIsFoundAcceptEncoding = true;
			}
			nStartPos = nEndPos + 2;
		}

		nEndPos = sRequest.size() - 1;
		if(sRequest[nEndPos] != '\n' || sRequest[nEndPos - 1] != '\r' ||
			sRequest[nEndPos - 2] != '\n' || sRequest[nEndPos - 3] != '\r')
		{
			return false;
		}

		return bIsFoundHost && bIsFoundAccept && bIsFoundAcceptEncoding;
	}
};

// Implementing own socket for testing
// This socket should use ParametersStruct, because socket 
// is used as a template not a pointer, not to carry resource 
// managment outside downloader class
template<typename ParametersStruct> 
class TestSocket : public Sockets::CSocket 
{
public:
	explicit TestSocket(const std::string &csAddressToUse) : CSocket(csAddressToUse)
	{
	}

	std::optional<std::vector<char>> ReadTill(const std::string &csStringToReadTill) noexcept override
	{
		if(!m_bIsShoulRespond)
		{
			return std::nullopt;
		}

		const size_t nFoundPos = m_params.sDataToUse.find(csStringToReadTill, m_nCurrentIndex);
		if(nFoundPos == std::string::npos)
		{
			return std::nullopt;
		}

		try
		{
			std::string newData = 
				m_params.sDataToUse.substr(m_nCurrentIndex, nFoundPos - m_nCurrentIndex + csStringToReadTill.size());
			m_nCurrentIndex = nFoundPos + csStringToReadTill.size();
			return std::vector<char>{newData.begin(), newData.end()};
		}
		catch(const std::exception &err)
		{
			return std::nullopt;
		}
	}

	std::optional<std::vector<char>> ReadCount(size_t nCountToRead) noexcept override
	{
		if(!m_bIsShoulRespond || 
				m_nCurrentIndex + nCountToRead > m_params.sDataToUse.size())
		{
			return std::nullopt;
		}

		std::string newData = m_params.sDataToUse.substr(m_nCurrentIndex, nCountToRead);
		m_nCurrentIndex += nCountToRead;
		return std::vector<char>{newData.begin(), newData.end()};
	}

	std::optional<std::vector<char>> ReadTillEnd() noexcept override
	{
		if(!m_bIsShoulRespond)
		{
			return std::nullopt;
		}
		
		std::string newData = m_params.sDataToUse.substr(m_nCurrentIndex);
		m_nCurrentIndex = m_params.sDataToUse.size();
		return std::vector<char>{newData.begin(), newData.end()};
	}

	bool Write(const char* pcchBytes, size_t nCount) noexcept override
	{
		std::string csGotData;
		csGotData.append(pcchBytes, nCount);

		m_bIsShoulRespond = m_params.CheckRequest(csGotData);

		return m_bIsShoulRespond;
	}
private:
	ParametersStruct m_params;
	bool m_bIsShoulRespond{false};
	size_t m_nCurrentIndex{0};
};

TEST(HTTPDownloaderTests, BaseLengthSpecifiedTest)
{
	struct LengthSpecificParams : public BaseParams{
		LengthSpecificParams() : BaseParams(
			"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: 10\r\n\r\n1234567890",
			"1234567890",
			"www.my.site.com",
			"/some/file.html"
		)
		{
		}
	} params;

	Downloaders::CHTTPDownloader<TestSocket<LengthSpecificParams>> downloader;
	const auto cGotData = downloader.Download("http://www.my.site.com/some/file.html");
	
	HTTP::CHTTPResponse cRealResponse;
	cRealResponse.LoadAll(std::vector<char>{params.sDataToUse.begin(), params.sDataToUse.end()});
	
	ASSERT_TRUE(cGotData);

	ASSERT_EQ(cGotData->GetCode(), cRealResponse.GetCode());
	ASSERT_EQ(cGotData->GetHeaders(), cRealResponse.GetHeaders());
	ASSERT_EQ(cGotData->GetData(), cRealResponse.GetData());
}

TEST(HTTPDownloaderTests, ChunkedSpecifiedTest)
{
	struct ChunkedParameters : public BaseParams{
		ChunkedParameters() : BaseParams(
			"HTTP/1.1 200 OK\r\n"
			"more-random-header: value\r\nsome-header: value_of_header\r\nTransfer-Encoding: chunked\r\n\r\n"
			"A\r\n1234567890\r\nC\r\n 12 14 18 15\r\n0\r\n",
			"1234567890 12 14 18 15",
			"ebay-bebay.com",
			"/"
		)
		{
		}
	} params;

	Downloaders::CHTTPDownloader<TestSocket<ChunkedParameters>> downloader;
	const auto cGotData = downloader.Download("some-proto://ebay-bebay.com");
	
	HTTP::CHTTPResponse cRealResponse;
	cRealResponse.LoadHeaders(std::vector<char>{params.sDataToUse.begin(), params.sDataToUse.end()});

	ASSERT_TRUE(cGotData);

	const std::vector<char> cRealData
		{params.sRealResponseData.begin(), params.sRealResponseData.end()};

	ASSERT_EQ(cGotData->GetCode(), cRealResponse.GetCode());
	ASSERT_EQ(cGotData->GetHeaders(), cRealResponse.GetHeaders());
	ASSERT_EQ(cGotData->GetData(), cRealData);
}

#define BIG_RESPONSE_DATA \
"123456789012 14 18 15 a lot of different meaningfull data, realy big amount of it, " \
"lorem ipsum Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore " \
"magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " \
"laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit " \
"in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur " \
"sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."

TEST(HTTPDownloaderTests, NoSizeSpecifiedTest)
{
	struct NoSizeParameters : public BaseParams{
		NoSizeParameters() : BaseParams(
			"HTTP/1.1 200 OK\r\n"
			"more-random-header: value\r\nsome-header: value_of_header\r\n\r\n" BIG_RESPONSE_DATA,
			BIG_RESPONSE_DATA,
			"ebay-bebay.com",
			"/"
		)
		{
		}
	} params;

	Downloaders::CHTTPDownloader<TestSocket<NoSizeParameters>> downloader;
	const auto cGotData = downloader.Download("some-proto://ebay-bebay.com?q=cool+films");
	
	HTTP::CHTTPResponse cRealResponse;
	cRealResponse.LoadHeaders(std::vector<char>{params.sDataToUse.begin(), params.sDataToUse.end()});

	ASSERT_TRUE(cGotData);

	const std::vector<char> cRealData
		{params.sRealResponseData.begin(), params.sRealResponseData.end()};

	ASSERT_EQ(cGotData->GetCode(), cRealResponse.GetCode());
	ASSERT_EQ(cGotData->GetHeaders(), cRealResponse.GetHeaders());
	ASSERT_EQ(cGotData->GetData(), cRealData);
}

TEST(HTTPDownloaderTests, FailWrongAddressTests)
{
	struct SimpleParams : public BaseParams{
		SimpleParams() : BaseParams(
			"HTTP/1.1 200 OK\r\n"
			"more-random-header: value\r\nsome-header: value_of_header\r\nContent-Length: 10\r\n\r\n"
			"1234567890",
			"1234567890",
			"ebay-bebay.com",
			"/"
		)
		{
		}
	} firstParams;

	Downloaders::CHTTPDownloader<TestSocket<SimpleParams>> downloader;

	auto gotData = downloader.Download("some-proto:/ebay-bebay.com");
	ASSERT_FALSE(gotData);

	gotData = downloader.Download(":ebay-bebay.com");
	ASSERT_FALSE(gotData);

	gotData = downloader.Download("/address/not/right.com/real/path");
	ASSERT_FALSE(gotData);
}

TEST(HTTPDownloaderTests, FailChunkTests)
{
	struct NoEndChunkParams : public BaseParams{
		NoEndChunkParams() : BaseParams(
			"HTTP/1.1 200 OK\r\n"
			"more-random-header: value\r\nsome-header: value_of_header\r\nTransfer-Encoding: chunked\r\n\r\n"
			"A\r\n1234567890\r\nC\r\n 12 14 18 15\r\n",
			"1234567890 12 14 18 15",
			"ebay-bebay.com",
			"/"
		)
		{
		}
	} firstParams;

	Downloaders::CHTTPDownloader<TestSocket<NoEndChunkParams>> downloader;
	auto gotData = downloader.Download("some-proto://ebay-bebay.com");
	
	HTTP::CHTTPResponse cRealResponse;
	cRealResponse.LoadHeaders(
		std::vector<char>{firstParams.sDataToUse.begin(), firstParams.sDataToUse.end()});

	ASSERT_FALSE(gotData);
	
	struct WrongLengthChunkParams : public BaseParams{
		WrongLengthChunkParams() : BaseParams(
			"HTTP/1.1 200 OK\r\n"
			"more-random-header: value\r\nsome-header: value_of_header\r\nTransfer-Encoding: chunked\r\n\r\n"
			"A\r\n1234567890\r\nAC\r\n 12 14 18 15\r\n",
			"1234567890 12 14 18 15",
			"ebay-bebay.com",
			"/"
		)
		{
		}
	} secondParams;

	Downloaders::CHTTPDownloader<TestSocket<WrongLengthChunkParams>> secondDownloader;
	gotData = secondDownloader.Download("some-proto://ebay-bebay.com");
	
	cRealResponse.LoadHeaders(std::vector<char>{secondParams.sDataToUse.begin(), secondParams.sDataToUse.end()});

	ASSERT_FALSE(gotData);
}

TEST(HTTPDownloaderTests, FailLengthTests)
{
	struct WrongLengthSpecified : public BaseParams{
		WrongLengthSpecified() : BaseParams(
			"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: 100\r\n\r\n1234567890",
			"1234567890",
			"www.my.site.com",
			"/some/file.html"
		)
		{
		}
	};

	Downloaders::CHTTPDownloader<TestSocket<WrongLengthSpecified>> downloader;
	auto cGotData = downloader.Download("http://www.my.site.com/some/file.html");
	ASSERT_FALSE(cGotData);

	struct WrongFormatSpecified : public BaseParams{
		WrongFormatSpecified() : BaseParams(
			"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: AAAAA\r\n\r\n1234567890",
			"1234567890",
			"www.my.site.com",
			"/some/file.html"
		)
		{
		}
	};

	Downloaders::CHTTPDownloader<TestSocket<WrongFormatSpecified>> secondDownloader;
	cGotData = secondDownloader.Download("http://www.my.site.com/some/file.html");
	ASSERT_FALSE(cGotData);
}
