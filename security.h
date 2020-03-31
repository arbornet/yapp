/* $Id: security.h,v 1.1 1996/09/23 14:52:30 thaler Exp $ */
#define NUM_RIGHTS    4
#define JOIN_RIGHT    0  /* r */
#define RESPOND_RIGHT 1  /* w */
#define ENTER_RIGHT   2  /* c */
#define CHACL_RIGHT   3  /* a */

int check_acl PROTO((int right, int idx));
void load_acl PROTO((int idx));
void reinit_acl();
int is_auto_ulist PROTO((int idx));

extern char acl_list[NUM_RIGHTS][MAX_LINE_LENGTH];
