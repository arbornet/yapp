/* EDBUF.H: @(#)edbuf.h 1.5 93/06/07 Copyright (c)1993 thalerd */
int  text_read  PROTO((int argc, char **argv));
int  text_write PROTO((int argc, char **argv));
int  text_edit  PROTO((int argc, char **argv));
int  text_print PROTO((int argc, char **argv));
int  text_clear PROTO((int argc, char **argv));
int  text_abort PROTO((int argc, char **argv));
int  text_done  PROTO((int argc, char **argv));
char text_cmd_dispatch PROTO((int argc, char **argv));
char text_loop PROTO((int new, char *label));
char edb_cmd_dispatch PROTO((int argc, char **argv));
