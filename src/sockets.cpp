#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <cstdio>
#include <unistd.h>
#include <stdio.h>

#include <cstring>
#include <vector>

#include "sockets.h"


std::optional<uint16_t> Sockets::CTcpSocket::ExtractPortInByteOrder(const CURI &cURIToGetPort) noexcept
{
	if(const auto cPort = cURIToGetPort.GetPort())
	{
		return htons(*cPort);
	}
	return htons(80);
}

std::optional<sockaddr> Sockets::CTcpSocket::GetSocketAddress(const CURI& cURIToGetAddress) noexcept
{
	if(const auto cAddress = cURIToGetAddress.GetPureAddress())
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

void Sockets::CTcpSocket::MoveData(CTcpSocket &&socketToMove) noexcept
{
	m_nBytesToRead = socketToMove.m_nBytesToRead;
	m_nReadBytes = socketToMove.m_nReadBytes;
	m_iSocketFD = socketToMove.m_iSocketFD;

	socketToMove.m_nBytesToRead = std::nullopt;
	socketToMove.m_nReadBytes = std::nullopt;
	socketToMove.m_iSocketFD = -1;
}


Sockets::CTcpSocket::CTcpSocket()
{
}

Sockets::CTcpSocket::~CTcpSocket()
{
	if(m_iSocketFD != -1)
	{
		close(m_iSocketFD);
	}
}


Sockets::CTcpSocket::CTcpSocket(CTcpSocket &&socketToMove)
{
	MoveData(std::move(socketToMove));
}

Sockets::CTcpSocket& Sockets::CTcpSocket::CTcpSocket::operator =(CTcpSocket &&socketToMove)
{
	if(this != &socketToMove)
	{
		MoveData(std::move(socketToMove));
	}

	return *this;
}

bool Sockets::CTcpSocket::Connect(const CURI &cURIToConnect) noexcept
{
	m_nReadBytes = 0;
	m_nBytesToRead = 0;

	// Getting address info to know which IP protocol to use
	std::optional<sockaddr> cResolvedAddress = GetSocketAddress(cURIToConnect);
	if(!cResolvedAddress)
	{
		fprintf(stderr, "Failed to resolve given address\n");
		return false;
	}

	std::optional<uint16_t> uiPort = ExtractPortInByteOrder(cURIToConnect);
	// If we got wrongly written port returning nullopt
	if(!uiPort)
	{
		fprintf(stderr, "Given port is invalid\n");
		return false;
	}
	
	// Creating socket
	m_iSocketFD = socket(cResolvedAddress->sa_family, SOCK_STREAM, 0);
	if(m_iSocketFD == -1)
	{
		fprintf(stderr, "Failed to create socket\n");
		return false;
	}

	// Setting specified port for resolved address
	sockaddr_in &address = ((sockaddr_in&)*cResolvedAddress);
	address.sin_port = *uiPort;

	if(connect(m_iSocketFD, (&*cResolvedAddress), sizeof(sockaddr)) == -1)
	{
		fprintf(stderr, "Failed to connect to given address\n");
		close(m_iSocketFD);
		m_iSocketFD = -1;
		return false;
	}

	return true;
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

		*m_nReadBytes += nRecivedBytes;
		*m_nBytesToRead += nRecivedBytes;

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
		*m_nReadBytes += 1;
		*m_nBytesToRead += 1;
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

	*m_nBytesToRead += nCountToRead;
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
		*m_nReadBytes += nBytesRecived;
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
