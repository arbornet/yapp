/* SYSOP.H */

int is_sysop PROTO(());
int cfcreate PROTO((int argc, char **argv));
int cfdelete PROTO((int argc, char **argv));
int upd_maillist PROTO((short security, char ** config, int idx));
