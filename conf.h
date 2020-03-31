/* CONF.H: @(#)conf.h 1.2 94/01/20 (c)1993 thalerd */
int   check        PROTO((int argc, char **argv));
int   show_conf_index PROTO((int argc, char **argv));
char  checkpoint   PROTO((SHORT idx, unsigned int sec, int silent));
int   do_next      PROTO((int argc, char **argv));
char  join         PROTO((char *conf, int observe, int secure));
int   leave        PROTO((int argc, char **argv));
int   participants PROTO((int argc, char **argv));
int   describe     PROTO((int argc, char **argv));
int   resign       PROTO((int argc, char **argv));
void  log          PROTO((SHORT idx, char *str));
char *get_desc     PROTO((char *name));
char *nextconf();
char *nextnewconf();  /* Next conference with new items in it */
char *prevconf();
unsigned int   security_type PROTO((char **config, SHORT idx));
int is_inlistfile PROTO((int idx, char *file));
int is_fairwitness PROTO((int idx));
int check_password PROTO((int idx));
int is_validorigin PROTO((int idx));
char *fullname_in_conference PROTO((status_t *stt));
char **grab_recursive_list PROTO((char *dir, char *filename));
