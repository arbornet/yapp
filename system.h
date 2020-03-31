/* SYSTEM.H: @(#)system.h 1.5 93/05/01 Copyright (c)1993 thalerd */
int unix_cmd PROTO((char *cmd));
FILE *spopen PROTO((char *cmd));
FILE *smopenw PROTO((char *file, long flg));
FILE *smopenr PROTO((char *file, long flg));
int smclose PROTO((FILE *fp));
int spclose PROTO((FILE *fp));
int edit PROTO((char *dir, char *file, int visual));
int priv_edit PROTO((char *dir, char *file, int visual));
int rm PROTO((char *file, int sec));
int copy_file PROTO((char *src, char *dest, int sec));
int move_file PROTO((char *src, char *dest, int sec));

/* Security levels */
#define SL_OWNER 0
#define SL_USER  1

#ifdef HAVE_DBM_OPEN
int save_dbm PROTO((char *userfile, char *keystr, char *valstr, int suid));
char *get_dbm PROTO((char *userfile, char *keystr, int suid));
void dump_dbm PROTO((char *file));
#endif
