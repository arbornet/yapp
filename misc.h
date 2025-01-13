#pragma once

int cluster(int argc, char **argv);
int date(int argc, char **argv);
int echo(int argc, char **argv);
int eval(int argc, char **argv);
int mail(int argc, char **argv);
int cd(int argc, char **argv);
int do_source(int argc, char **argv);
int test(int argc, char **argv);
int do_umask(int argc, char **argv);
#ifdef INCLUDE_EXTRA_COMMANDS
int set_cfdir(int argc, char **argv);
#endif
int set_debug(int argc, char **argv);
char misc_cmd_dispatch(int argc, char **argv);
int do_if(int argc, char **argv);
int do_else(int argc, char **argv);
int do_endif(int argc, char **argv);
int do_rm(int argc, char **argv);
int argset(int argc, char **argv);
int foreach (int argc, char **argv);
int test_if(void);
int load_values(int argc, char **argv);
int save_values(int argc, char **argv);
