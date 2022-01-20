#include "test_socket.h"

bool BaseParams::CheckRequest(const std::string &sRequest)
{
	// Request should have Host, Accept and Accept-Encoding headers
	// It should end with \r\n\r\n, each header ends with \r\n
	// Host header should have right address
	// Status line should contain valid path, GET method 
	// and HTTP/1.1 version should end with \r\n

	// Checking status line
	size_t nEndPos = sRequest.find(' ');
	if(nEndPos == std::string::npos)
	{
		return false;
	}
	std::string sGotString = sRequest.substr(0, nEndPos);
	if(sGotString != "GET")
	{
		return false;
	}

	size_t nStartPos = nEndPos;
	nEndPos = sRequest.find(' ', nEndPos + 1);
	if(nEndPos == std::string::npos)
	{
		return false;
	}
	sGotString = sRequest.substr(nStartPos + 1, nEndPos - nStartPos - 1);
	if(sGotString != sPath)
	{
		return false;
	}
	nStartPos = nEndPos;
	
	nEndPos = sRequest.find("\r\n", nEndPos);
	if(nEndPos == std::string::npos)
	{
		return false;
	}
	sGotString = sRequest.substr(nStartPos + 1, nEndPos - nStartPos - 1);
	if(sGotString != "HTTP/1.1")
	{
		return false;
	}
	
	// Checking headers
	bool bIsFoundHost{false};
	bool bIsFoundAccept{false};
	bool bIsFoundAcceptEncoding{false};

	nStartPos = nEndPos + 2;
	for(int i = 0; i < 3; i++){
		nEndPos = sRequest.find("\r\n", nStartPos);
		if(nEndPos == std::string::npos)
		{
			return false;
		}
		// Pair should follow convention of usage like this: "key: value"
		std::string sKeyValuPair = sRequest.substr(nStartPos, nEndPos - nStartPos);
		size_t nSeparatorPos = sKeyValuPair.find(':');
		if(nSeparatorPos == std::string::npos)
		{
			return false;
		}
		std::string sKey = sKeyValuPair.substr(0, nSeparatorPos);
		// value should be trimmed
		std::string sValue = sKeyValuPair.substr(nSeparatorPos + 2);
		// key is must be case insensitive
		std::transform(sKey.begin(), sKey.end(), sKey.begin(), 
			[](char chChar) { return std::tolower(chChar); });

		if(sKey == "host")
		{
			if(sValue != sAddress)
			{
				return false;
			}
			bIsFoundHost = true;
		}
		else if(sKey == "accept")
		{
			if(sValue != "*/*")
			{
				return false;
			}
			bIsFoundAccept = true;
		}
		else if(sKey == "accept-encoding")
		{
			if(sValue != "identity")
			{
				return false;
			}
			bIsFoundAcceptEncoding = true;
		}
		nStartPos = nEndPos + 2;
	}

	nEndPos = sRequest.size() - 1;
	if(sRequest[nEndPos] != '\n' || sRequest[nEndPos - 1] != '\r' ||
		sRequest[nEndPos - 2] != '\n' || sRequest[nEndPos - 3] != '\r')
	{
		return false;
	}

	return bIsFoundHost && bIsFoundAccept && bIsFoundAcceptEncoding;
}
