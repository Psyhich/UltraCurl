#include <string>
#include <algorithm>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <gmock/gmock-spec-builders.h>

#include "http_downloader.h"
#include "test_socket.h"
#include "sockets.h"

const std::string CRLFCRLF{"\r\n\r\n"};
const std::string CRLF{"\r\n"};

TEST(HTTPDownloaderTests, BaseLengthSpecifiedTest)
{
	const std::string csResponseHead
		{"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: 10\r\n\r\n"};
	const std::vector<char> cResponseHeadData
		{csResponseHead.begin(), csResponseHead.end()};

	const std::string csResponseBody
		{"1234567890"};
	const std::vector<char> cResponseBodyData
		{csResponseBody.begin(), csResponseBody.end()};


	const CURI cRequestURI{"http://www.my.site.com/some/file.html"};
	const std::string csRequestHost{"www.my.site.com"};
	const std::string csRequestPath{"/some/file.html"};

	// pTestSocket should: 
	// Connect
	// Write a valid request
	// Read till \r\n\r\n
	// Read 10 bytes
	std::unique_ptr<TestSocket> pTestSocket{new TestSocket()};

	::testing::Sequence seq;

	EXPECT_CALL(*pTestSocket, Connect(::testing::Eq(cRequestURI)))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, Write)
		.With(IsRightRequest(csRequestHost, csRequestPath))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, ReadTill(CRLFCRLF))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(cResponseHeadData));
	
	EXPECT_CALL(*pTestSocket, ReadCount(10))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(cResponseBodyData));
	

	Downloaders::CHTTPDownloader downloader{std::move(pTestSocket)};
	const auto cGotData = downloader.Download(CURI(cRequestURI));
	
	ASSERT_TRUE(cGotData);

	HTTP::CHTTPResponse realResponse;
	realResponse.LoadHeaders(cResponseHeadData);
	realResponse.LoadData(std::vector<char>{cResponseBodyData});

	ASSERT_EQ(cGotData->GetCode(), realResponse.GetCode());
	ASSERT_EQ(cGotData->GetHeaders(), realResponse.GetHeaders());
	ASSERT_EQ(cGotData->GetData(), realResponse.GetData());
}

TEST(HTTPDownloaderTests, ChunkedSpecifiedTest)
{
	const std::string csResponseHead =
		"HTTP/1.1 200 OK\r\n"
		"more-random-header: value\r\nsome-header: value_of_header\r\nTransfer-Encoding: chunked\r\n\r\n";
	const std::vector<char> cResponseHeadData
		{csResponseHead.begin(), csResponseHead.end()};

	const std::string csResponseBody
		{"1234567890 12 14 18 15"};
	const std::vector<char> cResponseBodyData
		{csResponseBody.begin(), csResponseBody.end()};

	const CURI cURIToConnect{"ebay-bebay.com"};
	const std::string csRequestHost{"ebay-bebay.com"};
	const std::string csRequestPath{"/"};

	// pTestSocket should: 
	// Connect
	// Write a valid request
	// Read till \r\n\r\n
	// Read till \r\n
	// Read 10 bytes or 12
	// Read till \r\n
	// Read 12 bytes

	std::unique_ptr<TestSocket> pTestSocket{new TestSocket()};

	::testing::Sequence seq;

	EXPECT_CALL(*pTestSocket, Connect(::testing::Eq(cURIToConnect)))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, Write)
		.With(IsRightRequest(csRequestHost, csRequestPath))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, ReadTill(CRLFCRLF))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(cResponseHeadData));

	EXPECT_CALL(*pTestSocket, ReadTill(CRLF))
		.Times(3)
		.WillOnce(testing::Return(std::vector<char>{'A'}))
		.WillOnce(testing::Return(std::vector<char>{'C'}))
		.WillOnce(testing::Return(std::vector<char>{'0'}));

	EXPECT_CALL(*pTestSocket, ReadCount(12))
		.Times(1)
		.WillOnce(testing::Return(
			std::vector<char>{cResponseBodyData.begin(), cResponseBodyData.begin() + 12}));

	;
	EXPECT_CALL(*pTestSocket, ReadCount(14))
		.Times(1)
		.WillOnce(testing::Return(std::vector<char>{cResponseBodyData.begin() + 10, cResponseBodyData.begin() + 24}));

	Downloaders::CHTTPDownloader downloader{std::move(pTestSocket)};
	const auto cGotData = downloader.Download(cURIToConnect);
	
	HTTP::CHTTPResponse cRealResponse;
	cRealResponse.LoadHeaders(cResponseHeadData);

	ASSERT_TRUE(cGotData);

	ASSERT_EQ(cGotData->GetCode(), cRealResponse.GetCode());
	ASSERT_EQ(cGotData->GetHeaders(), cRealResponse.GetHeaders());
	ASSERT_EQ(cGotData->GetData(), cResponseBodyData);
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
	const std::string csResponseHead =
			"HTTP/1.1 200 OK\r\n"
			"more-random-header: value\r\nsome-header: value_of_header\r\n\r\n";
	const std::vector<char> cResponseHeadData
		{csResponseHead.begin(), csResponseHead.end()};

	const std::string csResponseBody
		{BIG_RESPONSE_DATA};
	const std::vector<char> cResponseBodyData
		{csResponseBody.begin(), csResponseBody.end()};

	const CURI cURIToConnect{"some-proto://ebay-bebay.com?q=cool+films"};
	const std::string csRequestHost{"ebay-bebay.com"};
	const std::string csRequestPath{"/?q=cool+films"};

	std::unique_ptr<TestSocket> pTestSocket{new TestSocket()};

	::testing::Sequence seq;

	EXPECT_CALL(*pTestSocket, Connect(::testing::Eq(cURIToConnect)))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, Write)
		.With(IsRightRequest(csRequestHost, csRequestPath))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, ReadTill(CRLFCRLF))
		.Times(2)
		.InSequence(seq)
		.WillOnce(testing::Return(cResponseHeadData))
		.WillOnce(testing::Return(cResponseBodyData));

	Downloaders::CHTTPDownloader downloader{std::move(pTestSocket)};
	const auto cGotData = downloader.Download(cURIToConnect);
	
	HTTP::CHTTPResponse cRealResponse;
	cRealResponse.LoadHeaders(cResponseHeadData);

	ASSERT_TRUE(cGotData);

	ASSERT_EQ(cGotData->GetCode(), cRealResponse.GetCode());
	ASSERT_EQ(cGotData->GetHeaders(), cRealResponse.GetHeaders());
	ASSERT_EQ(cGotData->GetData(), cResponseBodyData);
}

/*
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
*/
