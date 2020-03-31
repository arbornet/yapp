/* SEP.H: @(#)sep.h 1.5 93/06/07 Copyright (c)1993 thalerd */

/* Once flags */
#define IS_START     0x0001
#define IS_ITEM      0x0002
#define IS_CFIDX     0x0004 /* special flag used for k cond in checkmsg */
#define IS_RESP      0x0400

#define IS_ALL       0x0BF8 /* all below */
#define IS_RETIRED   0x0008 /* used to check "if retired stuff" */
#define IS_FORGOTTEN 0x0010
#define IS_FROZEN    0x0020
#define IS_LINKED    0x0040
#define IS_CENSORED  0x0080
#define IS_UID       0x0100
#define IS_DATE      0x0200
#define IS_PARENT    0x0800


void confsep PROTO((char *str, SHORT idx, status_t *st, partentry_t *part, int fl));
void itemsep PROTO((char *str, int fl));
void fitemsep PROTO((FILE *fp, char *str, int fl));
void sepinit PROTO((SHORT x));
void skip_new_response PROTO((SHORT c, SHORT i, SHORT nr));
char *get_sep PROTO((char **pEptr));
void init_show PROTO(());
char itemcond PROTO((char **spp, long fl));
char confcond PROTO((char **spp, SHORT idx, status_t *st));

