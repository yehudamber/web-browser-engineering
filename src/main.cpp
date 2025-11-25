#include <cstdio>
#include <exception>
#include <print>
#include <string_view>

import Client;
import Display;

using namespace std::literals;

int main(int argc, char** argv) try
{
    static constexpr auto defaultUrl = "file://" RESOURCE_DIR "/example.html"sv;
    Client client(argc > 1 ? argv[1] : defaultUrl);
    display(client.load());
}
catch (const std::exception& exception)
{
    std::println(stderr, "Uncaught exception: {}", exception.what());
    return -1;
}
catch (...)
{
    std::println(stderr, "Uncaught unknown exception");
    return -1;
}