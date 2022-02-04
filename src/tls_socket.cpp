#include <algorithm>

#include "openssl/ssl.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/x509v3.h"

#include "tls_socket.h"

Sockets::CTLSSocket::CTLSSocket()
{
	m_nReadBytes = 0;
	m_nBytesToRead = 0;

	// Calling OpenSSL initialization once 
	std::call_once(OPENSSL_INITIALIZATION_FLAG, InitOpenSSLLib);
}

Sockets::CTLSSocket::~CTLSSocket()
{

}

bool Sockets::CTLSSocket::Connect(const CURI& cURIToConnect) noexcept
{
	// If at least one breaks we have an error during creation
	do
	{
		// Configuring OpenSSL connection structs
		#if OPENSSL_VERSION_NUMBER < 0x10100000L
			m_SSLConfiguration.reset(SSL_CTX_new(SSLv23_client_method()));
		#else
			m_SSLConfiguration.reset(SSL_CTX_new(TLS_client_method()));
		#endif

		if(m_SSLConfiguration == nullptr)
		{
			fprintf(stderr, "Failed to create configuration for SSL connection\n");
			break;
		}

		// Loading local trust store
		SSL_CTX_set_default_verify_file(m_SSLConfiguration.get());
		if (SSL_CTX_set_default_verify_paths(m_SSLConfiguration.get()) <= 0) {
			fprintf(stderr, "Cannot find local trust store\n");
			break;
		}

		// Setting minimal TLS version
		SSL_CTX_set_min_proto_version(m_SSLConfiguration.get(), TLS1_2_VERSION);

		// Creating SSL bio
		m_SSLBio.reset(BIO_new_ssl(m_SSLConfiguration.get(), 1));

		const std::string sExtractedPort = 
			std::to_string(ExtractServicePort(cURIToConnect).value_or(433));
		const std::string cHostName = *cURIToConnect.GetPureAddress();

		OpenSSLBindings::COpenSSLPointer<BIO> tcpBIO{BIO_new_connect(cHostName.c_str())};
		if(tcpBIO == nullptr)
		{
			fprintf(stderr, "Failed to create tcp connection!\n");
			return false;
		}

		if(BIO_set_conn_port(tcpBIO.get(), sExtractedPort.c_str()) <= 0)
		{
			fprintf(stderr, "Failed to set port name!\n");
			return false;
		}
		
		if(BIO_do_connect(tcpBIO.get()) <= 0)
		{
		  fprintf(stderr, 
		  	"Failed to establish connection with host %s\n", cHostName.c_str());
		  return false;
		}

		// Connecting SSL BIO pipe to TCP BIO
		// We need to realease resources held by tcpBIO, because the will be freed with m_SSLBio
		BIO_push(m_SSLBio.get(), tcpBIO.release());

		// Setting host destination to be included in handshake
		SSL_set_tlsext_host_name(GetSSL(), cHostName.c_str());
		#if OPENSSL_VERSION_NUMBER >= 0x10100000L
			SSL_set1_host(GetSSL(), cHostName.c_str());
		#endif

		if(BIO_do_handshake(m_SSLBio.get()) <= 0)
		{
			fprintf(stderr, "Failed to establish secure connection with given host\n");
			ERR_print_errors_fp(stderr);
			break;
		}

		if(VerifyCertificate(cHostName))
		{
			break;
		}

		m_bIsConnected = true;
		return true;
	} while(false);

	return false;
}

Sockets::ReadResult Sockets::CTLSSocket::ReadTill(const std::string &csStringToReadTill) noexcept
{
	if(!m_bIsConnected)
	{
		fprintf(stderr, "Socket is not connected!\n");
		return std::nullopt;
	}

	if(csStringToReadTill.size() < 1)
	{
		return std::nullopt;
	}

	size_t nCurrentStringIndex{0};

	char cReadChar{0};

	std::vector<char> readData;
	while(true)
	{
		if(BIO_read(m_SSLBio.get(), &cReadChar, sizeof(char)) == 0)
		{
			if(BIO_should_retry(m_SSLBio.get()) == 1)
			{
				continue;
			}
			return std::nullopt;
		}

		readData.push_back(cReadChar);

		// If we succeded in reading, checking the string
		if(csStringToReadTill[nCurrentStringIndex] == cReadChar)
		{
			++nCurrentStringIndex;
			if(nCurrentStringIndex == csStringToReadTill.size())
			{
				readData.shrink_to_fit();
				return readData;
			}
		}
		else
		{
			nCurrentStringIndex = 0;
		}
	}
}
Sockets::ReadResult Sockets::CTLSSocket::ReadCount(size_t nDataToRead) noexcept
{
	if(!m_bIsConnected)
	{
		fprintf(stderr, "Socket is not connected!\n");
		return std::nullopt;
	}

	ReadResult responseResult{std::vector<char>{}};
	responseResult->resize(nDataToRead);

	size_t nBytesRead{0};
	size_t nOveralBytesRead{0};

	char *pcStartOfWritableData = responseResult->data();

	while(nOveralBytesRead < nDataToRead)
	{
		if(BIO_read_ex(m_SSLBio.get(), pcStartOfWritableData,  
			nDataToRead - nOveralBytesRead, &nBytesRead) <= 0)
		{
			if(BIO_should_retry(m_SSLBio.get()) == 1)
			{
				continue;
			}
			return std::nullopt;
		}

		nOveralBytesRead += nBytesRead;
		pcStartOfWritableData += nBytesRead;
	}

	return responseResult;
}
Sockets::ReadResult Sockets::CTLSSocket::ReadTillEnd() noexcept
{
	if(!m_bIsConnected)
	{
		fprintf(stderr, "Socket is not connected!\n");
		return std::nullopt;
	}

	ReadResult responseResult{std::vector<char>{}};

	size_t nBytesRead{0};
	size_t nBytesGathered{0};

	responseResult->resize(BUFFER_SIZE);
	char *currentDataStart{responseResult->data()};

	// We should read till we won't get SSL_ERROR_ZERO_RETURN
	while(true)
	{
		if(BIO_read_ex(m_SSLBio.get(), currentDataStart, BUFFER_SIZE, &nBytesRead) == 0)
		{
			// Skipping all additions to retry read operation
			if(BIO_should_retry(m_SSLBio.get()) == 1)
			{
				continue;
			}
			// OpenSSL uses such return value so I cannot follow main style doc
			const unsigned long ulGotError = ERR_get_error();
			switch(ulGotError)
			{
				case SSL_ERROR_ZERO_RETURN:
				{
					// Breaking from loop on end of transmission
					responseResult->resize(nBytesGathered);
					responseResult->shrink_to_fit();
					break;
				}
				default:
				{
					responseResult = std::nullopt;
				}
			}
			break;
		}
		nBytesGathered += nBytesRead;
		responseResult->resize(nBytesGathered + BUFFER_SIZE);
		// We should reset pointer for end of data only after resize, because data can invalidate
		currentDataStart = responseResult->data() + nBytesGathered + 1;
	}


	return responseResult;
}

bool Sockets::CTLSSocket::Write(const char* cpcDataToSend, size_t nBytesCount) noexcept
{
	if(!m_bIsConnected)
	{
		fprintf(stderr, "Socket is not connected\n");
		return false;
	}

	if(nBytesCount == 0)
	{
		return false;
	}

	if(BIO_write(m_SSLBio.get(), cpcDataToSend, nBytesCount) <= 0)
	{
		if(BIO_should_retry(m_SSLBio.get()))
		{
			return Write(cpcDataToSend, nBytesCount);
		}
		return false;
	}

	return true;
}

void Sockets::CTLSSocket::InitOpenSSLLib() noexcept
{
	#if OPENSSL_VERSION_NUMBER < 0x10100000L
		SSL_library_init();
	#else
		OPENSSL_init_ssl(0, nullptr);
	#endif

	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	SSL_load_error_strings();
}


bool Sockets::CTLSSocket::VerifyCertificate(const std::string &csHostName) noexcept
{
	SSL *pSSLConnection = GetSSL();
    OpenSSLBindings::COpenSSLPointer<X509> pCert
		{SSL_get_peer_certificate(pSSLConnection)};
    if (pCert == nullptr) {
        fprintf(stderr, "No certificate was presented by the server\n");
        return false;
    }

	int iErr = SSL_get_verify_result(pSSLConnection);
    if (iErr != X509_V_OK) {
        const char *message = X509_verify_cert_error_string(iErr);
        fprintf(stderr, "Certificate verification error: %s (%d)\n", message, iErr);
        return false;
    }

    if (X509_check_host(pCert.get(), csHostName.data(), csHostName.size(), 0, nullptr) != 1) {
        fprintf(stderr, "Certificate verification error: Hostname mismatch\n");
        return false;
    }
	return true;
}
