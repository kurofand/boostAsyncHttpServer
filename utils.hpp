#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef>
#include <string>
#include <string_view>

size_t findSemicolon(std::string_view sv);
bool urlDecode(std::string_view src, std::string &res);
bool isTChar(unsigned char c);

#endif
