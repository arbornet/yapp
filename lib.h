// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdio>
#include <ctime>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "struct.h"

/* Grab file options */
#define GF_SILENT 0x0001
#define GF_WORD 0x0002
#define GF_HEADER 0x0004
#define GF_IGNCMT 0x0008   /* ignore comment lines? */
#define GF_NOHEADER 0x0010 /* don't require file header */

constexpr size_t nidx = ~0z;

std::string_view strim(std::string_view, const std::string_view ws = " ");
std::string_view strimw(std::string_view);

char *trim(char *str);
bool match(const char *a, const char *b);
bool match(const std::string_view &a, const std::string_view &b);
bool cat(const std::string_view &dir, const std::string_view &file);
std::vector<std::string> grab_file(const std::string_view &dir, const std::string_view &file, int flags);
std::vector<std::string> grab_more(FILE *fp, const char *end, size_t *endlen);
bool ngets(std::string &str, FILE *fp);
char *xgets(FILE *fp, int lvl);
std::vector<assoc_t> grab_list(const std::string_view &dir, const std::string_view &file, int flags);
std::size_t get_idx(const std::string_view &elt, const std::vector<assoc_t> &list);
const assoc_t *assoc_list_find(const std::vector<assoc_t> &vec, const std::string &key);
bool get_yes(const std::string_view &prompt, bool dflt);
std::string get_date(std::time_t t, int sty);
std::string compress(const std::string_view &str);
std::string noquote(const std::string_view &str);
void error(const std::string_view &str1, const std::string_view &str2 = "");
std::string &lower_case(std::string &str);
char *lower_case(char *str);
bool write_file(const std::string &file, const std::string_view &buff);
bool more(const std::string_view &dir, const std::string_view &filename);
void mkdir_all(const std::string &path, int mode);
