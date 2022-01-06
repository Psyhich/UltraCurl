#include <vector>
#include <string>

#include <gtest/gtest.h>

#include "response.h"

class ResponseTests : public ::testing::Test {
public:
	std::vector<char> ConvertIntoVector(std::string &&sStringToConvert)
	{
		return std::vector<char>{sStringToConvert.begin(), sStringToConvert.end()};
	}
};

TEST_F(ResponseTests, SuccessFullLoadResponseTest)
{
	// Also testing situation when headers are splitted with just LF not CRLF
	const std::vector<char> cResponseBytes = ConvertIntoVector(
		"HTTP/1.1 200 OK\r\ncontent-encoding: gzip\r\ninteresting: yes\nborring: no\r\n\r\nrandom bytes of data");

	const std::vector<char> cRightVariantOfData{ConvertIntoVector("random bytes of data")};
	const HTTP::Headers cRightHeaders{
		{"content-encoding", "gzip"}, {"interesting", "yes"}, {"borring", "no"}};


	HTTP::CHTTPResponse cServerResponse;
	const bool cbIsLoaded = cServerResponse.LoadAll(cResponseBytes);
	ASSERT_TRUE(cbIsLoaded);

	ASSERT_EQ(cServerResponse.GetCode(), 200);
	ASSERT_TRUE(cServerResponse.IsSuccess());

	ASSERT_EQ(cServerResponse.GetHeaders(), cRightHeaders);

	ASSERT_EQ(cServerResponse.GetData(), cRightVariantOfData);
}

TEST_F(ResponseTests, PartialLoadingTest)
{
	// Data contained in next vector this is just test, so it wont assume that data is a header
	const std::vector<char> cResponseHeaders = ConvertIntoVector(
			"HTTP/1.1 304 Not Modified\r\nis-realy-modified: yes\r\nrealy-realy: yes\r\n\r\nsome random bytes of data"
		);
	const std::vector<char> cResponseData = ConvertIntoVector(
			"some random bytes of data"
		);

	const HTTP::Headers cRightHeaders{
		{"is-realy-modified","yes"},
		{"realy-realy","yes"}
	};
	HTTP::CHTTPResponse parsedResponse;

	const bool cbIsLoaded = parsedResponse.LoadHeaders(cResponseHeaders);
	ASSERT_TRUE(cbIsLoaded);

	ASSERT_FALSE(parsedResponse.IsSuccess());
	ASSERT_EQ(parsedResponse.GetCode(), 304);

	ASSERT_EQ(parsedResponse.GetHeaders(), cRightHeaders);

	parsedResponse.LoadData(cResponseData);
	ASSERT_EQ(parsedResponse.GetData(), cResponseData);
}

TEST_F(ResponseTests, FailLoadingTests)
{
	std::vector<char> responseHeaders = ConvertIntoVector(
			"HTTP/1.1 403 Bad Request\r\nis-realy-bad: yes\r\n: yes\r\n\r\n"
		);

	HTTP::CHTTPResponse serverResponse;

	bool cbIsLoaded = serverResponse.LoadHeaders(responseHeaders);
	ASSERT_FALSE(cbIsLoaded);

	responseHeaders = ConvertIntoVector(
		"HTTP/1.1 not a code) This is not a response\n"
	);
	cbIsLoaded = serverResponse.LoadAll(responseHeaders);
	ASSERT_FALSE(cbIsLoaded);

	cbIsLoaded = serverResponse.LoadHeaders(responseHeaders);
	ASSERT_FALSE(cbIsLoaded);

	responseHeaders = ConvertIntoVector(
		"HTTP/1.1 202 This is not a response\r\nsome-random_header: false\r\nnot-a-response: yes\n wrong data segment"
	);
	cbIsLoaded = serverResponse.LoadHeaders(responseHeaders);
	ASSERT_FALSE(cbIsLoaded);

	cbIsLoaded = serverResponse.LoadAll(responseHeaders);
	ASSERT_FALSE(cbIsLoaded);
}
