/* SUM.H: @(#)sum.h 1.2 94/01/20 (c)1993 thalerd */
void check_mail    PROTO((int f));
void get_status    PROTO((status_t *st, sumentry_t *sum, 
                       partentry_t *part, SHORT idx));
int  item_sum      PROTO((SHORT i, sumentry_t *sum, 
                       partentry_t *part, SHORT idx, status_t *stt));
void load_sum      PROTO((sumentry_t *sum, 
                     partentry_t *part, status_t *stt, SHORT idx));
void refresh_link  PROTO((status_t *stt,sumentry_t *sum, 
                          partentry_t *part,SHORT idx,SHORT i));
void refresh_stats PROTO((sumentry_t *sum, partentry_t *part, status_t *st));
void refresh_sum   PROTO((SHORT item, SHORT idx, 
                       sumentry_t *sum, partentry_t *part, status_t *st));
void save_sum      PROTO((sumentry_t *sum, SHORT i, SHORT idx, status_t *stt));
int32 get_hash     PROTO((char *str));
void dirty_part    PROTO((int i));
void dirty_sum     PROTO((int i));
