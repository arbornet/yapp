// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

#include "struct.h"

char joq_cmd_dispatch(int argc, char **argv);
void write_part(const std::string &partfile);
char read_part(const std::string &partfile, partentry_t *part, status_t *st, int idx);
