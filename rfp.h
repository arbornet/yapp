// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "struct.h"

void add_response(sumentry_t *sumthis, const std::vector<std::string> &text,
    int idx, sumentry_t *sum, partentry_t *part, status_t *stt, long art,
    const std::string_view &mid, int uid, const std::string_view &login,
    const std::string_view &fullname, int resp);
int censor(int argc, char **argv);
int uncensor(int argc, char **argv);
int preserve(int argc, char **argv);
int reply(int argc, char **argv);
int respond(int argc, char **argv);
int rfp_respond(int argc, char **argv);
char rfp_cmd_dispatch(int argc, char **argv);
#ifdef INCLUDE_EXTRA_COMMANDS
int tree(int argc, char **argv);
int sibling(int r);
int parent(int r);
int child(int r);
#endif
void dump_reply(const char *sep);
