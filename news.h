/* NEWS.H: %Z%%M% %I% %E% (c)1993 thalerd */

/* EMAIL: */
int   incorporate     PROTO(( long i, sumentry_t  *sum, 
                          partentry_t *part, status_t *stt, SHORT idx));
int   make_emhead     PROTO((response_t *re, SHORT par));
int   make_emtail     PROTO(());

#ifdef NEWS
char *dot2slash       PROTO((char *str));
int   make_rnhead     PROTO((response_t *re, SHORT par));
char *message_id      PROTO((char *conf, SHORT itm, SHORT rsp, response_t *re));
void  get_article     PROTO((response_t *re));
void  refresh_news    PROTO((sumentry_t *sum, partentry_t *part, status_t *stt,
			                 SHORT idx));
#endif
