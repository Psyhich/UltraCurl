#ifndef BASE_FUNCTIONALITY_H
#define BASE_FUNCTIONALITY_H

#include <istream>
#include <ostream>
#include <optional>

#include "sockets.h"
#include "uri.h"
#include "downloader_pool.h"

namespace APIFunctionality
{
	using ConcurrentDownloaders = Downloaders::Concurrency::CConcurrentDownloader<Sockets::CTcpSocket>;
	using FDownloadCallback = std::function<void(const CURI &cDownloadedURI, std::optional<HTTP::CHTTPResponse>)>;

	/// Function to concurrently download all specified links and store them in files
	/// After all links have been added it returns DownloaderPool object for further operations
	ConcurrentDownloaders* WriteIntoFiles(std::istream *const cpInputStream, const bool cbOverwrite, 
		unsigned uCountOfThreads, FDownloadCallback fDownloadCallback) noexcept;

	/// Function to sequentialy download given files and output them into given stream
	void WriteIntoStream(std::istream *const cpInputStream, std::ostream *const cpOutputStream) noexcept;

	namespace Utility
	{
		bool CheckIfFileExists(const std::string &csFileName);
		std::optional<std::string> GetFileName(const CURI &cURIToExtractFrom);
	}
}

#endif /* !BASE_FUNCTIONALITY_H */
