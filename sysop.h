// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>

int is_sysop(int);
int cfcreate(int argc, char **argv);
int cfdelete(int argc, char **argv);
void upd_maillist(int security, const std::vector<std::string> &config, int idx);
