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
"Usage:\n" \
"%s <url> [options]\n"	\
"-f, --file <filepath>	file to read links from(should be divided with newlines)\n" \
"-h, --help, --usage	show help\n";

using HTTPTcpDownloader = Downloaders::CHTTPDownloader<Sockets::CTcpSocket>;

void PrintUsage(const std::string &name);

void WriteIntoFiles(std::istream *const pInputStream, std::ostream *const pOutputStream, 
	const CLI::CCLIHelper &cParamaters) noexcept;

bool GetStreams(std::istream **pInputStream, std::ostream **pOutputStream, 
	const CLI::CCLIHelper &cParameters) noexcept;

int main(int iArgc, char *argv[]) {
	const CLI::CCLIHelper cParamaters{iArgc, argv};

	std::istream *pInputStream = nullptr;
	std::ostream *pOutputStream = nullptr;
	
	if(!GetStreams(&pInputStream, &pOutputStream, cParamaters))
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

bool GetStreams(std::istream **pInputStream, std::ostream **pOutputStream, 
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
			*pInputStream = new std::ifstream(*fileParameterValue);
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
				*pInputStream = new std::stringstream(URIParamater->data());
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
		*pInputStream = &std::cin;
	}

	// Checking if output is being piped
	if(isatty(fileno(stdout)) && pOutputStream == nullptr)
	{
		*pOutputStream = &std::cout;
	}
	return true;
}

void WriteIntoFiles(std::istream *const pInputStream, std::ostream *const pOutputStream, 
	const CLI::CCLIHelper &cParamaters) noexcept
{
	// Now reading the stream and after each \n downloading the file from URI
	std::string sLine; 
	while(getline(*pInputStream, sLine))
	{
		// Downloading data
		HTTPTcpDownloader downloader;
		if(auto cResponse = downloader.Download(sLine))
		{
			// If we piping our output, writing into pipe
			if(pOutputStream != nullptr)
			{
				*pOutputStream << cResponse->GetData().data();
			} 
			else 
			{
				// Constructing file name from request path and content type
				std::string sFileName;
				const CURI pageURI = CURI(sLine);
				const auto path = pageURI.GetPath();
				if(path && path->has_filename())
				{
					sFileName = path->filename();
					if(path->has_extension())
					{
						sFileName += '.';
						sFileName += path->extension();
					}
				}
				else
				{
					// Can unwrap optional value because we know 
					// That adress exists and we got response from it
					sFileName = *pageURI.GetPureAddress();
				}
				// Checking if we won't overwrite anything
				const bool cbIsRewriteForced = 
					cParamaters.CheckIfParameterExist("force");

				std::ifstream fileToCheck{sFileName};
				if(!fileToCheck || cbIsRewriteForced)
				{
					fileToCheck.close();
					std::ofstream fileToWrite{sFileName};
					const std::vector<char> &cResponseData = cResponse->GetData();
					fileToWrite.write(cResponseData.data(), cResponseData.size());
					fileToWrite.close();
					printf("Saved: %s\n", sFileName.c_str());
				}
				else 
				{
					fileToCheck.close();
					fprintf(stderr, "Cannot overwrite: %s\n", sFileName.c_str());
				}
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
