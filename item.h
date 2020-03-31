/* ITEM.H: @(#)item.h 1.4 94/01/20 (c)1993 thalerd */
#include <sys/types.h>

#define RF_NORMAL    0x0000
#define RF_CENSORED  0x0001
#define RF_SCRIBBLED 0x0002
#define RF_EXPIRED   0x0004

int  do_enter    PROTO((sumentry_t *this, char *sub, char **text, SHORT idx,
                        sumentry_t *sum, partentry_t *part, status_t *stt,
			               long art, char *mid, int uid, char *login, 
								char *fullname));
int  do_find     PROTO((int argc, char **argv));
int  do_kill     PROTO((int argc, char **argv));
int  do_read     PROTO((int argc, char **argv));
int  enter       PROTO((int argc, char **argv));
int  fixseen     PROTO((int argc, char **argv));
int  fixto       PROTO((int argc, char **argv));
int  forget      PROTO((int argc, char **argv));
int  freeze      PROTO((int argc, char **argv));
int  linkfrom    PROTO((int argc, char **argv));
int  remember    PROTO((int argc, char **argv));
void show_header PROTO(());
void show_resp   PROTO((FILE *fp));
void show_range  PROTO(());
void show_nsep   PROTO((FILE *fp));
int  nextitem    PROTO((int inc));
int  is_enterer  PROTO((int item));
