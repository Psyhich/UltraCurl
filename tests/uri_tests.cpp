#include <gtest/gtest.h>

#include <string>

#include "uri.h"

TEST(URIParsingTest, ProtocolParrsingTest)
{
	std::string address = "some-proto://site.com:899?q=Cool+films";
	CURI addressURI(address);

	ASSERT_STREQ(addressURI.GetProtocol()->c_str(), "some-proto");
	
	address = "some-proto://site.com:899?q=Cool+films";
	addressURI = address;

	ASSERT_STREQ(addressURI.GetProtocol()->c_str(), "some-proto");
	
	address = "site.com:899?q=Cool+films";
	addressURI = address;

	ASSERT_EQ(addressURI.GetProtocol(), std::nullopt);
	
	address = "http//site.com:899?q=Cool+films";
	addressURI = address;

	ASSERT_EQ(addressURI.GetProtocol(), std::nullopt);
	
	address = "url:://site.com";
	addressURI = address;

	ASSERT_EQ(addressURI.GetProtocol(), std::nullopt);
	
	address = "u,rl://site.com";
	addressURI = address;

	ASSERT_EQ(addressURI.GetProtocol(), std::nullopt);
}

TEST(URIParsingTest, AddressParsingTest)
{
	std::string address = "blob://some.random.address.com:8999/path/to/file.txt?q=Text#sample";
	CURI addressURI(address);

	ASSERT_STREQ(addressURI.GetFullURI().c_str(), address.c_str());
	ASSERT_STREQ(addressURI.GetPureAddress()->c_str(), "some.random.address.com");

	address = "random_proto://really.original.address.org/path/to/file.txt";
	addressURI = address;

	ASSERT_STREQ(addressURI.GetFullURI().c_str(), address.c_str());
	ASSERT_STREQ(addressURI.GetPureAddress()->c_str(), "really.original.address.org");

	address = "surrelly.not.a.log4j.server.com";
	addressURI = address;

	ASSERT_STREQ(addressURI.GetFullURI().c_str(), address.c_str());
	ASSERT_STREQ(addressURI.GetPureAddress()->c_str(), address.c_str());
}

TEST(URIParsingTest, PortParssingTest)
{
	std::string address = "some_proto://site.com:899?q=Cool+films";
	CURI addressURI(address);

	ASSERT_EQ(*addressURI.GetPort(), 899);

	address = "some_proto://site.com:89999999?q=Cool+films";
	addressURI = address;

	ASSERT_EQ(addressURI.GetPort(), std::nullopt);

	address = "default.proto.test.com#some_div";
	addressURI = address;

	ASSERT_EQ(*addressURI.GetPort(), 80);

	address = "default.proto.test.com?q=some_div";
	addressURI = address;

	ASSERT_EQ(*addressURI.GetPort(), 80);

	address = "default.proto.test.com";
	addressURI = address;

	ASSERT_EQ(*addressURI.GetPort(), 80);

	address = "default.proto.test.com:1000#some_div";
	addressURI = address;

	ASSERT_EQ(*addressURI.GetPort(), 1000);

	address = "default.proto.test.com:1000";
	addressURI = address;

	ASSERT_EQ(*addressURI.GetPort(), 1000);

	address = "default.proto.test.com:notAPort100";
	addressURI = address;

	ASSERT_EQ(addressURI.GetPort(), std::nullopt);
}

TEST(URIParsingTest, PathParsingTest)
{
	std::string address = "some-proto://site.com:899/some/random/path/page.html?q=Cool+films";
	CURI addressURI(address);

	ASSERT_STREQ(addressURI.GetPath()->c_str(), "/some/random/path/page.html");

	address = "site.com:899/some/more/random/path/page.html#films";
	addressURI = address;

	ASSERT_STREQ(addressURI.GetPath()->c_str(), "/some/more/random/path/page.html");

	address = "site.com/some/path/page.html";
	addressURI = address;

	ASSERT_STREQ(addressURI.GetPath()->c_str(), "/some/path/page.html");

	address = "site.com/";
	addressURI = address;

	ASSERT_EQ(addressURI.GetPath(), std::nullopt);

	address = "some-proto://site.com/?q=Cute+kitties";
	addressURI = address;

	ASSERT_EQ(addressURI.GetPath(), std::nullopt);

	address = "some-proto://site.com#cool_div";
	addressURI = address;

	ASSERT_EQ(addressURI.GetPath(), std::nullopt);
}

TEST(URIParsingTest, QueryParsingTest)
{
	CURI URIToParse("some-proto://site.com:899/some/random/path/page.html?q=Cool+films");

	auto sQuery = URIToParse.GetQuery();
	ASSERT_STREQ(sQuery->c_str(), "q=Cool+films");

	URIToParse = CURI("some-proto://site.com/?q=Cute+kitties#kittie_1");
	sQuery = URIToParse.GetQuery();
	ASSERT_STREQ(sQuery->c_str(), "q=Cute+kitties");

	URIToParse = CURI("rand-proto://www.www.www.www.page.cool.name.org?login=admin;password=admin#logon_page");
	sQuery = URIToParse.GetQuery();
	ASSERT_STREQ(sQuery->c_str(), "login=admin;password=admin");

	URIToParse = CURI("rand-proto://www.www.www.www.page.cool.name.org?#logon_page");
	sQuery = URIToParse.GetQuery();
	ASSERT_EQ(sQuery, std::nullopt);

	URIToParse = CURI("rand-proto://www.www.www.www.page.cool.name.org");
	sQuery = URIToParse.GetQuery();
	ASSERT_EQ(sQuery, std::nullopt);
}

TEST(URIParsingTest, FragmentParsingTest)
{
	CURI URIToParse("some-proto://site.com:899/some/random/path/page.html#page_1");

	auto sFragment = URIToParse.GetFragment();
	ASSERT_STREQ(sFragment->c_str(), "page_1");

	URIToParse = CURI("some-proto://site.com/?q=Cute+kitties#kittie_1");
	sFragment = URIToParse.GetFragment();
	ASSERT_STREQ(sFragment->c_str(), "kittie_1");

	URIToParse = CURI("rand-proto://www.www.www.www.page.cool.name.org?login=admin;password=admin#logon_page");
	sFragment = URIToParse.GetFragment();
	ASSERT_STREQ(sFragment->c_str(), "logon_page");

	URIToParse = CURI("rand-proto://www.www.www.www.page.cool.name.org?login=admin;password=admin");
	sFragment = URIToParse.GetFragment();
	ASSERT_EQ(sFragment, std::nullopt);

	URIToParse = CURI("rand-proto://www.www.www.www.page.cool.name.org");
	sFragment = URIToParse.GetFragment();
	ASSERT_EQ(sFragment, std::nullopt);

	URIToParse = CURI("rand-proto://www.www.www.www.page.cool.name.org#");
	sFragment = URIToParse.GetFragment();
	ASSERT_EQ(sFragment, std::nullopt);
}
