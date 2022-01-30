#ifndef DOWNLOADER_SOCKETS_H
#define DOWNLOADER_SOCKETS_H

#include <optional>
#include <vector>
#include <string>

#include "uri.h"

namespace Sockets
{
	using ReadResult = std::optional<std::vector<char>>;
	/// Representation of simple blocking socket 
	/// that can send and recive data
	class CSocket 
	{
	public:
		explicit CSocket()
		{

		}

		virtual bool Connect(const CURI& cURIToConnect) noexcept = 0;

		virtual ReadResult ReadTill(const std::string &csStringToReadTill) noexcept = 0;
		virtual ReadResult ReadCount(size_t nCountToRead) noexcept = 0;
		virtual ReadResult ReadTillEnd() noexcept = 0;

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
} // Sockets 

#endif // DOWNLOADER_SOCKETS_H
