/* STATS.H: %Z%%M% %I% %E% (c)1993 thalerd */
char  *get_subj     PROTO((SHORT idx, SHORT item, sumentry_t *sum));
char  *get_auth     PROTO((SHORT idx, SHORT item, sumentry_t *sum));
char **get_config   PROTO((SHORT idx));
void   clear_cache  PROTO(());
void   store_subj   PROTO((SHORT idx, SHORT item, char *str));
void   store_auth   PROTO((SHORT idx, SHORT item, char *str));
