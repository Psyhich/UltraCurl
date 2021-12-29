#ifndef HTTP_DOWNLOADER_H
#define HTTP_DOWNLOADER_H

#include <string>
#include <optional>
#include <vector>
#include <map>
#include <numeric>
#include <iostream>

#include "sockets.h"
#include "response.h"

namespace Downloaders 
{

	///	Class responsible for downloading any given file 
	///	using HTTP application protocol.
	///	It can use any socket class that inherits CSocket
	template<class SocketClass> class CHTTPDownloader
	{
	public:
		CHTTPDownloader() {}
		std::optional<HTTP::CHTTPResponse> Download(const std::string &csUriString)
		{
			static_assert(std::is_base_of<Sockets::CSocket, SocketClass>::value, "Class should inherit CSocket");
			CURI parsedURI = csUriString;
			// Firstly we should form request for some server and sent it
			// After that we can read all data from socket and parse it as response 

			std::map<std::string, std::string> headers;
			// Adding needed headers for basic request
			if(auto hostAddress = parsedURI.GetPureAddress())
			{
				headers.insert({std::string("Host"), *hostAddress});
			} else
			{
				fprintf(stderr, "Given URI is invalid\n");
				return std::nullopt;
			}
			headers.insert({std::string("Accept"), std::string("*/*")});
			headers.insert({std::string("Accept-Encoding"), std::string("identity")});
			// TODO: think about adding encoding support with gzip and other libraries
			
			// Forming request
			// Getting path
			std::string path;
			if(const auto uriPath = parsedURI.GetPath())
			{
				path = std::move(*uriPath);
			} else
			{
				path = "/";
			}

			std::string sRequest = "GET " + path + " HTTP/1.1\r\n";
			// Adding headers
			AddHeaders(headers, sRequest);
			sRequest += "\r\n";

			SocketClass *pSocket = new SocketClass(csUriString);
			// Sending request
			if(!pSocket->Write(sRequest.data(), sRequest.size()))
			{
				fprintf(stderr, "Failed to send request!\n");
				return std::nullopt;
			}

			HTTP::CHTTPResponse response;
			const auto cHeaderResponse = pSocket->ReadTill("\r\n\r\n");
			if(!cHeaderResponse)
			{
				fprintf(stderr, "Failed to recieve response from server\n");
				return std::nullopt;
			}

			if(!response.LoadHeaders(*cHeaderResponse))
			{
				fprintf(stderr, "Failed to parse server response\n");
				return std::nullopt;
			}

			// To identify length of we have 5 ways
			// 1. HEAD requests should not have body(we use GET)
			// 2. CONNECT requests should not have body(we still use GET)
			// 3. If we have Transfer-Encoding header we should read data by chunks(I don't use them here)
			// 4. If we have 2 Content-Length headers we treat response as unrecoverable error(can't happen here)
			// 5. If we have Content-Length header, we read that amount of bytes(ReadCount)
			// 6. If all 5 before are false we can assume that there are no content
			// 7. We CAN check content by trying to recieve data before connection closes(ReadTillEnd)
			const HTTP::Headers &cResponseHeaders = response.GetHeaders();

			const auto cContentLengthHeader = 
				cResponseHeaders.find("Content-Length");
			const auto cTransferEncoding = 
				cResponseHeaders.find("Transfer-Encoding");

			std::optional<std::vector<char>> possiblyReadData;
			bool bIsReadData = false;

			if(cTransferEncoding != cResponseHeaders.end() && 
				cTransferEncoding->second == "chunked")
			{
				possiblyReadData = ReadByChunks(pSocket);
				bIsReadData = true;
			}
			else if(cContentLengthHeader != cResponseHeaders.end())
			{
				// Parsing bytes count value
				size_t nBytesCount = 0;
				size_t nNumberEndPosition;
				try
				{
					nBytesCount = std::stoull(cContentLengthHeader->second, &nNumberEndPosition);
				}
				catch(const std::exception &err)
				{
					fprintf(stderr, "Failed to determine length of content\n");
					return std::nullopt;
				}
				// Checking if headers is invalid
				if(nNumberEndPosition == cContentLengthHeader->second.size())
				{
					possiblyReadData = pSocket->ReadCount(nBytesCount);
				}
				bIsReadData = true;
			}
			else
			{
				possiblyReadData = pSocket->ReadTillEnd();
				bIsReadData = true;
			}

			if(bIsReadData && !possiblyReadData)
			{
				fprintf(stderr, "Failed to read response data\n");
				return std::nullopt;
			}
			response.LoadData(*possiblyReadData);

			return response;
		}
	private:
		void AddHeaders(const std::map<std::string, std::string> &cHeadersMap, std::string &requestString)
		{
			for(const auto &[csKey, csValue] : cHeadersMap)
			{
				requestString += csKey + ": " + csValue + "\r\n";
			}
		}
		std::optional<std::vector<char>> ReadByChunks(SocketClass *socket) noexcept
		{
			// If we have chunked encoding we should read till first CRLF and 
			// Parse those hexadecimal as number of bytes
			std::optional<size_t> nCountToRead;

			std::vector<char> readChunks;
			nCountToRead = ReadChunkSize(socket);
			while(nCountToRead)
			{
				if(*nCountToRead == 0)
				{
					return readChunks;
				}

				readChunks.reserve(readChunks.size() + *nCountToRead);
				// Taking in mind last CRLF, it will be excluded, but needs to be read
				if(const auto cReadData = socket->ReadCount(*nCountToRead + 2))
				{
					for(size_t nIndex = 0; nIndex < cReadData->size() - 2; nIndex++)
					{
						readChunks.push_back((*cReadData)[nIndex]);
					}
				}
				else
				{
					return std::nullopt;
				}
				nCountToRead = ReadChunkSize(socket);
			}
			return std::nullopt;
		}

		std::optional<size_t> ReadChunkSize(SocketClass *socket) noexcept
		{
			// Checking 
			if(auto readHexadecimal = socket->ReadTill("\r\n"))
			{

				readHexadecimal->push_back('\0');
				std::string sHexadecimal = "0x";
				sHexadecimal += readHexadecimal->data();

				// Parsing hexadecimal
				try
				{
					size_t size = std::stoull(sHexadecimal, nullptr, 16);
					return size;
				}
				catch(std::exception &err)
				{
					return std::nullopt;
				}
			}
			return std::nullopt;

		}
	};

} // Downloaders


#endif // HTTP_DOWNLOADER_H
