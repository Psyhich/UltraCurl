#ifndef TLS_SOCKET_H
#define TLS_SOCKET_H

#include <memory>
#include <mutex>

#include "openssl/ssl.h"
#include "openssl/bio.h"

#include "network_socket.h"

namespace Sockets 
{
	namespace OpenSSLBindings
	{
		// Implementations of deleters for OpenSSL on-heap types
		template<class T> struct DeleterOf;
		template<> struct DeleterOf<SSL_CTX>
		{ 
			void operator()(SSL_CTX *p) const { SSL_CTX_free(p); } 
		};
		template<> struct DeleterOf<SSL> {
			void operator()(SSL *p) const { SSL_free(p); } 
		};
		template<> struct DeleterOf<BIO>
		{
			void operator()(BIO *p) const { BIO_free_all(p); } 
		};
		template<> struct DeleterOf<X509>
		{
			void operator()(X509 *p) const { X509_free(p); } 
		};
		template<> struct DeleterOf<BIO_METHOD>
		{
			void operator()(BIO_METHOD *p) const { BIO_meth_free(p); } 
		};

		template<class T> 
		using COpenSSLPointer = std::unique_ptr<T, DeleterOf<T>>;
	}

	/// Thread-safe implementation of OpenSSL lib 
	/// with sockets
	class CTLSSocket : public CNetworkSocket
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
		static void InitOpenSSLLib() noexcept;

		bool VerifyCertificate(const std::string &csHostName) noexcept;

		inline SSL *GetSSL() noexcept
		{
			SSL *pSSL{nullptr};
			BIO_get_ssl(m_SSLBio.get(), &pSSL);
			return pSSL;
		}

	private:
		static constexpr const size_t BUFFER_SIZE{1024};
		static inline std::once_flag OPENSSL_INITIALIZATION_FLAG;

		bool m_bIsConnected{false};

		//CTCPSocketBIO m_TCPBio;
		OpenSSLBindings::COpenSSLPointer<BIO> m_SSLBio{nullptr};
		OpenSSLBindings::COpenSSLPointer<SSL_CTX> m_SSLConfiguration{nullptr};
	};
} // Sockets 

#endif // TLS_SOCKET_H
