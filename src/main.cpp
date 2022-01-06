#include <unistd.h>

#include <iostream>
#include <istream>
#include <fstream>
#include <string>
#include <sstream>

#include "cli_helper.h"
#include "base_functionality.h"

constexpr const char* HELP_MESSAGE =
"Usage:\n"
"%s <url> [options]\n"
"-f, --file <filepath>	file to read links from(should be divided with newlines)\n"
"-h, --help, --usage	show help\n"
"-t --threads			count of threads to use";

void PrintUsage(const std::string &name);

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

	// Getting count of threads to use
	// If we are not piping anything out, writing into files(so using threads)
	if(pOutputStream == nullptr)
	{
		// Getting count of threads
		unsigned uCountOfThreads = 0;
		// Trying to convert parameter -t OR --threads otherwise it's 0
		try
		{
			uCountOfThreads = std::stoul(
				cParamaters.GetParameterValue("t")
				.value_or(cParamaters.GetParameterValue("threads")
					.value_or("0")));
		}
		catch(...)
		{
			fprintf(stderr, "Wrong count of threads passed");
		}
		// Checking if we can overwite
		const bool cbShouldOverwrite = 
			cParamaters.CheckIfParameterExist("f") || cParamaters.CheckIfParameterExist("force");

		APIFunctionality::WriteIntoFiles(pInputStream, cbShouldOverwrite, uCountOfThreads);
	}
	else
	{
		APIFunctionality::WriteIntoStream(pInputStream, pOutputStream);
	}

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


void PrintUsage(const std::string& name)
{
	fprintf(stderr, HELP_MESSAGE, name.c_str());
}
