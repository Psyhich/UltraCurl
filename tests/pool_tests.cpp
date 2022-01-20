#include <gtest/gtest.h>

#include "downloader_pool.h"
#include "fake_socket.h"

template<class SocketClass> using Downloader = Downloaders::Concurrency::CConcurrentDownloader<RouterSocket<SocketClass>>;

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
						"www.debian.com",
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

	Downloader<TestRouter> concurentDownloader;
	
	for(auto& [uri, params] : router.tables)
	{
		concurentDownloader.AddNewTask(uri, 
			[&](auto) -> bool
			{
				//ASSERT_EQ(gotData, params.sRealResponseData);
				return false;
			});
	}
}
