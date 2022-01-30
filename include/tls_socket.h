#ifndef TLS_SOCKET_H
#define TLS_SOCKET_H

#include <mutex>

#include "openssl/ssl.h"

#include "tcp_socket.h"

namespace Sockets 
{
	/// Thread-safe implementation of OpenSSL lib 
	/// with sockets
	class CTLSSocket : public CTcpSocket
	{
	public:
		CTLSSocket();
		~CTLSSocket();

		CTLSSocket(const CTLSSocket &cSocketToCopy) = delete;
		CTLSSocket operator=(const CTLSSocket &cSocketToCopy) = delete;

		CTLSSocket(CTLSSocket &&socketToMove);
		CTLSSocket& operator =(CTLSSocket &&socketToMove);

		bool Connect(const CURI& cURIToConnect) noexcept override;

		ReadResult ReadTill(const std::string &csStringToReadTill) noexcept override;
		ReadResult ReadCount(size_t nCountToRead) noexcept override;
		ReadResult ReadTillEnd() noexcept override;

		bool Write(const char* pcchBytes, size_t nCount) noexcept override;
	private:
		void DealocateResources() noexcept;

	private:
		static std::once_flag OPENSSL_INITIALIZATION_FLAG;

		bool m_bIsConnected{false};

		SSL_CTX *m_sslConfigs{nullptr};
		SSL *m_sslConnection{nullptr};
	};
} // Sockets 

#endif // TLS_SOCKET_H
