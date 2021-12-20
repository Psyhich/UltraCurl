#ifndef URI_H
#define URI_H

#include <string>
#include <optional>

class URI {
public:
	explicit URI(const std::string& cStringToSet);

	std::optional<std::string> GetProtocol() const noexcept;
	std::optional<std::string> GetPureAddress() const noexcept;
	std::optional<int> GetPort() const noexcept;
private:
	std::string m_originalString;
};

#endif // URI_H
