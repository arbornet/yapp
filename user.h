#ifdef INCLUDE_EXTRA_COMMANDS
char *get_ticket PROTO((int tm, char *who));
int authenticate PROTO((int argc, char **argv));
#endif
void refresh_list  PROTO(());
int do_cflist PROTO((int argc, char **argv));
char *make_ticket PROTO((char *key, int tm));
int passwd PROTO((int argc, char **argv));
int chfn PROTO((int argc, char **argv));
int newuser PROTO((int argc, char **argv));
char *get_sysop_login PROTO(());
int get_nobody_uid PROTO(());
int get_user PROTO((int *uid, char *login, char *fullname, char *home, char *email));
void login_user PROTO(());
int  del_cflist PROTO((char *cfname));
void show_cflist PROTO(());
char *email2login PROTO((char *email));
int partfile_perm();
void get_partdir PROTO((char *partdir, char *login));
char *get_userfile PROTO((char *who, int *suid));
