#ifndef MY_URI_H
#define MY_URI_H

#include <string>
#include <optional>

class URI {
public:
	URI(const std::string& cStringToSet);

	std::optional<std::string> GetProtocol() const noexcept;
	std::optional<std::string> GetPureAddress() const noexcept;
	std::optional<int> GetPort() const noexcept;
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
