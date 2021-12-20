#include <cstdio>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <cstring>
#include <stdio.h>

#include "sockets.h"

Sockets::CSocket::CSocket::CSocket(const std::string &addressToUse) : 
	m_address{addressToUse} 
{
}

std::optional<uint16_t> Sockets::CTcpSocket::ExtractPortInByteOrder(
	const URI &cURI) noexcept
{
	if(const auto cPort = cURI.GetPort())
	{
		return htons(*cPort);
	}
	return htons(80);
}

std::optional<sockaddr> Sockets::CTcpSocket::GetSocketAddress(
	const URI &cURI) noexcept
{
	if(const auto cAddress = cURI.GetPureAddress())
	{
		// Setting hint to look for host(protocol, socket type and IPv4)
		addrinfo addressHint;
		std::memset(&addressHint, 0, sizeof(addressHint));
		addressHint.ai_family = AF_INET;
		addressHint.ai_socktype = SOCK_STREAM;
		addressHint.ai_protocol = 0;

		// Creating pointer for array of resolved hosts(we would need only first one)
		addrinfo *ppResolvedHosts = nullptr;
		if(getaddrinfo(cAddress->c_str(), HTTP_SERVICE, &addressHint, &ppResolvedHosts) != 0 || 
			ppResolvedHosts == nullptr)
		{
			fprintf(stderr, "Failed to resolve given address\n");
			return std::nullopt;
		}
		const sockaddr cFirstAddress = *ppResolvedHosts[0].ai_addr;
		freeaddrinfo(ppResolvedHosts);
		return cFirstAddress;
	}
	return std::nullopt;
}


Sockets::CTcpSocket::CTcpSocket(
	const std::string& addressToUse) : CSocket(addressToUse)
{
	// Getting address info to know which IP protocol to use
	std::optional<sockaddr> cResolvedAddress = GetSocketAddress(GetAddress());
	if(!cResolvedAddress)
	{
		fprintf(stderr, "Failed to resolve given address\n");
		return;
	}

	std::optional<uint16_t> uiPort = GetAddress().GetPort();
	// If we got wrongly written port returning nullopt
	if(!uiPort)
	{
		fprintf(stderr, "Given port is invalid\n");
		return;
	}
	
	// Creating socket
	m_iSocketFD = socket(cResolvedAddress->sa_family, SOCK_STREAM, 0);
	if(m_iSocketFD == -1)
	{
		fprintf(stderr, "Failed to create socket\n");
		return;
	}

	// Setting specified port for resolved address
	((sockaddr_in&)*cResolvedAddress).sin_port = *uiPort;

	if(connect(m_iSocketFD, &*cResolvedAddress, sizeof(*cResolvedAddress)) == -1)
	{
		fprintf(stderr, "Failed to connect to given address\n");
		close(m_iSocketFD);
		m_iSocketFD = -1;
		return;
	}
}

Sockets::CTcpSocket::~CTcpSocket()
{
	if(m_iSocketFD != -1)
	{
		close(m_iSocketFD);
	}
}

std::optional<std::vector<char>> Sockets::CTcpSocket::ReadTillEnd() noexcept
{
	if(m_iSocketFD == -1)
	{
		fprintf(stderr, "The given socket doesn't exist or it's not opened\n");
		return std::nullopt;
	}
	
	char chBuffer[BUFFER_SIZE];
	memset(chBuffer, 0, sizeof(char) * BUFFER_SIZE);

	std::vector<char> messageVector;
	messageVector.reserve(BUFFER_SIZE * 4);

	// If we recive same count as BUFFER_SIZE, we can request new block of data
	ssize_t nRecivedBytes;
	do
	{
		nRecivedBytes = recv(m_iSocketFD, chBuffer, sizeof(char) * BUFFER_SIZE, 0);
		if(nRecivedBytes == -1)
		{
			fprintf(stderr, "Error while reciving data\n");
			return std::nullopt;
		} else if(nRecivedBytes == 0)
		{
			// FIXME: rarely block of recived data can be equeal 
			// To BUFFER_SIZE and it will roll into such error
			fprintf(stderr, "Closed before reciving all\n");
		}
		for(ssize_t nCount = 0; nCount < nRecivedBytes; nCount++)
		{
			messageVector.push_back(chBuffer[nCount]);
		}

	} while(nRecivedBytes == BUFFER_SIZE);

	messageVector.shrink_to_fit();
	return messageVector;
}

bool Sockets::CTcpSocket::Write(const char *pcchBytes, size_t nCount) noexcept
{
	if(m_iSocketFD == -1)
	{
		fprintf(stderr, "The given socket doesn't exist or it's not opened\n");
		return false;
	}

	size_t nBytesToSend = nCount;
	while(nBytesToSend != 0)
	{
		ssize_t nSentBytes = 
			send(m_iSocketFD, pcchBytes + (nCount - nBytesToSend), nBytesToSend, 0);
		if(nSentBytes == -1)
		{
			fprintf(stderr, "Failed to send data\n");
			return false;
		} else if(nSentBytes == 0)
		{
			fprintf(stderr, "The connection was closed\n");
			return false;
		} else 
		{
			nBytesToSend -= (size_t)nSentBytes;
		}
	}
	return true;
}
