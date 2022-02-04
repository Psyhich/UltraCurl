#include <netdb.h>

#include <cstring>

#include "network_socket.h"

std::optional<int> Sockets::CNetworkSocket::ExtractServicePort(const CURI &cURIToExtract) noexcept
{
	// Getting right port 
	// Firstly, getting port 
	// if not possible , we try to get protocol, 
	if(const auto ciAvailablePort = cURIToExtract.GetPort())
	{
		return *ciAvailablePort;
	}

	if(const auto csProto = cURIToExtract.GetProtocol())
	{
		servent *service = getservbyname(csProto->c_str(), "tcp");
		if(service != nullptr)
		{
			return htons(service->s_port);
		}
	}	
	return std::nullopt;
}

std::optional<Sockets::CNetworkSocket::CAddrInfo> 
	Sockets::CNetworkSocket::GetHostAddresses(const CURI& cURIToGetAddress) noexcept
{
	if(const auto cAddress = cURIToGetAddress.GetPureAddress())
	{
		// Setting hint to look for host(protocol, socket type and IPv4)
		addrinfo addressHint;
		std::memset(&addressHint, 0, sizeof(addressHint));
		addressHint.ai_family = AF_INET;
		addressHint.ai_socktype = SOCK_STREAM;
		addressHint.ai_protocol = 0;

		const std::string csService = 
			cURIToGetAddress.GetProtocol().value_or("");

		// Creating pointer for array of resolved hosts(we would need only first one)
		addrinfo *pResolvedHosts = nullptr;
		if(getaddrinfo(cAddress->c_str(), csService.c_str(), &addressHint, &pResolvedHosts) != 0 || 
			pResolvedHosts == nullptr)
		{
			fprintf(stderr, "Failed to resolve given address\n");
			return std::nullopt;
		}
		return CAddrInfo(pResolvedHosts);
	}
	return std::nullopt;
}
