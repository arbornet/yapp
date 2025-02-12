// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <stdio.h>

#include <string>
#include <vector>

#include "struct.h"

class Item {
public:
	std::string filename() const;

private:
	size_t itemno;
};

#define RF_NORMAL 0x0000
#define RF_CENSORED 0x0001
#define RF_SCRIBBLED 0x0002
#define RF_EXPIRED 0x0004

int do_enter(sumentry_t *sumthis,
    const std::string_view &sub, const std::vector<std::string> &texta,
    int idx, sumentry_t *sum, partentry_t *part, status_t *stt, long art,
    const std::string_view &mid, int uid, const std::string_view &login,
    const std::string_view &fullname);
int do_find(int argc, char **argv);
int do_kill(int argc, char **argv);
int do_read(int argc, char **argv);
int enter(int argc, char **argv);
int fixseen(int argc, char **argv);
int fixto(int argc, char **argv);
int forget(int argc, char **argv);
int freeze(int argc, char **argv);
int linkfrom(int argc, char **argv);
int remember(int argc, char **argv);
void show_header(void);
void show_resp(FILE *fp);
void show_range(void);
void show_nsep(FILE *fp);
int nextitem(int inc);
int is_enterer(int item);
