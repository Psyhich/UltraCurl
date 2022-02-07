#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <netdb.h>

#include "network_socket.h"

namespace Sockets
{
	class CTcpSocket : public CNetworkSocket
	{
	public:
		explicit CTcpSocket();
		~CTcpSocket() override;

		CTcpSocket(const CTcpSocket &cSocketToCopy) = delete;
		CTcpSocket operator=(const CTcpSocket &cSocketToCopy) = delete;

		CTcpSocket(CTcpSocket &&socketToMove);
		CTcpSocket& operator =(CTcpSocket &&socketToMove);

		bool Connect(const CURI &cURIToConnect) noexcept override;

		// IO operations

		/// Reads till some specified string including that string
		/// If string is not found, returning nullopt
		ReadResult ReadTill(const std::string &csStringToReadTill) noexcept override;

		/// Reads some count of chars from socket
		ReadResult ReadCount(size_t nCountToRead) noexcept override;

		/// Reads data till the socket is 
		/// not closed or there is nothing to get
		ReadResult ReadTillEnd() noexcept override;

		/// Writes all data to socket
		bool Write(const char* pcchBytes, size_t nCount) noexcept override;

		static const constexpr size_t BUFFER_SIZE = 4096;
	private:
		/// Returns port in network byte order
		static std::optional<sockaddr_in> GetSocketAddress(const CURI &cURIToGetPort) noexcept;
		void MoveData(CTcpSocket &&socketToMove) noexcept;

	private:
		static const constexpr char *DEFAULT_PORT = "http";

		int m_iSocketFD{-1};
		std::array<char, BUFFER_SIZE> m_buffer;
		std::array<char, BUFFER_SIZE>::iterator m_nCurrentValidDataEnd{m_buffer.begin()};
	};
}

#endif
