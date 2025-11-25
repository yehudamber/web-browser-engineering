module;

#include <cstdio>
#include <string_view>

export module Display;

export
void display(const std::string_view& content)
{
    auto inTag = false;
    for (auto c : content)
    {
        if (c == '<')
        {
            inTag = true;
        }
        else if (c == '>')
        {
            inTag = false;
        }
        else if (!inTag)
        {
            std::putchar(c);
        }
    }
}