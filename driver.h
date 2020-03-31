/* DRIVER.H: @(#)driver.h 1.16 93/06/07 Copyright (c)1993 thalerd */

void endbbs PROTO(());
void init PROTO(());
char source PROTO((char *dir,char *file, int fl, int sec));
char command PROTO((char *cmd, int lvl));
void print_prompt PROTO((U_CHAR mode));
char ok_cmd_dispatch PROTO((int argc, char **argv));
char get_command PROTO((char *deflt, int lvl));
void handle_int PROTO(());
void handle_pipe PROTO(());
void handle_other PROTO((int sig, int code, void *scp, char *addr));
void open_cluster PROTO((char *bdir, char *hdir));
void open_pipe PROTO(());
void ints_on PROTO(());
void ints_off PROTO(());
void push_stdin PROTO((FILE *fp, int type));
void pop_stdin();
