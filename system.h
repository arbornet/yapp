// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <stdio.h>

#include <string>
#include <string_view>

#include "config.h"

int unix_cmd(const std::string &cmd);
FILE *spopen(const std::string &cmd);
FILE *smopenw(const std::string &file, long flg);
FILE *smopenr(const std::string &file, long flg);
int smclose(FILE *fp);
int spclose(FILE *fp);
int sdpopen(FILE **finp, FILE **foutp, const std::string &cmd);
int sdpclose(FILE *, FILE *fp);
int edit(const std::string &dir, const std::string &file, bool visual);
int priv_edit(const std::string &dir, const std::string &file, int flags);
int rm(const std::string &file, int sec);
int copy_file(const std::string &src, const std::string &dest, int sec);
int move_file(const std::string &src, const std::string &dest, int sec);

/* Security levels */
#define SL_OWNER 0
#define SL_USER 1

#ifdef HAVE_DBM_OPEN
int save_dbm(const std::string &userfile, const std::string_view &key, const std::string_view &value, int suid);
std::string get_dbm(const std::string &userfile, const std::string_view &key, int suid);
void dump_dbm(const std::string &file);
#endif
