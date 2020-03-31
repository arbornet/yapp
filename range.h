/* RANGE.H: @(#)range.h 1.2 94/01/22 (c)1993 thalerd */

/* Option flags */
#define OF_NEWRESP    0x0001
#define OF_REVERSE    0x0002
#define OF_UID        0x0004
#define OF_NUMBERED   0x0008
#define OF_DATE       0x0010
#define OF_PASS       0x0020
#define OF_NOFORGET   0x0040
#define OF_RANGE      0x0080
#define OF_BRANDNEW   0x0100
#define OF_FORMFEED   0x0200
#define OF_UNSEEN     0x0400
#define OF_FORGOTTEN  0x0800
#define OF_RETIRED    0x1000
#define OF_SHORT      0x2000
#define OF_NORESPONSE 0x4000
#define OF_EXPIRED    0x8000
# define OF_NEXT      0x10000
# define OF_NONE      0x20000

/* Action values */
#define A_SKIP  0 /* Don't do item */
#define A_COVER 1 /* Do item based on option flags */
#define A_FORCE 2 /* Always do item */

char  cover      PROTO((SHORT i, SHORT idx, SHORT spec, SHORT act, 
                  sumentry_t *sum, partentry_t *part, status_t *st));
void  range      PROTO((int argc, char **argv, short *flags, char *act, 
                  sumentry_t *sum, status_t *st, int bef));
void  rangetoken PROTO((char *token, short *flags, char *act, sumentry_t *sum,
                  status_t  *st));
time_t since     PROTO((int argc, char **argv, short *ip));
void  rangeinit();
int   is_newresp  PROTO((partentry_t *p, sumentry_t *s));
int   is_brandnew PROTO((partentry_t *p, sumentry_t *s));
