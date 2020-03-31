/* MACRO.H: @(#)macro.h 1.16 93/06/07 Copyright (c)1993 thalerd */
/* Define macro mask */
#define DM_OK             0x0001
#define DM_VAR            0x0002
#define DM_PARAM          0x0004
#define DM_RFP            0x0008
#define DM_SUPERSANE      0x0040
#define DM_SANE           0x0080
#define DM_ENVAR          0x0100
#define DM_BASIC          0x7FFF
#define DM_CONSTANT       0x8000

void undef_macro PROTO((macro_t *prev));
void undef_name  PROTO((char *name));
char *expand PROTO((char *mac, U_SHORT mask));
char *capexpand PROTO((char *mac, U_SHORT mask, int cap));
int  define PROTO((int argc, char **argv));
void def_macro PROTO((char *name, int mask, char *value));
macro_t *find_macro PROTO((char *name, unsigned short mask));
void undefine PROTO((unsigned short mask));
char *conference PROTO((int cap));
char *fairwitness PROTO((int cap));
char *topic PROTO((int cap));
char *subject PROTO((int cap));
