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
		explicit CSocket(const std::string& csAddressToUse);

		virtual std::optional<std::vector<char>> ReadTill(const std::string &csStringToReadTill) noexcept = 0;
		virtual std::optional<std::vector<char>> ReadCount(size_t nCountToRead) noexcept = 0;
		virtual std::optional<std::vector<char>> ReadTillEnd() noexcept = 0;

		virtual bool Write(const char* pcchBytes, size_t nCount)  noexcept = 0;
		virtual ~CSocket() {}
	protected:
		inline const CURI& GetAddress() const noexcept { return m_address; }
	private:
		CURI m_address;
	};

	class CTcpSocket : public CSocket
	{
	public:
		explicit CTcpSocket(const std::string& csAddressToUse);
		~CTcpSocket() override;

		CTcpSocket(const CTcpSocket &cSocketToCopy) = delete;
		CTcpSocket operator=(const CTcpSocket &cSocketToCopy) = delete;

		CTcpSocket(CTcpSocket &&socketToMove);
		CTcpSocket& operator =(CTcpSocket &&socketToMove);
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
		std::optional<uint16_t> ExtractPortInByteOrder() const noexcept;
		std::optional<sockaddr> GetSocketAddress() const noexcept;

		static const constexpr char *HTTP_SERVICE = "80";
		static const constexpr size_t BUFFER_SIZE = 4096;
	private:
		int m_iSocketFD{-1};
	};



} // Sockets 



#endif // DOWNLOAD_SOCKETS_H
