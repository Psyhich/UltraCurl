#include <cstdio>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdio.h>
#include <vector>

#include "sockets.h"

Sockets::CSocket::CSocket::CSocket(const std::string &addressToUse) : 
	m_address{addressToUse} 
{
}

std::optional<uint16_t> Sockets::CTcpSocket::ExtractPortInByteOrder() const noexcept
{
	if(const auto cPort = GetAddress().GetPort())
	{
		return htons(*cPort);
	}
	return htons(80);
}

std::optional<sockaddr> Sockets::CTcpSocket::GetSocketAddress() const noexcept
{
	if(const auto cAddress = GetAddress().GetPureAddress())
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
	std::optional<sockaddr> cResolvedAddress = GetSocketAddress();
	if(!cResolvedAddress)
	{
		fprintf(stderr, "Failed to resolve given address\n");
		return;
	}

	std::optional<uint16_t> uiPort = ExtractPortInByteOrder();
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
	sockaddr_in &address = ((sockaddr_in&)*cResolvedAddress);
	address.sin_port = *uiPort;

	if(connect(m_iSocketFD, (&*cResolvedAddress), sizeof(sockaddr)) == -1)
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

Sockets::CTcpSocket::CTcpSocket(CTcpSocket &&socketToMove) : 
	CSocket(socketToMove.GetAddress().GetFullURI())
{
	this->m_iSocketFD = socketToMove.m_iSocketFD;
	socketToMove.m_iSocketFD = -1;
}

Sockets::CTcpSocket& Sockets::CTcpSocket::CTcpSocket::operator =(CTcpSocket &&socketToMove)
{
	if(this == &socketToMove)
	{
		return *this;
	}

	this->m_iSocketFD = socketToMove.m_iSocketFD;
	socketToMove.m_iSocketFD = -1;

	return *this;
}

std::optional<std::vector<char>> Sockets::CTcpSocket::ReadTillEnd() noexcept
{
	if(m_iSocketFD < 0)
	{
		fprintf(stderr, "The given socket doesn't exist or it's not opened\n");
		return std::nullopt;
	}
	
	char chBuffer[BUFFER_SIZE];
	memset(chBuffer, 0, sizeof(char) * BUFFER_SIZE);

	std::vector<char> messageVector;
	messageVector.reserve(BUFFER_SIZE);

	// If we recive same count as BUFFER_SIZE, we can request new block of data
	ssize_t nRecivedBytes;
	do
	{
		nRecivedBytes = recv(m_iSocketFD, chBuffer, sizeof(char) * BUFFER_SIZE, 0);
		if(nRecivedBytes == -1)
		{
			fprintf(stderr, "Error while reciving data\n");
			return std::nullopt;
		}

		for(ssize_t nCount = 0; nCount < nRecivedBytes; nCount++)
		{
			messageVector.push_back(chBuffer[nCount]);
		}

	} while(nRecivedBytes != 0);

	messageVector.shrink_to_fit();
	return messageVector;
}


std::optional<std::vector<char>> Sockets::CTcpSocket::ReadTill(const std::string &csStringToReadTill) noexcept
{
	if(m_iSocketFD < 0)
	{
		fprintf(stderr, "The given socket doesn't exist or it's not opened\n");
		return std::nullopt;
	}
	if(csStringToReadTill.size() < 1)
	{
		return std::nullopt;
	}
	size_t nCurrentStringIndex = 0;
	
	std::vector<char> readData;

	char chReadChar;
	// TODO: rewrite this socket as adapter with internal buffer
	while(recv(m_iSocketFD, &chReadChar, 1, 0) > 0)
	{
		readData.push_back(chReadChar);
		if(chReadChar == csStringToReadTill[nCurrentStringIndex])
		{
			++nCurrentStringIndex;
			if(nCurrentStringIndex == csStringToReadTill.size())
			{
				return readData;
			}
		}
		else 
		{
			nCurrentStringIndex = 0;
		}
	}
	return std::nullopt;
}

std::optional<std::vector<char>> Sockets::CTcpSocket::ReadCount(size_t nCountToRead) noexcept
{
	if(m_iSocketFD < 0)
	{
		fprintf(stderr, "The given socket doesn't exist or it's not opened\n");
		return std::nullopt;
	}

	if(nCountToRead == 0)
	{
		return std::nullopt;
	}

	std::vector<char> readData;
	readData.resize(nCountToRead);

	ssize_t nBytesRead = 0;
	size_t nBytesLeft = nCountToRead;
	while(nBytesLeft > 0)
	{
		const ssize_t nBytesRecived = 
			recv(m_iSocketFD, readData.data() + nBytesRead, nBytesLeft, 0);
		if(nBytesRecived < 0)
		{
			fprintf(stderr, "An error occured while reciving data\n");
			return std::nullopt;
		}
		if(nBytesRecived == 0)
		{
			fprintf(stderr, "Connection was closed by host");
			return std::nullopt;
		}
		nBytesRead += nBytesRecived;
		nBytesLeft = nBytesLeft - nBytesRecived;
	}

	return readData;
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
