#ifndef HTTP_DOWNLOADER_H
#define HTTP_DOWNLOADER_H

#include <cstddef>
#include <string>
#include <optional>
#include <vector>
#include <map>
#include <numeric>

#include "sockets.h"
#include "response.h"

namespace Downloaders 
{

	/// Class responsible for downloading any given file 
	/// using HTTP application protocol.
	/// It can use any socket class that inherits CSocket

	class CHTTPDownloader
	{
	using SocketClass = std::unique_ptr<Sockets::CSocket>;
	public:
		CHTTPDownloader(SocketClass&& pSocketToUse) : 
			m_pSocket{std::move(pSocketToUse)}
		{

		}

		std::optional<HTTP::CHTTPResponse> Download(const CURI &cURI)
		{
			std::optional<HTTP::CHTTPResponse> response{HTTP::CHTTPResponse()};
			do
			{
				// Connecting to given address
				if(!m_pSocket->Connect(cURI))
				{
					response = std::nullopt;
					break;
				}
				// Firstly we should form request for some server and sent it
				// After that we can read all data from socket and parse it as response 
				const std::optional<std::string> csRequest = ConstructRequest(cURI);
				if(!csRequest)
				{
					response = std::nullopt;
					break;
				}

				// Sending request
				if(!m_pSocket->Write(csRequest->data(), csRequest->size()))
				{
					fprintf(stderr, "Failed to send request!\n");
					response = std::nullopt;
					break;
				}

				if(!LoadResponseHeaders(*response))
				{
					response = std::nullopt;
					break;
				}

				if(!LoadResponseData(*response))
				{
					response = std::nullopt;
					break;
				}
			} while(false);

			m_pSocket.reset(nullptr);
			return response;
		}

		std::optional<std::tuple<std::size_t, std::size_t>> GetProgress() const noexcept
		{
			if(!m_pSocket)
			{
				return std::nullopt;
			}

			if(auto readBytes = m_pSocket->GetReadBytes() ; 
				auto bytesToDownload = m_pSocket->GetBytesToRead())
			{
				return std::make_tuple(*readBytes, *bytesToDownload);
			}
			return std::nullopt;
		}

	private:
		std::optional<std::string> ConstructRequest(const CURI &cURI) noexcept
		{
			std::map<std::string, std::string> headers;
			// Adding needed headers for basic request
			if(auto hostAddress = cURI.GetPureAddress())
			{
				headers.insert({std::string("Host"), *hostAddress});
			} else
			{
				fprintf(stderr, "Given URI is invalid\n");
				return std::nullopt;
			}
			headers.insert({std::string("Accept"), std::string("*/*")});
			headers.insert({std::string("Accept-Encoding"), std::string("identity, zstd")});
			
			// Forming request
			// Getting path
			std::string sPath;
			if(const auto uriPath = cURI.GetPath())
			{
				sPath = std::move(*uriPath);
			} else
			{
				sPath = "/";
			}
			// Adding query and fragment to path
			if(const auto csQuery = cURI.GetQuery())
			{
				sPath += '?' + *csQuery;
			}
			if(const auto csFragment = cURI.GetFragment())
			{
				sPath += '#' + *csFragment;
			}

			std::string sRequest = "GET " + sPath + " HTTP/1.1\r\n";
			// Adding headers
			AddHeaders(headers, sRequest);
			sRequest += "\r\n";

			return sRequest;
		}

		bool LoadResponseHeaders(HTTP::CHTTPResponse &response) noexcept
		{
			if(m_pSocket == nullptr)
			{
				return false;
			}

			const auto cHeaderResponse = m_pSocket->ReadTill("\r\n\r\n");
			if(!cHeaderResponse)
			{
				fprintf(stderr, "Failed to recieve response from server\n");
				return false;
			}

			if(!response.LoadHeaders(*cHeaderResponse))
			{
				fprintf(stderr, "Failed to parse server response\n");
				return false;
			}

			return true;
		}

		bool LoadResponseData(HTTP::CHTTPResponse &response) noexcept
		{
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
				cResponseHeaders.find("content-length");
			const auto cTransferEncoding = 
				cResponseHeaders.find("transfer-encoding");

			std::optional<std::vector<char>> possiblyReadData;
			bool bIsReadData = false;

			// Check for chunked encoding
			if(cTransferEncoding != cResponseHeaders.end() && 
				cTransferEncoding->second == "chunked")
			{
				possiblyReadData = ReadByChunks();
				bIsReadData = true;
			}
			// Check for content length
			else if(cContentLengthHeader != cResponseHeaders.end())
			{
				// Parsing bytes count value
				std::size_t nBytesCount{0};
				std::size_t nNumberEndPosition{0}; // Will be changed by stoull
				try
				{
					nBytesCount = std::stoull(cContentLengthHeader->second, &nNumberEndPosition);
				}
				catch(const std::exception &err)
				{
					fprintf(stderr, "Failed to determine length of content\n");
					return false;
				}
				// Checking if headers is valid
				if(nNumberEndPosition == cContentLengthHeader->second.size())
				{
					possiblyReadData = m_pSocket->ReadCount(nBytesCount);
				}
				bIsReadData = true;
			}
			// Trying to read all data till end of connection
			else
			{
				possiblyReadData = m_pSocket->ReadTillEnd();
				bIsReadData = true;
			}

			if(bIsReadData && !possiblyReadData)
			{
				fprintf(stderr, "Failed to read response data\n");
				return false;
			}

			response.LoadData(std::move(*possiblyReadData));

			return true;
		}
		
		void AddHeaders(const std::map<std::string, std::string> &cHeadersMap, std::string &requestString) noexcept
		{
			for(const auto &[csKey, csValue] : cHeadersMap)
			{
				requestString += csKey + ": " + csValue + "\r\n";
			}
		}
		std::optional<std::vector<char>> ReadByChunks() noexcept
		{
			// If we have chunked encoding we should read till first CRLF and 
			// Parse those hexadecimal as number of bytes
			std::optional<std::size_t> nCountToRead;

			std::vector<char> readChunks;
			nCountToRead = ReadChunkSize();
			while(nCountToRead)
			{
				if(*nCountToRead == 0)
				{
					return readChunks;
				}

				readChunks.reserve(readChunks.size() + *nCountToRead);
				// Taking in mind last CRLF, it will be excluded, but needs to be read
				if(const auto cReadData = m_pSocket->ReadCount(*nCountToRead + 2))
				{
					for(std::size_t nIndex = 0; nIndex < cReadData->size() - 2; nIndex++)
					{
						readChunks.push_back((*cReadData)[nIndex]);
					}
				}
				else
				{
					return std::nullopt;
				}
				nCountToRead = ReadChunkSize();
			}
			return std::nullopt;
		}

		std::optional<std::size_t> ReadChunkSize() noexcept
		{
			// Checking 
			if(auto readHexadecimal = m_pSocket->ReadTill("\r\n"))
			{

				readHexadecimal->push_back('\0');
				std::string sHexadecimal = "0x";
				sHexadecimal += readHexadecimal->data();

				// Parsing hexadecimal
				try
				{
					std::size_t size = std::stoull(sHexadecimal, nullptr, 16);
					return size;
				}
				catch(std::exception &err)
				{
					return std::nullopt;
				}
			}
			return std::nullopt;

		}
	private:
		SocketClass m_pSocket{nullptr};
	};

} // Downloaders


#endif // HTTP_DOWNLOADER_H
