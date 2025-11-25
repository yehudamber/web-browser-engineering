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

Client::Client(std::string_view url)
{
    auto endOfScheme = url.find(SchemeSeperator);
    if (endOfScheme == std::string_view::npos)
    {
        throw std::invalid_argument("Client: URL must have a scheme");
    }
    m_scheme = url.substr(0, endOfScheme);
    url.remove_prefix(endOfScheme + SchemeSeperator.length());
    if (m_scheme == FileScheme)
    {
        m_path = url;
        if (m_path.empty())
        {
            throw std::invalid_argument("Client: 'file' URL must have a path");
        }
        return;
    }

    if (m_scheme == HttpScheme)
    {
        m_port = "80"sv;
    }
    else if (m_scheme == HttpsScheme)
    {
        m_port = "443"sv;
    }
    else
    {
        throw std::invalid_argument(std::format("Client: Unsupported URL scheme: \"{}\"", m_scheme));
    }

    auto endOfHost = url.find('/');
    if (endOfHost == std::string_view::npos)
    {
        m_host = url;
        m_path = "/"sv;
    }
    else
    {
        m_host = url.substr(0, endOfHost);
        m_path = url.substr(endOfHost);
    }

    if (auto portSeparator = m_host.find(':'); portSeparator != std::string_view::npos)
    {
        if (portSeparator + 1 < m_host.length())
        {
            m_port.assign(m_host, portSeparator + 1);
        }
        m_host.erase(portSeparator);
    }

    if (m_host.empty())
    {
        throw std::invalid_argument("Client: URL must have a host");
    }
}

template <typename Socket>
std::string httpLoad(Socket& socket, const std::string& host, const std::string& path)
{
    asio::streambuf request;
    std::format_to(std::ostreambuf_iterator(&request),
                   "GET {} HTTP/1.1\r\n"
                   "Host: {}\r\n"
                   "Connection: close\r\n"
                   "User-Agent: web-browser-engineering\r\n"
                   "\r\n",
                   path, host);
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

std::string Client::load() const
{
    if (m_scheme == FileScheme)
    {
        std::filebuf file;
        if (!file.open(m_path, std::ios::in))
        {
            throw std::runtime_error(std::format("Client: failed to open file \"{}\"", m_path));
        }
        return std::string(std::istreambuf_iterator(&file), {});
    }

    asio::io_context ioContext;
    tcp::resolver resolver(ioContext);
    tcp::socket socket(ioContext);

    asio::connect(socket, resolver.resolve(m_host, m_port));

    if (m_scheme == HttpsScheme)
    {
        asio::ssl::context sslContext(asio::ssl::context::sslv23);
        asio::ssl::stream<tcp::socket&> sslSocket(socket, sslContext);
        sslSocket.handshake(asio::ssl::stream_base::client);
        return httpLoad(sslSocket, m_host, m_path);
    }

    return httpLoad(socket, m_host, m_path);
}
