module;

#include <string>
#include <string_view>
#include <variant>

export module Client;

struct NetworkUrlData
{
    std::string m_host;
    std::string m_port;
    std::string m_path;
};

struct FileUrlData
{
    std::string m_path;
};

struct DataUrlData
{
    std::string m_type;
    std::string m_content;
};

export
class Client
{
public:
    explicit Client(std::string_view url);

    std::string load() const;

private:
    std::string m_scheme;
    std::variant<NetworkUrlData, FileUrlData, DataUrlData> m_data;
};
