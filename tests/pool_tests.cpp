#include <gtest/gtest.h>

#include "downloader_pool.h"
#include "uri.h"
#include "fake_socket.h"

TEST(DownloaderPoolTests, BaseWorkingTests)
{
	struct TestRouter : RoutingTables 
	{
		TestRouter() 
		{
			tables = {
			{
					CURI("http://www.google.com"),
					BaseParams(
						"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: 10\r\n\r\n1234567890",
						"1234567890",
						"www.google.com",
						"/"
					)
				},
				{
					CURI("www.debian.org"),
					BaseParams(
						"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: 12\r\n\r\n123456789011",
						"123456789011",
						"www.debian.org",
						"/"
					)
				},
				{
					CURI("some.other.link.gov/some/path/to/file.html"),
					BaseParams(
						"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: 12\r\n\r\n123456789011",
						"123456789011",
						"some.other.link.gov",
						"/some/path/to/file.html"
					)
				},
				{
					CURI("some.link.com/file.html"),
					BaseParams(
						"HTTP/1.1 200 OK\r\nsome-header: value_of_header\r\nContent-Length: 12\r\n\r\n123456789011",
						"123456789011",
						"some.link.com",
						"/file.html"
					)
				},
				{
					CURI("ebay-bebay.com"),
					BaseParams(
						"HTTP/1.1 200 OK\r\n"
						"more-random-header: value\r\nsome-header: value_of_header\r\nTransfer-Encoding: chunked\r\n\r\n"
						"A\r\n1234567890\r\nC\r\n 12 14 18 15\r\n0\r\n",
						"1234567890 12 14 18 15",
						"ebay-bebay.com",
						"/"
					)
				},
				{
					CURI("ebay-bebay.com/some/random/path/index.html"),
					BaseParams(
						"HTTP/1.1 200 OK\r\n"
						"more-random-header: value\r\nsome-header: value_of_header\r\nTransfer-Encoding: chunked\r\n\r\n"
						"A\r\n1234567890\r\nC\r\n 12 14 18 15\r\nA\r\n1234567890\r\n0\r\n",
						"1234567890 12 14 18 151234567890",
						"ebay-bebay.com",
						"/some/random/path/index.html"
					)
				}
			};
		}
	} router;

	auto concurentDownloader = 
		Downloaders::Concurrency::CConcurrentDownloader::AllocatePool(
		[](const CURI &)
		{
			return std::unique_ptr<RouterSocket<TestRouter>>(new RouterSocket<TestRouter>());
		},
		2);

	std::map<CURI, std::vector<char>> realResults;
	
	for(const auto &pair : router.tables)
	{
		concurentDownloader->AddNewTask(pair.first, 
			[&](auto gotData) -> bool
			{
				if(gotData)
				{
					realResults.insert({pair.first, gotData->GetData()});
				}
				return false;
			});
	}

	std::map<CURI, std::vector<char>> desiredResults;
	for(const auto &pair : router.tables)
	{
		std::vector<char> convertedData
			{pair.second.sRealResponseData.begin(), pair.second.sRealResponseData.end()};
		desiredResults.insert({pair.first, convertedData});
	}

	concurentDownloader->JoinTasksCompletion();

	for(const auto &pair : desiredResults)
	{
		const auto foundResult = realResults.find(pair.first);
		ASSERT_FALSE(foundResult == realResults.end());

		ASSERT_EQ(pair.second, foundResult->second);
	}

	ASSERT_EQ(realResults, desiredResults);
}
