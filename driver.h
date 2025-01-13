#pragma once

#include <stdio.h>

#include <string>
#include <string_view>

void endbbs(int);
void init(int, char **);
char source(const std::string &dir, const std::string &file, int fl, int sec);
char command(const std::string &cmd, int lvl);
void print_prompt(int mode);
char ok_cmd_dispatch(int argc, char **argv);
bool get_command(const std::string_view &deflt, int lvl);
void handle_int(int);
void handle_pipe(int);
void handle_other(int sig, int code, void *scp, char *addr);
void open_cluster(const std::string &bdir, const std::string &hdir);
void open_pipe(void);
void ints_on(void);
void ints_off(void);
void push_stdin(FILE *fp, int type);
void pop_stdin(void);
