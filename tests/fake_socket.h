#ifndef FAKE_SOCKET_H
#define FAKE_SOCKET_H

#include <string>
#include <map>

#include "test_socket.h"
#include "sockets.h"

struct BaseParams
{
	std::string sDataToUse;
	std::string sRealResponseData;

	std::string sAddress;
	std::string sPath;

	BaseParams()
	{
	}

	BaseParams(const std::string &_sDataToUse, const std::string &_sRealResponseData, 
		const std::string &_sAddess, const std::string &_sPath) : 
		sDataToUse{_sDataToUse}, 
		sRealResponseData{_sRealResponseData}, 
		sAddress{_sAddess},
		sPath{_sPath}
	{
	}
};

struct RoutingTables 
{
public:
	std::map<CURI, BaseParams> tables;
};

// Implementing own socket for testing
// This socket should use ParametersStruct, because socket 
// is used as a template not a pointer, not to carry resource 
// managment outside downloader class
template<typename ParametersRouter> 
class RouterSocket : public Sockets::CSocket 
{
public:
	RouterSocket()
	{

	}

	bool Connect(const CURI &cURIToConnect) noexcept override
	{
		auto foundPair = m_router.tables.find(cURIToConnect);
		if(foundPair == m_router.tables.end())
		{
			return true;
		}

		m_params = foundPair->second;
		return true;
	}

	std::optional<std::vector<char>> ReadTill(const std::string &csStringToReadTill) noexcept override
	{
		if(!m_bIsShoulRespond)
		{
			return std::nullopt;
		}

		const size_t nFoundPos = m_params.sDataToUse.find(csStringToReadTill, m_nCurrentIndex);
		if(nFoundPos == std::string::npos)
		{
			return std::nullopt;
		}

		try
		{
			std::string newData = 
				m_params.sDataToUse.substr(m_nCurrentIndex, nFoundPos - m_nCurrentIndex + csStringToReadTill.size());
			m_nCurrentIndex = nFoundPos + csStringToReadTill.size();
			return std::vector<char>{newData.begin(), newData.end()};
		}
		catch(const std::exception &err)
		{
			return std::nullopt;
		}
	}

	std::optional<std::vector<char>> ReadCount(size_t nCountToRead) noexcept override
	{
		if(!m_bIsShoulRespond || 
				m_nCurrentIndex + nCountToRead > m_params.sDataToUse.size())
		{
			return std::nullopt;
		}

		std::string newData = m_params.sDataToUse.substr(m_nCurrentIndex, nCountToRead);
		m_nCurrentIndex += nCountToRead;
		return std::vector<char>{newData.begin(), newData.end()};
	}

	std::optional<std::vector<char>> ReadTillEnd() noexcept override
	{
		if(!m_bIsShoulRespond)
		{
			return std::nullopt;
		}
		
		std::string newData = m_params.sDataToUse.substr(m_nCurrentIndex);
		m_nCurrentIndex = m_params.sDataToUse.size();
		return std::vector<char>{newData.begin(), newData.end()};
	}

	bool Write(const char* pcchBytes, size_t nCount) noexcept override
	{
		std::string csGotData;
		csGotData.append(pcchBytes, nCount);

		m_bIsShoulRespond = CheckRequest(csGotData, m_params.sAddress, m_params.sPath);

		return m_bIsShoulRespond;
	}
private:
	ParametersRouter m_router;
	BaseParams m_params;
	bool m_bIsShoulRespond{false};
	size_t m_nCurrentIndex{0};
};

#endif // FAKE_SOCKET_H
