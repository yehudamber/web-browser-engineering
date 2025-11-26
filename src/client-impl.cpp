module;

#include <filesystem>
#include <format>
#include <fstream>
#include <istream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <variant>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

module Client;

using namespace std::literals;

namespace asio = boost::asio;
using asio::ip::tcp;

constexpr auto HttpScheme = "http"sv;
constexpr auto HttpsScheme = "https"sv;
constexpr auto FileScheme = "file"sv;
constexpr auto DataScheme = "data"sv;

auto parseNetworkUrl(const std::string& scheme, std::string_view url)
{
    if (url.length() < 2 || url[0] != '/' || url[1] != '/')
    {
        throw std::invalid_argument("Client: Network URL must begin with '<scheme>://'");
    }
    url.remove_prefix(2);

    std::string_view host;
    std::string_view path;
    if (auto endOfHost = url.find('/'); endOfHost == std::string_view::npos)
    {
        host = url;
        path = "/"sv;
    }
    else
    {
        host = url.substr(0, endOfHost);
        path = url.substr(endOfHost);
    }

    auto port = scheme == HttpScheme ? "443"sv : "80"sv;
    if (auto portSeparator = host.find(':'); portSeparator != std::string_view::npos)
    {
        if (portSeparator + 1 < host.length())
        {
            port = host.substr(portSeparator + 1);
        }
        host.remove_suffix(host.length() - portSeparator);
    }

    if (host.empty())
    {
        throw std::invalid_argument("Client: URL must have a host");
    }

    return NetworkUrlData{.m_host{host}, .m_port{port}, .m_path{path}};
}

auto parseFileUrl(std::string_view url)
{
    if (url.length() >= 2 && url[0] == '/' && url[1] == '/')
    {
        url.remove_prefix(2);
    }
    if (url.empty())
    {
        throw std::invalid_argument("Client: 'file' URL must have a path");
    }
    if (std::filesystem::path(url).is_relative())
    {
        throw std::invalid_argument("Client: 'file' URL path must be absolute");
    }
    return FileUrlData{.m_path{url}};
}

auto parseDataUrl(std::string_view url)
{
    auto endOfType = url.find(',');
    if (endOfType == std::string_view::npos)
    {
        throw std::invalid_argument("Client: 'data' URL must have a comma before its content");
    }
    if (endOfType + 1 == url.length())
    {
        throw std::invalid_argument("Client: 'data' URL must have a content");
    }
    return DataUrlData{.m_type{url.substr(0, endOfType)}, .m_content{url.substr(endOfType + 1)}};
}

Client::Client(std::string_view url)
{
    auto endOfScheme = url.find(':');
    if (endOfScheme == std::string_view::npos)
    {
        throw std::invalid_argument("Client: URL must have a scheme");
    }
    m_scheme = url.substr(0, endOfScheme);
    url.remove_prefix(endOfScheme + 1);
    if (m_scheme == HttpScheme || m_scheme == HttpsScheme)
    {
        m_data = parseNetworkUrl(m_scheme, url);
    }
    else if (m_scheme == FileScheme)
    {
        m_data = parseFileUrl(url);
    }
    else if (m_scheme == DataScheme)
    {
        m_data = parseDataUrl(url);
    }
    else
    {
        throw std::invalid_argument(std::format("Client: Unsupported URL scheme: \"{}\"", m_scheme));
    }
}

auto loadFile(const FileUrlData& data)
{
    std::filebuf file;
    if (!file.open(data.m_path, std::ios::in))
    {
        throw std::runtime_error(std::format("Client: failed to open file \"{}\"", data.m_path));
    }
    return std::string(std::istreambuf_iterator(&file), {});
}

template <typename Socket>
auto httpLoad(Socket& socket, const NetworkUrlData& data)
{
    asio::streambuf request;
    std::format_to(std::ostreambuf_iterator(&request),
                   "GET {} HTTP/1.1\r\n"
                   "Host: {}\r\n"
                   "Connection: close\r\n"
                   "User-Agent: web-browser-engineering\r\n"
                   "\r\n",
                   data.m_path, data.m_host);
    asio::write(socket, request);

    asio::streambuf response;
    boost::system::error_code ec;
    asio::read(socket, response, ec);
    if (ec != asio::error::eof)
    {
        throw std::system_error(ec);
    }

    std::istream responseStream(&response);
    std::string line;
    std::getline(responseStream, line);
    std::unordered_map<std::string, std::string> headers;
    for (std::getline(responseStream, line); line != "\r"sv; std::getline(responseStream, line))
    {
        auto colon = line.find(':');
        if (colon == std::string_view::npos)
        {
            throw std::runtime_error("Client: ill-formed HTTP response");
        }
        auto header = line.substr(0, colon);
        boost::algorithm::to_lower(header);
        auto value = line.substr(colon + 1);
        boost::algorithm::trim(value);
        headers.emplace(std::move(header), std::move(value));
    }
    if (headers.contains("transfer-encoding"s) || headers.contains("content-encoding"s))
    {
        throw std::runtime_error("Client: chunked transfer encoding or content encoding are not supported");
    }
    auto content = std::string(std::istreambuf_iterator(&response), {});
    boost::algorithm::replace_all(content, "\r\n", "\n");
    return content;
}

auto loadFromNetwork(const std::string& scheme, const NetworkUrlData& data)
{
    asio::io_context ioContext;
    tcp::resolver resolver(ioContext);
    tcp::socket socket(ioContext);

    asio::connect(socket, resolver.resolve(data.m_host, data.m_port));

    if (scheme == HttpsScheme)
    {
        asio::ssl::context sslContext(asio::ssl::context::sslv23);
        asio::ssl::stream<tcp::socket&> sslSocket(socket, sslContext);
        sslSocket.handshake(asio::ssl::stream_base::client);
        return httpLoad(sslSocket, data);
    }

    return httpLoad(socket, data);
}

std::string Client::load() const
{
    if (m_scheme == FileScheme)
    {
        return loadFile(std::get<FileUrlData>(m_data));
    }
    if (m_scheme == DataScheme)
    {
        return std::get<DataUrlData>(m_data).m_content;
    }

    return loadFromNetwork(m_scheme, std::get<NetworkUrlData>(m_data));
}
