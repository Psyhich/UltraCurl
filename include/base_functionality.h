#ifndef BASE_FUNCTIONALITY_H
#define BASE_FUNCTIONALITY_H

#include <unistd.h>

#include <iostream>
#include <istream>
#include <fstream>
#include <string>
#include <sstream>

#include "downloader_pool.h"
#include "http_downloader.h"
#include "sockets.h"
#include "uri.h"

namespace APIFunctionality
{
	/// Function to concurrently download all specified links and store them in files
	void WriteIntoFiles(std::istream *const cpInputStream, const bool cbOverwrite, unsigned uCountOfThreads) noexcept;

	/// Function to sequentialy download given files and output them into given stream
	void WriteIntoStream(std::istream *const cpInputStream, std::ostream *const cpOutputStream) noexcept;

	namespace Utility
	{
		bool CheckIfFileExists(const std::string &csFileName);
		std::optional<std::string> GetFileName(const CURI &cURIToExtractFrom);
	}
}

#endif /* !BASE_FUNCTIONALITY_H */
