#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

#include <netdb.h>
#include <netinet/in.h>

#include "sockets.h"
#include "uri.h"

namespace Sockets {
	/// Pure virtual class that contains helper network functions
	class CNetworkSocket : public CSocket
	{
	protected:
		struct SAddrInfoDeleter
		{
			void operator()(addrinfo *pAddrInfo)
			{
				freeaddrinfo(pAddrInfo);
			}
		};
		using CAddrInfo = std::unique_ptr<addrinfo[], SAddrInfoDeleter>;

		/// Function to resolve right port for protocol or given port in URI
		static std::optional<int> ExtractServicePort(const CURI &cURIToExtract) noexcept;
		static std::optional<CAddrInfo> GetHostAddresses(const CURI &cURIToExtract) noexcept;
	};
}


#endif // NETWORK_SOCKET_H
