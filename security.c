/* 
 * Implements conference security with
 *  [rwca] [+-][all f:ulist f:observers fwlist originlist password sysop]
 *
 * $Id: security.c,v 1.1 1996/09/23 14:52:32 thaler Exp $
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "yapp.h"
#include "struct.h"
#include "globals.h" /* for login */
#include "lib.h"     /* for explode */
#include "xalloc.h"  /* for xsizeof */
#include "conf.h"    /* for security_type */
#include "stats.h"   /* for get_config */
#include "security.h"
static int    acl_idx  = -1;

char acl_list[NUM_RIGHTS][MAX_LINE_LENGTH];
static char *rightstr="rwca";

void
reinit_acl()
{
   acl_idx = -1;
}

void
load_acl(idx)
   int idx;
{
   char **line, *ptr, *q;
   int i;

   if (idx==acl_idx)
      return;
   
   for (i=0; i<NUM_RIGHTS; i++)
      acl_list[i][0] = '\0';

   line = grab_file( conflist[idx].location, "acl", GF_IGNCMT|GF_SILENT);
   for (i=0; i<xsizeof(line); i++) {
      for (ptr = line[i]; isspace(*ptr); ptr++);
      q = strchr(ptr, ' ');
      while (ptr<q) {
         char *r = strchr(rightstr, *ptr++);
         if (r) 
            strcpy(acl_list[r-rightstr], q+1);
      }
   }
   xfree_array(line);

   /*
    * If we hit eof without finding any relevant lines, revert to 
    * conference security type default 
    */
   if (!acl_list[JOIN_RIGHT][0] || !acl_list[RESPOND_RIGHT][0] 
    || !acl_list[ENTER_RIGHT][0]) {
      char base[MAX_LINE_LENGTH];
      int sec = security_type(get_config(idx), idx);

      strcpy(base, "+registered");
      if ((sec & CT_BASIC)==CT_PRESELECT
       || (sec & CT_BASIC)==CT_PARANOID)
         strcat(base, " +f:ulist");
      if (sec & CT_ORIGINLIST)
         strcat(base, " +originlist");

      if (!acl_list[RESPOND_RIGHT][0]) {
         strcpy(acl_list[RESPOND_RIGHT], base);
         strcat(acl_list[RESPOND_RIGHT], " -f:observers");
      }
      if (!acl_list[JOIN_RIGHT][0]) {
         if (sec & CT_READONLY)
            strcpy(acl_list[JOIN_RIGHT], "+all");
         else
            strcpy(acl_list[JOIN_RIGHT], base);
         if ((sec & CT_BASIC)==CT_PASSWORD
          || (sec & CT_BASIC)==CT_PARANOID)
            strcat(acl_list[JOIN_RIGHT], " +password");
      } 
      if (!acl_list[ENTER_RIGHT][0]) {
         strcpy(acl_list[ENTER_RIGHT], acl_list[RESPOND_RIGHT]);
         if (sec & CT_NOENTER)
            strcat(acl_list[ENTER_RIGHT], " +fwlist");
      }
   }
   if (!acl_list[CHACL_RIGHT][0])
      strcpy(acl_list[CHACL_RIGHT], "+sysop");

   acl_idx  = idx;
}

static int            /* RETURNS: 1 if passes criteria, 0 if not */
check_field(str, idx) /* ARGUMENTS:                              */
   char *str;         /*    IN: criteria to test                 */
   int   idx;         /*    IN: conference index                 */
{
   int not=0;
   int ok = 0;
   char uidstr[10];

   sprintf(uidstr,"%d",uid);

   if (str[0]=='-') {
      not=1;
      str++;
   } else if (str[0]=='+') {
      str++;
   }

   if (tolower(str[0])=='f' && str[1]==':')
      ok = is_inlistfile(idx, str+2);
   else if (!strcmp(str, "fwlist")) 
      ok = is_fairwitness(idx);
   else if (!strcmp(str, "all"))
      ok = 1;
   else if (!strcmp(str, "registered"))
      ok = !(status & S_NOAUTH);
   else if (!strcmp(str, "password"))
      ok = check_password(idx);
   else if (!strcmp(str, "originlist"))
      ok = is_validorigin(idx);
   else if (!strcmp(str, "sysop")) 
      ok = is_sysop(1);

   if (not) ok = !ok;
   return ok;
}

int                   /* RETURNS: 1 if passwd, 0 if failed */
check_acl(right, idx) /* ARGUMENTS:                        */
   int  right;        /*    IN: Right to check (r/w/c)     */
   int  idx;          /*    IN: Conference index           */
{
   char **field;
   int    i, ok;

   load_acl(idx);
   field = explode(acl_list[right], " ", 1);

   /* Ok to 1 if user fits every field, and 1 if user fails any field.  */
   for (ok=1, i=0; ok && i<xsizeof(field); i++)
      ok = check_field(field[i], idx);

   xfree_array(field);
   return ok;
}

/******************************************************************************/
/* TEST TO SEE IF ULIST SHOULD BE UPDATED WITH NEW JOINERS                    */
/******************************************************************************/
int                /* RETURNS: 1 if ulist maintance is automatic, 0 if manual */
is_auto_ulist(idx) /* ARGUMENTS:             */
   int idx;        /*   IN: conference index */
{
   int i;

#if 0
   return (!(sec & CT_READONLY)
          && (sec & CT_BASIC)!=CT_PRESELECT
          && (sec & CT_BASIC)!=CT_PARANOID);
#endif

   /* True if and only if the ulist file is not mentioned in any acls */
   load_acl(idx);
   for (i=0; i<NUM_RIGHTS; i++)
      if (strstr(acl_list[i], "ulist"))
         return 0;
   return 1;
}
