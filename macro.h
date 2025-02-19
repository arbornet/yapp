// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "struct.h"

/* Define macro mask */
constexpr mask_t DM_OK        = 0b0000'0000'0000'0001;
constexpr mask_t DM_VAR       = 0b0000'0000'0000'0010;
constexpr mask_t DM_PARAM     = 0b0000'0000'0000'0100;
constexpr mask_t DM_RFP       = 0b0000'0000'0000'1000;
constexpr mask_t DM_SUPERSANE = 0b0000'0000'0100'0000;
constexpr mask_t DM_SANE      = 0b0000'0000'1000'0000;
constexpr mask_t DM_ENVAR     = 0b0000'0001'0000'0000;
constexpr mask_t DM_CONSTANT  = 0b1000'0000'0000'0000;

void undef_name(const std::string_view &name);
std::string expand(const std::string_view &mac, mask_t mask);
std::string capexpand(const std::string_view &mac, mask_t mask, bool cap);
int define(int argc, char **argv);
void def_macro(const std::string_view &name, mask_t mask, const std::string_view &value);
std::optional<std::reference_wrapper<macro_t>> find_macro(const std::string_view &name, mask_t mask);
void undefine(mask_t mask);
std::string conference(bool cap = false);
inline std::string Conference(void) { return conference(true); }
std::string fairwitness(bool cap = false);
inline std::string Fairwitness(void) { return fairwitness(true); }
std::string topic(bool cap = false);
inline std::string Topic(void) { return topic(true); }
std::string subject(bool cap = false);
inline std::string Subject(void) { return subject(true); }
