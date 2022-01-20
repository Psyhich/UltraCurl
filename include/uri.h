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
	std::optional<std::string> GetPureAddress() const noexcept;
	std::optional<std::filesystem::path> GetPath() const noexcept;
	std::optional<int> GetPort() const noexcept;

	friend bool operator<(const CURI& cLURIToCompare, const CURI &cRURIToCompare) noexcept;
private:
	static inline bool CanBeUsedInProtocol(char chCharToCheck) noexcept
	{
		return (chCharToCheck >= 'A' && chCharToCheck <= 'Z') || 
			(chCharToCheck >= 'a' && chCharToCheck <= 'z') ||
			chCharToCheck == '.' || chCharToCheck == '+' ||
			chCharToCheck == '-';
	}
private:
	std::string m_originalString;
};


#endif // MY_URI_H
