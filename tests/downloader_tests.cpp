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
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(cResponseHeadData));

	EXPECT_CALL(*pTestSocket, ReadTillEnd)
		.Times(1)
		.InSequence(seq)
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

TEST(HTTPDownloaderTests, FailWrongAddressTests)
{
	const CURI cURIToConnect{"some-proto://ebay-bebay.com?q=cool+films"};
	const std::string csRequestHost{"ebay-bebay.com"};
	const std::string csRequestPath{"/?q=cool+films"};

	std::unique_ptr<TestSocket> pTestSocket{new TestSocket()};
	EXPECT_CALL(*pTestSocket, Connect)
		.Times(1)
		.WillRepeatedly(testing::Return(false));

	Downloaders::CHTTPDownloader downloader{std::move(pTestSocket)};

	auto gotData = downloader.Download(CURI("some-proto:/ebay-bebay.com"));
	ASSERT_FALSE(gotData);

	pTestSocket.reset(new TestSocket());
	EXPECT_CALL(*pTestSocket, Connect)
		.Times(1)
		.WillRepeatedly(testing::Return(false));
	 downloader = Downloaders::CHTTPDownloader(std::move(pTestSocket));

	gotData = downloader.Download(CURI(":ebay-bebay.com"));
	ASSERT_FALSE(gotData);

	pTestSocket.reset(new TestSocket());
	EXPECT_CALL(*pTestSocket, Connect)
		.Times(1)
		.WillRepeatedly(testing::Return(false));
	 downloader = Downloaders::CHTTPDownloader(std::move(pTestSocket));

	gotData = downloader.Download(CURI("/address/not/right.com/real/path"));
	ASSERT_FALSE(gotData);
}

TEST(HTTPDownloaderTests, FailChunkTests)
{
	std::string sResponseHead =
			"HTTP/1.1 200 OK\r\n"
			"more-random-header: value\r\nsome-header: value_of_header\r\nTransfer-Encoding: chunked\r\n\r\n";
	std::vector<char> responseHeadData
		{sResponseHead.begin(), sResponseHead.end()};

	std::string sResponseBody
		{"1234567890\r\n 12 14 18 15\r\n"};
	std::vector<char> responseBodyData
		{sResponseBody.begin(), sResponseBody.end()};

	const CURI cURIToConnect{"some-proto://ebay-bebay.com"};
	const std::string csRequestHost{"ebay-bebay.com"};
	const std::string csRequestPath{"/"};

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
		.WillOnce(testing::Return(responseHeadData));

	EXPECT_CALL(*pTestSocket, ReadTill(CRLF))
		.Times(3)
		.WillOnce(testing::Return(std::vector<char>{'A'}))
		.WillOnce(testing::Return(std::vector<char>{'C'}))
		.WillOnce(testing::Return(std::nullopt));

	EXPECT_CALL(*pTestSocket, ReadCount)
		.Times(2)
		.WillOnce(testing::Return(std::vector<char>{responseBodyData.begin(), responseBodyData.begin() + 12}))
		.WillOnce(testing::Return(std::vector<char>{responseBodyData.begin() + 13, responseBodyData.end()}));

	Downloaders::CHTTPDownloader downloader{std::move(pTestSocket)};
	auto gotData = downloader.Download(cURIToConnect);

	ASSERT_FALSE(gotData);

	sResponseHead = 
		"HTTP/1.1 200 OK\r\n"
		"more-random-header: value\r\nsome-header: value_of_header\r\nTransfer-Encoding: chunked\r\n\r\n";

	responseHeadData = {sResponseHead.begin(), sResponseHead.end()};

	sResponseBody = "1234567890\r\n 12 14 18 15\r\n";
	responseBodyData = {sResponseBody.begin(), sResponseBody.end()};

	::testing::Sequence seq2;
	 pTestSocket.reset(new TestSocket());

	EXPECT_CALL(*pTestSocket, Connect(::testing::Eq(cURIToConnect)))
		.Times(1)
		.InSequence(seq2)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, Write)
		.With(IsRightRequest(csRequestHost, csRequestPath))
		.Times(1)
		.InSequence(seq2)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, ReadTill(CRLFCRLF))
		.Times(1)
		.InSequence(seq2)
		.WillOnce(testing::Return(responseHeadData));

	EXPECT_CALL(*pTestSocket, ReadTill(CRLF))
		.Times(2)
		.WillOnce(testing::Return(std::vector<char>{'A'}))
		.WillOnce(testing::Return(std::vector<char>{'A', 'C'}));

	EXPECT_CALL(*pTestSocket, ReadCount)
		.Times(2)
		.WillOnce(testing::Return(std::vector<char>{responseBodyData.begin(), responseBodyData.begin() + 12}))
		.WillOnce(testing::Return(std::nullopt));


	downloader = Downloaders::CHTTPDownloader(std::move(pTestSocket));
	gotData = downloader.Download(cURIToConnect);

	ASSERT_FALSE(gotData);
}

TEST(HTTPDownloaderTests, FailLengthTests)
{
	std::string sResponseHead =
			"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: 100\r\n\r\n1234567890";
	std::vector<char> responseHeadData
		{sResponseHead.begin(), sResponseHead.end()};

	const CURI cURIToConnect{"www.my.site.com/some/file.html"};
	const std::string csRequestHost{"www.my.site.com"};
	const std::string csRequestPath{"/some/file.html"};

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
		.WillOnce(testing::Return(responseHeadData));

	EXPECT_CALL(*pTestSocket, ReadCount(100))
		.Times(1)
		.InSequence(seq)
		.WillOnce(testing::Return(std::nullopt));

	Downloaders::CHTTPDownloader downloader{std::move(pTestSocket)};

	auto cGotData = downloader.Download(cURIToConnect);
	ASSERT_FALSE(cGotData);

	sResponseHead = "HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: AAAAA\r\n\r\n1234567890";
	responseHeadData = {sResponseHead.begin(), sResponseHead.end()};

	pTestSocket.reset(new TestSocket());

	::testing::Sequence seq2;

	EXPECT_CALL(*pTestSocket, Connect(::testing::Eq(cURIToConnect)))
		.Times(1)
		.InSequence(seq2)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, Write)
		.With(IsRightRequest(csRequestHost, csRequestPath))
		.Times(1)
		.InSequence(seq2)
		.WillOnce(testing::Return(true));

	EXPECT_CALL(*pTestSocket, ReadTill(CRLFCRLF))
		.Times(1)
		.InSequence(seq2)
		.WillOnce(testing::Return(responseHeadData));

	downloader = Downloaders::CHTTPDownloader(std::move(pTestSocket));

	cGotData = downloader.Download(cURIToConnect);
	ASSERT_FALSE(cGotData);
}
