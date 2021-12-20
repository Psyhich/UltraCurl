#ifndef DOWNLOAD_SOCKETS_H
#define DOWNLOAD_SOCKETS_H

#include <sys/socket.h>

#include <optional>
#include <vector>
#include <string>

#include "uri.h"

namespace Sockets
{
	/* 
	* Representation of simple blocking socket that can send and recive data
	*/
	class CSocket 
	{
	public:
		CSocket(const std::string& addressToUse);
		virtual std::optional<std::vector<char>> ReadTillEnd() noexcept = 0;
		virtual bool Write(const std::vector<char>& cBytes)  noexcept = 0;
		virtual ~CSocket() {}
	protected:
		inline const URI& GetAddress() { return m_address; }
	private:
		URI m_address;
	};

	class CTcpSocket : public CSocket
	{
	public:
		explicit CTcpSocket(const std::string& addressToUse);
		~CTcpSocket() override;
		
		std::optional<std::vector<char>> ReadTillEnd() noexcept override;
		bool Write(const std::vector<char> &cBytes) noexcept override;

		CTcpSocket(const CTcpSocket &cSockeToCopy) = delete;
		CTcpSocket operator=(const CTcpSocket &cSockeToCopy) = delete;

		CTcpSocket(CTcpSocket &&socketToMove);
		CTcpSocket operator =(CTcpSocket &&socketToMove);
	private:
		// Return port in network byte order
		static std::optional<uint16_t> ExtractPortInByteOrder(const URI &cURIString) noexcept;
		static std::optional<sockaddr> GetSocketAddress(const URI &cURIString) noexcept;
		static const constexpr char *HTTP_SERVICE = "80";
		static const constexpr size_t BUFFER_SIZE = 4096;
	private:
		int m_iSocketFD{0};

	};



} // Sockets 



#endif // DOWNLOAD_SOCKETS_H
