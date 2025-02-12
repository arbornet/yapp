// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

int text_read(int argc, char **argv);
int text_write(int argc, char **argv);
int text_edit(int argc, char **argv);
int text_print(int argc, char **argv);
int text_clear(int argc, char **argv);
int text_abort(int argc, char **argv);
int text_done(int argc, char **argv);
char text_cmd_dispatch(int argc, char **argv);
char text_loop(bool is_new, const char *label);
char edb_cmd_dispatch(int argc, char **argv);
