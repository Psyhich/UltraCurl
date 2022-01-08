#include <string>

#include <gtest/gtest.h>

#include "http_downloader.h"
#include "sockets.h"

#include "test_socket.h"


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
	const auto cGotData = downloader.Download(CURI("http://www.my.site.com/some/file.html"));
	
	ASSERT_TRUE(cGotData);

	HTTP::CHTTPResponse cRealResponse;
	cRealResponse.LoadAll(std::vector<char>{params.sDataToUse.begin(), params.sDataToUse.end()});

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
	const auto cGotData = downloader.Download(CURI("some-proto://ebay-bebay.com"));
	
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
	const auto cGotData = downloader.Download(CURI("some-proto://ebay-bebay.com?q=cool+films"));
	
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

	auto gotData = downloader.Download(CURI("some-proto:/ebay-bebay.com"));
	ASSERT_FALSE(gotData);

	gotData = downloader.Download(CURI(":ebay-bebay.com"));
	ASSERT_FALSE(gotData);

	gotData = downloader.Download(CURI("/address/not/right.com/real/path"));
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
	auto gotData = downloader.Download(CURI("some-proto://ebay-bebay.com"));
	
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
	gotData = secondDownloader.Download(CURI("some-proto://ebay-bebay.com"));
	
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
	auto cGotData = downloader.Download(CURI("http://www.my.site.com/some/file.html"));
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
	cGotData = secondDownloader.Download(CURI("http://www.my.site.com/some/file.html"));
	ASSERT_FALSE(cGotData);
}
