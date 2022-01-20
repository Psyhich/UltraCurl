#ifndef DOWNLOAD_SOCKETS_H
#define DOWNLOAD_SOCKETS_H

#include <sys/socket.h>

#include <optional>
#include <vector>
#include <string>

#include "uri.h"

namespace Sockets
{
	/// Representation of simple blocking socket 
	/// that can send and recive data
	class CSocket 
	{
	public:
		explicit CSocket()
		{

		}

		virtual bool Connect(const CURI& cURIToConnect) noexcept = 0;

		virtual std::optional<std::vector<char>> ReadTill(const std::string &csStringToReadTill) noexcept = 0;
		virtual std::optional<std::vector<char>> ReadCount(size_t nCountToRead) noexcept = 0;
		virtual std::optional<std::vector<char>> ReadTillEnd() noexcept = 0;

		virtual bool Write(const char* pcchBytes, size_t nCount)  noexcept = 0;
		virtual ~CSocket() {}

		inline std::optional<size_t> GetBytesToRead() const noexcept 
		{
			return m_nBytesToRead;
		}

		inline std::optional<size_t> GetReadBytes() const noexcept 
		{
			return m_nReadBytes;
		}

	protected:
		// Progress bytes are optional because not all 
		// sockets can deduce how much data they would need to download
		std::optional<size_t> m_nBytesToRead{std::nullopt};
		std::optional<size_t> m_nReadBytes{std::nullopt};
	};

	class CTcpSocket : public CSocket
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
		std::optional<std::vector<char>> ReadTill(const std::string &csStringToReadTill) noexcept override;

		/// Reads some count of chars from socket
		std::optional<std::vector<char>> ReadCount(size_t nCountToRead) noexcept override;

		/// Reads data till the socket is 
		/// not closed or there is nothing to get
		std::optional<std::vector<char>> ReadTillEnd() noexcept override;

		/// Writes all data to socket
		bool Write(const char* pcchBytes, size_t nCount) noexcept override;
	private:
		/// Returns port in network byte order
		static std::optional<uint16_t> ExtractPortInByteOrder(const CURI &cURIToGetAddress) noexcept;
		static std::optional<sockaddr> GetSocketAddress(const CURI &cURIToGetPort) noexcept;
		void MoveData(CTcpSocket &&socketToMove) noexcept;
	private:
		static const constexpr char *HTTP_SERVICE = "80";
		static const constexpr size_t BUFFER_SIZE = 4096;

		int m_iSocketFD{-1};
		std::array<char, BUFFER_SIZE> m_buffer;
		std::array<char, BUFFER_SIZE>::iterator m_nCurrentValidDataEnd{m_buffer.begin()};
	};

} // Sockets 



#endif // DOWNLOAD_SOCKETS_H
