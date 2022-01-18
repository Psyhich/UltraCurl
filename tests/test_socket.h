#ifndef TEST_SOCKET_H
#define TEST_SOCKET_H

#include <gmock/gmock.h>

#include "sockets.h"

bool CheckRequest(const std::string &sRequest, const std::string& sAddress, const std::string &sPath);

MATCHER_P2(IsRightRequest, sAddress, sPath, "")
{
	std::string sRequest;
	sRequest.append(std::get<0>(arg), std::get<1>(arg));
	return CheckRequest(sRequest, sAddress, sPath);
}

class TestSocket : public Sockets::CSocket 
{
public:
	using PossibleData = std::optional<std::vector<char>>;

	MOCK_METHOD(bool, Connect, (const CURI& cURITOConnect), (noexcept, override));

	MOCK_METHOD(PossibleData, ReadTill, (const std::string& sReadTillString), (noexcept, override));
	MOCK_METHOD(PossibleData, ReadCount, (size_t nCountOfBytes), (noexcept, override));
	MOCK_METHOD(PossibleData, ReadTillEnd, (), (noexcept, override));

	MOCK_METHOD(bool, Write, (const char* pcBytesToWrite, size_t nBytesCount), (noexcept, override));
};


#endif // TEST_SOCKET_H
