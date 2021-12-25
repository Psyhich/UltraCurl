#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>
#include <vector>
#include <map>

namespace HTTP 
{
	using Headers = std::map<std::string, std::string>;
	class CHTTPResponse {
	public:
		inline bool IsSuccess() const noexcept
		{
			return m_iCode >= 200 && m_iCode < 300;
		}

		inline int GetCode() const noexcept
		{
			return m_iCode;
		}

		inline const std::vector<char>& GetData() const noexcept
		{
			return m_data;
		}
		inline const Headers& GetHeaders() const noexcept
		{
			return m_headers;
		}

		/*
		* Function to load all(headers, body) data from vector
		*/
		bool LoadAll(const std::vector<char> &cDataToParse) noexcept;

		/*
		* Function to extract header data from vector
		*/
		bool LoadHeaders(const std::vector<char> &cDataToParse) noexcept;

		/*
		* Loads data to response body
		*/
		void LoadData(const std::vector<char> &cDataToLoad) noexcept;

	private:
		bool LoadStatusLine(size_t &nIndex, const std::vector<char> &cDataToParse) noexcept;
		bool LoadHeaders(size_t &nIndex, const std::vector<char> &cDataToParse) noexcept;
		void LoadData(size_t &nIdex, const std::vector<char> &cDataToLoad) noexcept; 
	private:
		int m_iCode{0};
		Headers m_headers;
		std::vector<char> m_data;
	};

}

#endif // RESPONSE_H
