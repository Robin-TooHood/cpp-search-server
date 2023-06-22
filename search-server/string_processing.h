#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <set>
#include <cstdint>

std::vector<std::string_view> SplitIntoWordsView(std::string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(StringContainer strings)
{
    std::set<std::string, std::less<>> non_empty_strings;
    for (auto str : strings)
    {
        if (!str.empty())
        {
            std::string new_str(str);
            non_empty_strings.insert(new_str);
        }
    }
    return non_empty_strings;
}