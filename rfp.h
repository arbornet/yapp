/* RFP.H: @(#)rfp.h 1.3 94/01/22 (c)1993 thalerd */

void  add_response     PROTO((sumentry_t *this, char **text,
                          SHORT idx, sumentry_t *sum, partentry_t *part, 
                          status_t *stt, long art, char *mid, int uid, 
                          char *login, char *fullname, SHORT resp));
int   censor           PROTO((int argc, char **argv));
int   uncensor         PROTO((int argc, char **argv));
int   preserve         PROTO((int argc, char **argv));
int   reply            PROTO((int argc, char **argv));
int   respond          PROTO((int argc, char **argv));
int   rfp_respond      PROTO((int argc, char **argv));
char  rfp_cmd_dispatch PROTO((int argc, char **argv));
#ifdef INCLUDE_EXTRA_COMMANDS
int   tree             PROTO((int argc, char **argv));
short sibling          PROTO((SHORT r));
short parent           PROTO((SHORT r));
short child            PROTO((SHORT r));
#endif
void  dump_reply       PROTO((char *sep));
