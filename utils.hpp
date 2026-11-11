#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef>
#include <string>
#include <string_view>

size_t findSemicolon(std::string_view sv);
std::string urlDecode(std::string src);
bool isTChar(unsigned char c);

#endif
