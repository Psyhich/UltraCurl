#include <istream>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "http_downloader.h"
#include "cli_helper.h"
#include "sockets.h"
#include "uri.h"

constexpr const char* HELP_MESSAGE =
"Usage:\n"
"%s <url> [options]\n"
"-f, --file <filepath>	file to read links from(should be divided with newlines)\n"
"-h, --help, --usage	show help\n";

using HTTPTcpDownloader = Downloaders::CHTTPDownloader<Sockets::CTcpSocket>;

void PrintUsage(const std::string &name);

void WriteIntoFiles(std::istream *const pInputStream, std::ostream *const pOutputStream, 
	const CLI::CCLIHelper &cParamaters) noexcept;

bool GetStreams(std::istream *&pInputStream, std::ostream *&pOutputStream, 
	const CLI::CCLIHelper &cParameters) noexcept;

int main(int iArgc, char *argv[]) {
	const CLI::CCLIHelper cParamaters{iArgc, argv};

	std::istream *pInputStream = nullptr;
	std::ostream *pOutputStream = nullptr;
	
	if(!GetStreams(pInputStream, pOutputStream, cParamaters))
	{
		return -1;
	}

	WriteIntoFiles(pInputStream, pOutputStream, cParamaters);

	// Dealocating input stream if it was allocated
	if(pInputStream != &std::cin)
	{
		delete pInputStream;
	}
	if(pOutputStream != &std::cout)
	{
		delete pOutputStream;
	}
}

bool GetStreams(std::istream *&pInputStream, std::ostream *&pOutputStream, 
	const CLI::CCLIHelper &cParameters) noexcept
{
	// Checking if input is being piped
	if(isatty(fileno(stdin)))
	{
		// If it's not piped looking for -f or --file 
		// Argument and trying to load that file
		auto fileParameterValue = cParameters.GetParameterValue("f");
		// Trying alternative
		if(!fileParameterValue)
		{
			fileParameterValue = cParameters.GetParameterValue("file");
		}
		if(fileParameterValue)
		{
			pInputStream = new std::ifstream(*fileParameterValue);
			if(!pInputStream)
			{
				fprintf(stderr, "File: %s doesn't exist", fileParameterValue->c_str());
				delete pInputStream;
				return false;
			}
		}
		else
		{
			// if we haven't found value trying to get specified url
			if(const auto URIParamater = cParameters.GetParameterValue(""))
			{
				pInputStream = new std::stringstream(URIParamater->data());
			}
			else // If we don't get any URI returning
			{
				PrintUsage(cParameters.GetName());
				return false;
			}
		}
	}
	else
	{
		pInputStream = &std::cin;
	}

	// Checking if output is being piped
	if(!isatty(fileno(stdout)))
	{
		pOutputStream = &std::cout;
	}
	return true;
}

void WriteIntoFiles(std::istream *const pInputStream, std::ostream *pOutputStream, 
	const CLI::CCLIHelper &cParamaters) noexcept
{
	// Now reading the stream and after each \n downloading the file from URI
	std::string sLine; 
	while(getline(*pInputStream, sLine))
	{
		const CURI pageURI = CURI(sLine);
		std::string sFileName;
		// Checking for available stream
		// If we don't pipe data, saving into files
		// But checking if can overwrite constructed file names
		if(pOutputStream == nullptr)
		{
			// Constructing file name from request path and content type
			const auto path = pageURI.GetPath();
			if(path && path->has_filename())
			{
				sFileName = path->filename();
			}
			else
			{
				if(auto sPageAddress = pageURI.GetPureAddress())
				{
					sFileName = std::move(*sPageAddress);
				} 
				else
				{
					fprintf(stderr, "Invalid URI: %s", sLine.c_str());
					continue;
				}
			}
			// Checking if we won't overwrite anything
			const bool cbIsRewriteForced = 
				cParamaters.CheckIfParameterExist("force");

			std::ifstream fileToCheck{sFileName};
			if(!fileToCheck || cbIsRewriteForced)
			{
				fileToCheck.close();
				pOutputStream = new std::ofstream{sFileName};
			}
			else 
			{
				fileToCheck.close();
				fprintf(stderr, "Cannot overwrite: %s\n", sFileName.c_str());
				continue;
			}
		}
		
		// Downloading data
		HTTPTcpDownloader downloader;
		if(auto cResponse = downloader.Download(pageURI))
		{
			pOutputStream->write(cResponse->GetData().data(), cResponse->GetData().size());
			if(pOutputStream != &std::cout)
			{
				printf("Saved: %s\n", sFileName.c_str());
				delete pOutputStream;
				pOutputStream = nullptr;
			}
		}
		else
		{
			printf("Failed to download from: %s\n", sLine.c_str());
		}
	}
}

void PrintUsage(const std::string& name)
{
	fprintf(stderr, HELP_MESSAGE, name.c_str());
}
