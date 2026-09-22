#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace utils
{
    std::string toUpper(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::toupper(c));
            });

        return value;
    }
} // namespace utils
