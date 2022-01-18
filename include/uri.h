#ifndef MY_URI_H
#define MY_URI_H

#include <string>
#include <filesystem>
#include <optional>

/// Utility class for easier parsing of URIs 
/// specified by RFC3986
class CURI {
public:
	CURI(const std::string& cStringToSet);

	inline std::string GetFullURI() const noexcept 
	{
		return m_originalString;
	}
	std::optional<std::string> GetProtocol() const noexcept;
	std::optional<int> GetPort() const noexcept;
	std::optional<std::string> GetPureAddress() const noexcept;
	std::optional<std::filesystem::path> GetPath() const noexcept;
	std::optional<std::string> GetQuery() const noexcept;
	std::optional<std::string> GetFragment() const noexcept;

	friend bool operator<(const CURI& cLURIToCompare, const CURI &cRURIToCompare) noexcept;
	friend bool operator==(const CURI& cLURIToCompare, const CURI &cRURIToCompare) noexcept;
private:
	static inline bool CanBeUsedInProtocol(char chCharToCheck) noexcept
	{
		return (chCharToCheck >= 'A' && chCharToCheck <= 'Z') || 
			(chCharToCheck >= 'a' && chCharToCheck <= 'z') ||
			chCharToCheck == '.' || chCharToCheck == '+' ||
			chCharToCheck == '-';
	}
private:
	static const constexpr std::array<char, 4> POSSIBLE_ADDRESS_END{':', '#', '?', '/'};
	static const constexpr std::array<char, 3> POSSIBLE_PORT_END{'#', '?', '/'};
	static const constexpr std::array<char, 2> POSSIBLE_PATH_END{'#', '?'};
	static const std::string sProtocolSeparator;
	std::string m_originalString;
};


#endif // MY_URI_H
