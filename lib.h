/* LIB.H: @(#)lib.h 1.16 93/06/07 Copyright (c)1993 thalerd */

/* Grab file options */
#define GF_SILENT         0x0001
#define GF_WORD           0x0002
#define GF_HEADER         0x0004
#define GF_IGNCMT         0x0008  /* ignore comment lines? */
#define GF_NOHEADER       0x0010  /* don't require file header */

char match PROTO((char *a, char *b));
char cat PROTO((char *dir, char *file));
char **grab_file PROTO((char *dir, char *file, CHAR silent));
char **grab_more PROTO((FILE *fp, char *end, CHAR silent, int *endlen));
short searcha PROTO((char *elt, char **arr, SHORT start));
char **explode PROTO((char *str, char *sep, int mult));
void implode PROTO((char *buff, char **arr, char *sep, SHORT start));
char *get_password PROTO(());
char *ngets PROTO((char *str, FILE *fp));
char *xgets PROTO((FILE *fp, int lvl));
assoc_t *grab_list PROTO((char *dir,char *file, int flags));
int  get_idx      PROTO((char *elt, assoc_t *list, int size));
void free_list PROTO((assoc_t *list));
int  get_yes PROTO((char *pr, int err));
char *get_date PROTO((time_t t, int sty));
#ifndef HAVE_STRNCASECMP
int strncasecmp PROTO((char *s1, char *s2, int n));
#endif
char *compress PROTO((char *str));
char *noquote  PROTO((char *str,int x));
void error PROTO((char *str1,char *str2));
char *lower_case PROTO((char *str));
char write_file PROTO((char *file, char *buff));
char *mystrtok PROTO((char *str, char *sep));
char more PROTO((char *dir, char *filename));
