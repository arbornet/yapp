// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/types.h>

#include <ctime>
#include <string>
#include <string_view>

#ifdef INCLUDE_EXTRA_COMMANDS
std::string get_ticket(std::time_t tm, const std::string_view &who);
int authenticate(int argc, char **argv);
#endif
void refresh_list(void);
int do_cflist(int argc, char **argv);
int passwd(int argc, char **argv);
int chfn(int argc, char **argv);
int newuser(int argc, char **argv);
const std::string &get_sysop_login(void);
uid_t get_nobody_uid(void);
int get_user(uid_t *uid, std::string &login, std::string &fullname, std::string &home, std::string &email);
void login_user(void);
int del_cflist(const std::string &cfname);
void show_cflist(void);
const char *email2login(const std::string_view &email);
int partfile_perm(void);
std::string get_partdir(const std::string_view &login);
std::string get_userfile(const std::string_view &who, int *suid = nullptr);
bool sane_fullname(const std::string_view &name);
