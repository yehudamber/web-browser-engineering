module;

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

constexpr auto SchemeSeperator = "://"sv;
constexpr auto HttpScheme = "http"sv;
constexpr auto HttpsScheme = "https"sv;
constexpr auto FileScheme = "file"sv;

auto parseNetworkUrl(const std::string& scheme, std::string_view url)
{
    auto port = scheme == HttpScheme ? "443"sv : "80"sv;

    std::string_view host;
    std::string_view path;
    auto endOfHost = url.find('/');
    if (endOfHost == std::string_view::npos)
    {
        host = url;
        path = "/"sv;
    }
    else
    {
        host = url.substr(0, endOfHost);
        path = url.substr(endOfHost);
    }

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
    if (url.empty())
    {
        throw std::invalid_argument("Client: 'file' URL must have a path");
    }
    return FileUrlData{.m_path{url}};
}

Client::Client(std::string_view url)
{
    auto endOfScheme = url.find(SchemeSeperator);
    if (endOfScheme == std::string_view::npos)
    {
        throw std::invalid_argument("Client: URL must have a scheme");
    }
    m_scheme = url.substr(0, endOfScheme);
    url.remove_prefix(endOfScheme + SchemeSeperator.length());
    if (m_scheme == HttpScheme || m_scheme == HttpsScheme)
    {
        m_data = parseNetworkUrl(m_scheme, url);
    }
    else if (m_scheme == FileScheme)
    {
        m_data = parseFileUrl(url);
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

    return loadFromNetwork(m_scheme, std::get<NetworkUrlData>(m_data));
}
