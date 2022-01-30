#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>

#include "openssl/ssl.h"
#include "openssl/evp.h"

#include "tcp_socket.h"
#include "tls_socket.h"

Sockets::CTLSSocket::CTLSSocket()
{
	m_nReadBytes = 0;
	m_nBytesToRead = 0;
}

Sockets::CTLSSocket::~CTLSSocket()
{
	DealocateResources();
}

bool Sockets::CTLSSocket::Connect(const CURI& cURIToConnect) noexcept
{
	// Calling OpenSSL initialization once 
	#if OPENSSL_VERSION_NUMBER < 0x10100000L
		std::call_once(OPENSSL_INITIALIZATION_FLAG, SSL_library_init);
	#else
		std::call_once(OPENSSL_INITIALIZATION_FLAG, OPENSSL_init_ssl, 0, nullptr);
	#endif

	// If at least one breaks we have an error during creation
	do
	{
		// Configuring OpenSSL connection structs
		m_sslConfigs = SSL_CTX_new(TLS_method());
		if(m_sslConfigs == nullptr)
		{
			fprintf(stderr, "Failed to create configuration for SSL connection\n");
			break;
		}

		m_sslConnection = SSL_new(m_sslConfigs);
		if(m_sslConnection == nullptr)
		{
			fprintf(stderr, "Failed to create connection struct\n");
			break;
		}

		if(!CTcpSocket::EstablishTCPConnection(cURIToConnect))
		{
			break;
		}

		if(SSL_set_fd(m_sslConnection, GetFD()) == 0)
		{
			fprintf(stderr, "Failed to connect file descriptor to SSL connection\n");
			break;
		}

		if(SSL_connect(m_sslConnection) <= 0)
		{
			fprintf(stderr, "Error, during connection to host\n");
			break;
		}

		m_bIsConnected = true;
		return true;
	} while(false);

	// Disconnecting and dealocating resources after failed connection
	DealocateResources();
	Disconnect();

	return false;
}

void Sockets::CTLSSocket::DealocateResources() noexcept
{
	if(m_sslConnection != nullptr)
	{
		SSL_free(m_sslConnection);
	}
	if(m_sslConfigs != nullptr)
	{
		SSL_CTX_free(m_sslConfigs);
	}
}

Sockets::ReadResult Sockets::CTLSSocket::ReadTill(const std::string &csStringToReadTill) noexcept
{
	if(!m_bIsConnected)
	{
		fprintf(stderr, "The given socket is not opened\n");
		return std::nullopt;
	}

	if(csStringToReadTill.size() < 1)
	{
		return std::nullopt;
	}

	size_t nCurrentStringIndex{0};
	ssize_t nBytesRead{0};

	std::array<char, BUFFER_SIZE> &buffer = GetBuffer();
	std::array<char, BUFFER_SIZE>::iterator &validDataEnd = GetValidEnd();

	std::vector<char> readData;
	while(true)
	{
		// Firstly trying to find given string in buffer, for situation when it's not empty
		for(auto currentChar = buffer.begin(); 
			currentChar != validDataEnd + 1; currentChar++)
		{
			readData.push_back(*currentChar);

			if(*currentChar == csStringToReadTill[nCurrentStringIndex])
			{
				++nCurrentStringIndex;
				if(nCurrentStringIndex == csStringToReadTill.size())
				{
					size_t nBytesScanned = 
						std::distance(buffer.begin(), currentChar);
					*m_nReadBytes += nBytesScanned;
					*m_nBytesToRead += nBytesScanned;

					// Setting new valid data end
					validDataEnd = 
						buffer.begin() + std::distance(currentChar, validDataEnd);
					// After we found point where given string ends, 
					// shifting remainnig data
					std::rotate(buffer.begin(), currentChar, buffer.end());
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
			SSL_read(m_sslConnection, buffer.data(), sizeof(char) * buffer.size());
		if(nBytesRead <= 0)
		{
			fprintf(stderr, "Error while reciving data\n");
			return std::nullopt;
		}

		validDataEnd = buffer.begin() + nBytesRead + 1;

		*m_nReadBytes += BUFFER_SIZE;
		*m_nBytesToRead += BUFFER_SIZE;
	}

	return std::nullopt;
}
Sockets::ReadResult Sockets::CTLSSocket::ReadCount(size_t nCountToRead) noexcept
{
	if(!m_bIsConnected)
	{
		fprintf(stderr, "The given socket is not opened\n");
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

	std::array<char, BUFFER_SIZE> &buffer = GetBuffer();
	std::array<char, BUFFER_SIZE>::iterator &validDataEnd = GetValidEnd();

	while(nBytesLeft > 0)
	{
		// Checking and writing data into vector from buffer
		size_t nLengthOfValidData = 
			(size_t)std::distance(buffer.begin(), validDataEnd);
		if(nLengthOfValidData < nBytesLeft)
		{
			std::copy(buffer.begin(), validDataEnd, 
				std::back_inserter(readData));
			nBytesLeft -= nLengthOfValidData;
			*m_nReadBytes += nLengthOfValidData;
		}
		else
		{
			std::copy(buffer.begin(), buffer.begin() + nBytesLeft + 1,
				std::back_inserter(readData));
			*m_nReadBytes += nBytesLeft;
			return readData;
		}
		
		const ssize_t nBytesRecived = 
			SSL_read(m_sslConnection, buffer.data(), sizeof(char) * buffer.size());

		if(nBytesRecived <= 0)
		{
			fprintf(stderr, "An error occured while reciving data\n");
			return std::nullopt;
		}
		// Adding 1 because all ranges will exclude last value
		validDataEnd = buffer.begin() + nBytesRecived + 1;

	}

	return readData;
}
Sockets::ReadResult Sockets::CTLSSocket::ReadTillEnd() noexcept
{
	if(!m_bIsConnected)
	{
		fprintf(stderr, "The given socket is not opened\n");
		return std::nullopt;
	}

	std::array<char, BUFFER_SIZE> &buffer = GetBuffer();
	std::array<char, BUFFER_SIZE>::iterator &validDataEnd = GetValidEnd();
	
	// Consuming all data from m_buffer
	std::vector<char> messageVector{buffer.begin(), GetValidEnd()};

	ssize_t nRecivedBytes;
	while(true)
	{
		nRecivedBytes = SSL_read(m_sslConnection, buffer.data(), sizeof(char) * buffer.size());
		if(nRecivedBytes <= 0)
		{
			// If we got zero data - the socket sent close
			if(SSL_get_error(m_sslConnection, nRecivedBytes) != SSL_ERROR_ZERO_RETURN)
			{
				fprintf(stderr, "Error while reciving data\n");
				return std::nullopt;
			}
			else
			{
				break;
			}
		}
		validDataEnd = buffer.begin() + nRecivedBytes;

		std::copy(buffer.begin(), validDataEnd, 
			std::back_inserter(messageVector));

		*m_nReadBytes += nRecivedBytes;
		*m_nBytesToRead += nRecivedBytes;

	}

	messageVector.shrink_to_fit();
	return messageVector;
}

bool Sockets::CTLSSocket::Write(const char* pcchBytes, size_t nCount) noexcept
{
	if(!m_bIsConnected)
	{
		fprintf(stderr, "The given socket is not opened\n");
		return false;
	}

	size_t nBytesToSend = nCount;
	while(nBytesToSend != 0)
	{
		ssize_t nSentBytes = 
			SSL_write(m_sslConnection, pcchBytes + (nCount - nBytesToSend), nBytesToSend);
		if(nSentBytes <= 0)
		{
			fprintf(stderr, "Failed to send data\n");
			return false;
		} else 
		{
			nBytesToSend -= (size_t)nSentBytes;
		}
	}
	return true;

}
