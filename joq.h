/* JOQ.H: @(#)joq.h 1.5 93/06/07 Copyright (c)1993 thalerd */
char joq_cmd_dispatch PROTO((int argc, char **argv));
void write_part PROTO((char *partfile));
char read_part PROTO((char *partfile, partentry_t part[], status_t *st, 
                      SHORT idx));
