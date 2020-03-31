char * get_informix PROTO(( char* key_field, char* key_val, 
 char* return_field, int suid));

int informix_insert PROTO(( char *login, char *passwd, char *full_name,
 char *email, int   suid));

int informix_update PROTO(( char * login, char * field, char * value, 
 int suid));
