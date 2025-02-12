// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "struct.h"

class Conference {
public:
	const std::string &name() const;
	const std::string &desc() const;
	const std::string &dir() const;
	const std::vector<std::string> &fairwitness() const;

private:
};

int check(int argc, char **argv);
int show_conf_index(int argc, char **argv);
char checkpoint(int idx, unsigned int sec, int silent);
int do_next(int argc, char **argv);
char join(const std::string &conf, int observe, int secure);
int leave(int argc, char **argv);
int participants(int argc, char **argv);
int describe(int argc, char **argv);
int resign(int argc, char **argv);
void ylog(int idx, const std::string_view &str);
const char *get_desc(const std::string_view &name);
const char *nextconf(void);
const char *nextnewconf(void); /* Next conference with new items in it */
const char *prevconf(void);
unsigned int security_type(const std::vector<std::string> &config, int idx);
bool is_inlistfile(int idx, const std::string &file);
bool is_fairwitness(int idx);
bool check_password(int idx);
bool is_validorigin(int idx);
std::string_view fullname_in_conference(status_t *stt);
std::vector<std::string> grab_recursive_list(const std::string &dir, const std::string &filename);
