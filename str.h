// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace str {

constexpr size_t nidx = ~0uz;

std::intmax_t toi(const std::string_view &str);
std::string concat(const std::vector<std::string_view> &sv);
std::string join(const std::string_view &sep, const std::vector<std::string_view> &sv);
std::string_view ltrim(std::string_view, const std::string_view ws = " \t\f\v\r\n");
std::string_view rtrim(std::string_view, const std::string_view ws = " \t\f\v\r\n");
std::string_view trim(std::string_view, const std::string_view ws = " \t\f\v\r\n");
bool eq(const std::string_view &a, const std::string_view &b);
bool eqcase(const std::string_view &a, const std::string_view &b);
bool starteqcase(const std::string_view &s, const std::string_view &prefix);
bool match(const std::string_view &a, const std::string_view &b);
bool contains(const std::vector<std::string> &arr, const std::string_view &key);
bool contains(const std::vector<std::string_view> &arr, const std::string_view &key);
std::vector<std::string_view> split(const std::string_view &str, const std::string_view &sep, bool discard_empty = true);
std::vector<std::string> splits(const std::string_view &str, const std::string_view &sep, bool discard_empty = true);
std::string unquote(const std::string_view &str);
std::string &lowercase(std::string &str);
std::string lowercase(const std::string_view &str);
std::string strip(const std::string_view &str, const std::string_view &chars);
}  // namespace str
