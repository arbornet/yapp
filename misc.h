/* MISC.H: @(#)misc.h 1.3 93/05/20 Copyright (c)1993 thalerd */

int  cluster PROTO((int argc, char **argv));
int  date PROTO((int argc, char **argv));
int  echo PROTO((int argc, char **argv));
int  eval PROTO((int argc, char **argv));
int  mail PROTO((int argc, char **argv));
int  cd   PROTO((int argc, char **argv));
int  do_source PROTO((int argc, char **argv));
int  test PROTO((int argc, char **argv));
int  do_umask PROTO((int argc, char **argv));
#ifdef INCLUDE_EXTRA_COMMANDS
int  set_cfdir PROTO((int argc, char **argv));
#endif
int  set_debug PROTO((int argc, char **argv));
char misc_cmd_dispatch PROTO((int argc, char **argv));
int  do_if PROTO((int argc, char **argv));
int  do_else PROTO((int argc, char **argv));
int  do_endif PROTO((int argc, char **argv));
int  do_rm PROTO((int argc, char **argv));
int  argset PROTO((int argc, char **argv));
int  foreach PROTO((int argc, char **argv));
int  test_if PROTO(());
int  load_values PROTO((int argc, char **argv));
int  save_values PROTO((int argc, char **argv));
