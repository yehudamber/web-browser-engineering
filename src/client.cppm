module;

#include <format>
#include <string>
#include <string_view>

export module Client;

export
class Client
{
public:
    explicit Client(std::string_view url);

    std::string toString() const;

    std::string load() const;

private:
    std::string m_scheme;
    std::string m_host;
    std::string m_port;
    std::string m_path;
};

std::string Client::toString() const
{
    return std::format("{{scheme = {}, host = {}, port = {}, path = {}}}", m_scheme, m_host, m_port, m_path);
}
