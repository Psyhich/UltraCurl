#include <fstream>
#include <sstream>
#include <string>

#include "base_functionality.h"
#include "downloader_pool.h"
#include "http_downloader.h"
#include "sockets.h"

using CConcurrentDownloader = Downloaders::Concurrency::CConcurrentDownloader;

APIFunctionality::DownloadersPool APIFunctionality::WriteIntoFiles( std::istream *const cpInputStream, 
	const bool cbOverwrite, unsigned uCountOfThreads, 
	FDownloadCallback fDownloadCallback) noexcept
{
	DownloadersPool downloaderPool{ CConcurrentDownloader::AllocatePool(
		Downloaders::CHTTPDownloader::ValidSocketFactory, uCountOfThreads)};

	// Now reading the stream and after each \n downloading the file from URI
	std::string sLine; 
	while(getline(*cpInputStream, sLine))
	{
		const CURI pageURI = CURI(sLine);
		std::string sFileName;
		// Checking for any possible name, if URI doesn't contain host, skipping
		if(auto sPossibleFileName = APIFunctionality::Utility::GetFileName(pageURI))
		{
			sFileName = std::move(*sPossibleFileName);
		}
		else
		{
			continue;
		}

		std::ofstream *pFileToWriteInto;
		// Checking if we can overwrite file or create new
		if(cbOverwrite || !APIFunctionality::Utility::CheckIfFileExists(sFileName))
		{
			pFileToWriteInto = new(std::nothrow) std::ofstream{sFileName};
		}
		else 
		{
			fprintf(stderr, "Cannot overwrite: %s\n", sFileName.c_str());
			continue;
		}
		
		// Downloading data
		downloaderPool->AddNewTask(pageURI, 
		// Callback for completed download
		[pFileToWriteInto, sFileName, sLine, fDownloadCallback]
		(std::optional<HTTP::CHTTPResponse> &&cResponse)
		{
			// We don't check for success codes to make user know about any other server states
			if(cResponse)
			{
				if(const auto cEncoding = cResponse->GetHeaders().find("content-encoding");
					cEncoding != cResponse->GetHeaders().end())
				{
					fprintf(stderr, "Encoding of response: %s", cEncoding->second.c_str());
					if(cEncoding->second.find("zstd") != std::string::npos)
					{
						cResponse->DecompressBody();
					}
				}
				pFileToWriteInto->write(cResponse->GetData().data(), cResponse->GetData().size());
			}

			fDownloadCallback(CURI(sLine), cResponse);
			delete pFileToWriteInto;
			return false;
		});

		// Setting nullptr, so next iterations couldn't use stream that is already used
		pFileToWriteInto = nullptr;
	}
	return downloaderPool;
}

void APIFunctionality::WriteIntoStream(std::istream *const cpInputStream, std::ostream *const cpOutputStream) noexcept
{
	std::string sLine; 
	while(getline(*cpInputStream, sLine))
	{
		const CURI cPageURI{sLine};
		// Checking if URI contains at least host
		if(!cPageURI.GetPureAddress())
		{
			fprintf(stderr, "Invalid URI: %s", cPageURI.GetFullURI().c_str());
			continue;
		}

		Downloaders::CHTTPDownloader downloader
			{Downloaders::CHTTPDownloader::ValidSocketFactory(cPageURI)};
		if(const auto cResponse = downloader.Download(cPageURI))
		{
			cpOutputStream->write(cResponse->GetData().data(), cResponse->GetData().size());
		}else
		{
			printf("Failed to download from: %s\n", sLine.c_str());
			continue;
		}

	}
}
bool APIFunctionality::Utility::CheckIfFileExists(const std::string &csFileName)
{
	// If we succeded in opening file for reading, the file exists
	std::ifstream fileToCheck{csFileName};
	return fileToCheck.good();
}

std::optional<std::string> APIFunctionality::Utility::GetFileName(const CURI &cURIToExtractFrom)
{
	const auto path = cURIToExtractFrom.GetPath();
	if(path && path->has_filename())
	{
		return path->filename();
	}
	else
	{
		if(auto sPageAddress = cURIToExtractFrom.GetPureAddress())
		{
			return *sPageAddress;
		}
		else
		{
			fprintf(stderr, "Invalid URI: %s", cURIToExtractFrom.GetFullURI().c_str());
			return std::nullopt;
		}
	}
}
