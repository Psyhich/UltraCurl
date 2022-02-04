#ifndef BASE_FUNCTIONALITY_H
#define BASE_FUNCTIONALITY_H

#include <istream>
#include <ostream>
#include <memory>

#include "downloader_pool.h"
#include "tcp_socket.h"
#include "uri.h"

namespace APIFunctionality
{
	using DownloadersPool = std::unique_ptr<Downloaders::Concurrency::CConcurrentDownloader>;
	using FDownloadCallback = std::function<void(const CURI &cDownloadedURI, std::optional<HTTP::CHTTPResponse>)>;

	/// Function to concurrently download all specified links and store them in files
	/// After all links have been added it returns DownloaderPool object for further operations
	DownloadersPool WriteIntoFiles(std::istream *const cpInputStream, const bool cbOverwrite, 
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
