#pragma once

#include <string>

#define NUM_RIGHTS 4
#define JOIN_RIGHT 0    /* r */
#define RESPOND_RIGHT 1 /* w */
#define ENTER_RIGHT 2   /* c */
#define CHACL_RIGHT 3   /* a */

bool check_acl(int right, int idx);
void load_acl(int idx);
void reinit_acl(void);
bool is_auto_ulist(int idx);

extern std::string acl_list[NUM_RIGHTS];
