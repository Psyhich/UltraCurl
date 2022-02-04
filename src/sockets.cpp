#include <arpa/inet.h>
#include <iterator>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <cstdio>
#include <unistd.h>
#include <stdio.h>

#include <cstring>
#include <algorithm>
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
	
	// Consuming all data from m_buffer
	std::vector<char> messageVector{m_buffer.begin(), m_nCurrentValidDataEnd};

	ssize_t nRecivedBytes;
	do
	{
		nRecivedBytes = recv(m_iSocketFD, m_buffer.data(), sizeof(char) * m_buffer.size(), 0);
		if(nRecivedBytes == -1)
		{
			fprintf(stderr, "Error while reciving data\n");
			return std::nullopt;
		}
		m_nCurrentValidDataEnd = m_buffer.begin() + nRecivedBytes;

		std::copy(m_buffer.begin(), m_nCurrentValidDataEnd, 
			std::back_inserter(messageVector));

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
	size_t nCurrentStringIndex{0};
	ssize_t nBytesRead{0};

	std::vector<char> readData;
	do
	{
		// Firstly trying to find given string in buffer, for situation when it's not empty
		for(auto currentChar = m_buffer.begin(); 
			currentChar != m_nCurrentValidDataEnd + 1; currentChar++)
		{
			readData.push_back(*currentChar);

			if(*currentChar == csStringToReadTill[nCurrentStringIndex])
			{
				++nCurrentStringIndex;
				if(nCurrentStringIndex == csStringToReadTill.size())
				{
					size_t nBytesScanned = 
						std::distance(m_buffer.begin(), currentChar);
					*m_nReadBytes += nBytesScanned;
					*m_nBytesToRead += nBytesScanned;

					// Setting new valid data end
					m_nCurrentValidDataEnd = 
						m_buffer.begin() + std::distance(currentChar, m_nCurrentValidDataEnd);
					// After we found point where given string ends, 
					// shifting remainnig data
					std::rotate(m_buffer.begin(), currentChar, m_buffer.end());
					readData.shrink_to_fit();
					return readData;
				}
			}
			else 
			{
				nCurrentStringIndex = 0;
			}
		}
		
		// If we haven't found string in given buffer, reading next bytes from socket
		nBytesRead = 
			recv(m_iSocketFD, m_buffer.data(), sizeof(char) * m_buffer.size(), 0);
		if(nBytesRead == -1)
		{
			fprintf(stderr, "Error while reciving data\n");
			return std::nullopt;
		}

		m_nCurrentValidDataEnd = m_buffer.begin() + nBytesRead + 1;

		*m_nReadBytes += BUFFER_SIZE;
		*m_nBytesToRead += BUFFER_SIZE;
	} while(nBytesRead != 0);

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

	size_t nBytesLeft = nCountToRead;

	*m_nBytesToRead += nCountToRead;

	while(nBytesLeft > 0)
	{
		// Checking and writing data into vector from buffer
		size_t nLengthOfValidData = 
			(size_t)std::distance(m_buffer.begin(), m_nCurrentValidDataEnd);
		if(nLengthOfValidData < nBytesLeft)
		{
			std::copy(m_buffer.begin(), m_nCurrentValidDataEnd, 
				std::back_inserter(readData));
			nBytesLeft -= nLengthOfValidData;
			*m_nReadBytes += nLengthOfValidData;
		}
		else
		{
			std::copy(m_buffer.begin(), m_buffer.begin() + nBytesLeft + 1,
				std::back_inserter(readData));
			*m_nReadBytes += nBytesLeft;
			return readData;
		}
		
		const ssize_t nBytesRecived = 
			recv(m_iSocketFD, m_buffer.data(), m_buffer.size() * sizeof(char), 0);

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
		// Adding 1 because all ranges will exclude last value
		m_nCurrentValidDataEnd = m_buffer.begin() + nBytesRecived + 1;

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
