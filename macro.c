/* $Id: macro.c,v 1.29 1998/02/10 11:36:15 kaylene Exp $ */

/* Message-Id: <%14D.%{uid}@%{hostname}>\n */
#define MAILHEADER     "\
Date: %15D\n\
From: \"%{fullname}\" <%{email}>\n\
Subject: %(pRe: %)%h\n\
Message-Id: <%14D.%{uid}@%{hostname}>\n\
To: %{address}\n\
%(rIn-Reply-To: <%m> from \"%a\" at %d\n%)"
#define MAILSEP        "\
%(1x%a writes:%)\
%(2x> %L%)\
%(4x%)"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for getpid */
#endif
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "macro.h"
#include "lib.h"     /* for ultrix where strdup() is defined */
#include "xalloc.h"
#include "stats.h"   /* for get_config() */
#include "conf.h"    /* for nextconf(), prevconf() , nextnewconf*/
#include "user.h"
#include "license.h"
#include "driver.h"  /* for open_pipe */
#include "security.h" /* for load_acl */

static macro_t *first_macro=NULL;
static char *rovars[]={ /* list of READ-ONLY variables */
"fullname", "login", "uid", "work", "home", "hostname", "address",
"mailheader", "pathinfo", "requestmethod", "pid", "exit", "version",
"status", "mode", "lowresp", "fwlist", "remotehost", "querystring",
"confname", "bbsdir", "wwwdir", "sysop", "cflist", "conflist", 
"fromlogin", "cgidir", "euid", "nobody", "partdir", "remoteaddr",
"racl", "wacl", "cacl", "aacl", "cursubj", "hitstoday", "newresps", "confdir",
"isnew", "isbrandnew", "isnewresp","canracl","canwacl","cancacl","canaacl",
"userfile", "verifyemail", "userdbm",
#ifdef INCLUDE_EXTRA_COMMANDS
"ticket",
#endif
NULL
};

char *
itostr(i)
   short i;
{
static char buff[20];

   sprintf(buff,"%d",i);
   return buff;
}

char *
ixtostr(i)
   int i;
{
static char buff[20];

   sprintf(buff,"0x%x",i);
   return buff;
}

macro_t *
find_macro(name,mask)
   char *name;
   unsigned short mask;
{
   macro_t *curr = first_macro;

   while (curr && (!(mask & curr->mask) || !match(name, curr->name)))
      curr=curr->next;
   return curr;
}

/******************************************************************************/
/* EXPAND A MACRO                                                             */
/* UNK Could put hashing in later to speed things up if needed                */
/******************************************************************************/
char *                  /* RETURNS: string macro expands to */
expand(mac,mask)        /* ARGUMENTS:                       */
   char          *mac;  /*    Macro name to expand          */
   unsigned short mask; /*    Type of macro (see macro.h)   */
{
   char **config;
   macro_t *curr;

   mask &= DM_BASIC; /* drop DM_CONSTANT bit */
   curr = find_macro(mac, mask);

   if (!curr) {
      /* A few default values */
      if (debug & DB_MACRO)
         printf("expand: '%s' %hu\n",mac, mask);
      if (mask & DM_VAR) {
         switch(tolower(mac[0])) {
         case 'a':
#ifdef WWW
            if (match(mac,"aacl")) {
               load_acl(confidx);
               return acl_list[CHACL_RIGHT];
            }
#endif
            if (match(mac,"alpha"))     {
               if (getenv("ALPHA")) return getenv("ALPHA");
               return "";
            }
            if (match(mac,"address"))   {
               if ((config = get_config(confidx)) && xsizeof(config)>CF_EMAIL)
                  return config[CF_EMAIL];
            }
            break; /* 'a' */

         case 'b':
            if (match(mac,"beta"))     {
               if (getenv("BETA")) return getenv("BETA");
               return "";
            }
            if (match(mac,"bufdel"))    return BUFDEL;
            if (match(mac,"bullmsg"))   return BULLMSG;
            if (match(mac,"brandnew"))  return itostr(st_glob.i_brandnew);
            if (match(mac,"bbsdir"))    return get_conf_param("bbsdir", BBSDIR);
            break; /* 'b' */

         case 'c':
#ifdef WWW
            if (match(mac,"cgidir")) {
               static char buff[MAX_LINE_LENGTH];
               char *str = getenv("SCRIPT_NAME");
               char **dirs;
               if (!str) str=".";
               dirs = explode(str, "/", 1);
               strcpy(buff, dirs[0]);
               xfree_array(dirs);
               return buff;
            }
            if (match(mac,"cacl")) {
               load_acl(confidx);
               return acl_list[ENTER_RIGHT];
            }
            if (match(mac,"canaacl"))   
               return itostr(check_acl(CHACL_RIGHT,confidx));
            if (match(mac,"canracl"))   
               return itostr(check_acl(JOIN_RIGHT,confidx));
            if (match(mac,"canwacl"))   
               return itostr(check_acl(RESPOND_RIGHT,confidx));
            if (match(mac,"cancacl"))   
               return itostr(check_acl(ENTER_RIGHT,confidx));
#endif
            if (match(mac,"conference"))return CONFERENCE;
            if (match(mac,"cmddel"))    return CMDDEL;
            if (match(mac,"censored"))  return CENSORED;
            if (match(mac,"checkmsg"))  return CHECKMSG;
            if (match(mac,"confindexmsg"))  return CONFINDEXMSG;
            if (match(mac,"confmsg"))   return CONFMSG;
            if (match(mac,"curitem"))   return itostr(st_glob.i_current);
            if (match(mac,"curresp"))   return itostr(st_glob.r_current);
            if (match(mac,"curline"))   return itostr(st_glob.l_current);
            if (match(mac,"cfadm"))     return get_conf_param("cfadm",CFADM);
            if (match(mac,"cflist"))    return cfliststr;
            if (match(mac,"cursubj"))
               return (confidx>=0 && st_glob.i_current<=st_glob.i_last)
                ? get_subj(confidx,st_glob.i_current-1,sum) : "";
            if (match(mac,"confname"))  
               return (confidx>=0)? compress(conflist[confidx].name) : "noconf";
            if (match(mac,"confdir"))
               return (confidx>=0)? conflist[confidx].location : "noconf";
            if (match(mac,"conflist"))   {
               static char buff[2048];
               int i;
               char *p = buff+1;
   
               strcpy(buff, " ");
               for (i=1; i<maxconf; i++) {
                  strcat(p, compress(conflist[i].name));
                  p += strlen(p);
                  strcat(p, " ");
                  p++;
               }
               return buff;
            }
            break; /* 'c' */

         case 'd':
            if (match(mac,"delta"))     {
               if (getenv("DELTA")) return getenv("DELTA");
               return "";
            }
            break;

         case 'e':
#ifdef WWW
            if (match(mac,"exit")) {
               extern int exit_status;
               return itostr(exit_status);
            }
#endif
            if (match(mac,"editor"))     {
               if (getenv("EDITOR")) return getenv("EDITOR");
               return EDITOR;
            }
            if (match(mac,"edbprompt")) return EDBPROMPT;
            if (match(mac,"escape"))    return ESCAPE;
            if (match(mac,"email"))     return email;
            if (match(mac,"euid"))      return itostr(geteuid());
            break; /* 'e' */

         case 'f':
            if (match(mac,"fairwitness")) return FAIRWITNESS;
            if (match(mac,"fsep"))      return FSEP;
            if (match(mac,"firstitem")) return itostr(st_glob.i_first);
            if (match(mac,"fromlogin")) return re[st_glob.r_current].login;
            if (match(mac,"fullname"))  return fullname_in_conference(&st_glob);
            if (match(mac,"fwlist"))   {
               if ((config = get_config(confidx)) && xsizeof(config)>CF_FWLIST)
                  return config[CF_FWLIST];
            }
            break; /* 'f' */

         case 'g':
            if (match(mac,"gamma"))     {
               if (getenv("GAMMA")) return getenv("GAMMA");
               return "";
            }
            if (match(mac,"gecos"))     return GECOS;
            if (match(mac,"groupindexmsg"))  return GROUPINDEXMSG;
            break; /* 'g' */

         case 'h':
            if (match(mac,"hitstoday")) return itostr(get_hits_today());
            if (match(mac,"highresp"))  return itostr(st_glob.r_last);
            if (match(mac,"home"))      return home;
            if (match(mac,"hostname"))  return hostname;
            break; /* 'h' */

         case 'i':
            if (match(mac,"item"))      return ITEM;
            if (match(mac,"ishort"))    return ISHORT;
            if (match(mac,"isep"))      return ISEP;
            if (match(mac,"indxmsg"))   return INDXMSG;
            if (match(mac,"isbrandnew")) return itostr(is_brandnew(
             &part[st_glob.i_current-1], &sum[st_glob.i_current-1]));
            if (match(mac,"isnewresp")) return itostr(is_newresp(
             &part[st_glob.i_current-1], &sum[st_glob.i_current-1]));
            if (match(mac,"isnew"))     return itostr(is_newresp(
             &part[st_glob.i_current-1], &sum[st_glob.i_current-1]) || 
             is_brandnew(&part[st_glob.i_current-1], &sum[st_glob.i_current-1]));
            break; /* 'i' */

         case 'j':
            if (match(mac,"joqprompt")) return JOQPROMPT;
            if (match(mac,"joinmsg"))   return JOINMSG;
            break; /* 'j' */

         case 'k':
            break;

         case 'l':
            if (match(mac,"linmsg"))    return LINMSG;
            if (match(mac,"loutmsg"))   return LOUTMSG;
            if (match(mac,"listmsg"))   return LISTMSG;
            if (match(mac,"lastitem"))  return itostr(st_glob.i_last);
            if (match(mac,"lowresp"))   return itostr(st_glob.r_first);
            if (match(mac,"lastresp"))  
               return itostr(sum[st_glob.i_current-1].nr - 1);
               /* return itostr(st_glob.r_max); */
            if (match(mac,"login"))     return login;
            break; /* 'l' */

         case 'm':
            if (match(mac,"mailmsg"))   return MAILMSG;
            if (match(mac,"mailsep"))   return MAILSEP;
            if (match(mac,"mailheader"))   return MAILHEADER;
            if (match(mac,"mode"))      return itostr((int)mode);
            break; /* 'm' */

         case 'n':
            if (match(mac,"noconfp"))   return NOCONFP;
            if (match(mac,"nsep"))      return NSEP;
#ifdef NEWS
            if (match(mac,"newssep"))   return NEWSSEP;
#endif
            if (match(mac,"nextconf"))  return nextconf();
            if (match(mac,"nextnewconf"))  return nextnewconf();
            if (match(mac,"nextitem"))  return itostr(st_glob.i_next);
            if (match(mac,"newresp"))   return itostr(st_glob.i_newresp);
            if (match(mac,"numitems"))  return itostr(st_glob.i_numitems);
            if (match(mac,"newresps"))  
               return itostr(sum[st_glob.i_current-1].nr - abs(part[st_glob.i_current-1].nr));
            if (match(mac,"nobody"))    return get_conf_param("nobody",NOBODY);
            break; /* 'n' */

         case 'o':
            if (match(mac,"obvprompt")) return OBVPROMPT;
            break;

         case 'p':
#ifdef WWW
            if (match(mac,"pathinfo")) {
               char *str = getenv("PATH_INFO");
               return (str && str[0])? str+1 : "";
            }
            if (match(mac,"pid"))    return itostr(getpid());
#endif
            if (match(mac,"printmsg"))  return PRINTMSG; 
            if (match(mac,"partmsg"))   return PARTMSG;
            if (match(mac,"prevconf"))  return prevconf();
            if (match(mac,"previtem"))  return itostr(st_glob.i_prev);
            if (match(mac,"partdir"))   return partdir;
            break; /* 'p' */

         case 'q':
#ifdef WWW
            if (match(mac,"querystring")) {
               if (getenv("QUERY_STRING")) return getenv("QUERY_STRING");
               return "";
            }
#endif
/* This should not be there, since "define pager" should turn
 * off paging.
            if (match(mac,"pager"))     return PAGER;
 */
            if (match(mac,"prompt"))    return PROMPT;
            break;

         case 'r':
#ifdef WWW
            if (match(mac,"requestmethod")) {
               if (getenv("REQUEST_METHOD")) return getenv("REQUEST_METHOD");
               return "";
            }
            if (match(mac,"remotehost")) {
               if (getuid()!=get_nobody_uid()) 
                  return hostname;  /* localhost */
               if (getenv("REMOTE_HOST")) return getenv("REMOTE_HOST");
               return "";
            }
            if (match(mac,"remoteaddr")) {
               if (getuid()!=get_nobody_uid())
               return "127.0.0.1"; /* localaddr */
               if (getenv("REMOTE_ADDR")) return getenv("REMOTE_ADDR");
               return "";
            }
            if (match(mac,"racl")) {
               load_acl(confidx);
               return acl_list[JOIN_RIGHT];
            }
#endif
            if (match(mac,"rfpprompt")) return RFPPROMPT;
            if (match(mac,"rsep"))      return RSEP;
            if (match(mac,"replysep"))  return REPLYSEP;
            break; /* 'r' */

         case 's':
            if (match(mac,"subject"))   return SUBJECT;
            if (match(mac,"scribbled")) return SCRIBBLED;
            if (match(mac,"scribok"))   return SCRIBOK;
            if (match(mac,"shell"))     {
               if (getenv("SHELL")) return getenv("SHELL");
               return SHELL;
            }
            if (match(mac,"status"))    return ixtostr((int)status);
            if (match(mac,"seenresp"))  
               return itostr(abs(part[st_glob.i_current-1].nr));
               /* return itostr(st_glob.r_lastseen); */
            if (match(mac,"sysop"))     return get_sysop_login();
            break; /* 's' */

         case 't':
#ifdef WWW
#ifdef INCLUDE_EXTRA_COMMANDS
            if (match(mac,"ticket")) return get_ticket(0, login);
#endif /* INCLUDE_EXTRA_COMMANDS */
#endif /* WWW */
            if (match(mac,"text"))      return TEXT;
            if (match(mac,"txtsep"))    return TXTSEP;
            if (match(mac,"totalnewresp")) return itostr(st_glob.r_totalnewresp);
            break; /* 't' */

         case 'u':
            if (match(mac,"unseen"))    return itostr(st_glob.i_unseen);
            if (match(mac,"userdbm")) 
               return get_conf_param("userdbm",USERDBM);
            if (match(mac,"uid"))       return itostr(uid);
            if (match(mac,"userfile"))  {
               int suid;
               return get_userfile(login, &suid);
            }
            break; /* 'u' */

         case 'v':
            if (match(mac,"visual"))     { 
               if (getenv("VISUAL")) return getenv("VISUAL");
               return expand("editor",DM_VAR);
            }
            if (match(mac,"verifyemail")) 
               return get_conf_param("verifyemail",VERIFY_EMAIL);
            if (match(mac,"version"))   return VERSION;
            break; /* 'v' */

         case 'w':
#ifdef WWW
            if (match(mac,"wacl")) {
               load_acl(confidx);
               return acl_list[RESPOND_RIGHT];
            }
#endif
            if (match(mac,"wellmsg"))   return WELLMSG;
            if (match(mac,"work"))      return work;
            if (match(mac,"wwwdir"))    {
               static char wwwdef[256];
               sprintf(wwwdef, "%s/www", get_conf_param("bbsdir", BBSDIR));
               return get_conf_param("wwwdir", wwwdef);
            }
            break; /* 'w' */

         case 'x':
            break;
         case 'y':
            break;

         case 'z':
            if (match(mac,"zsep"))      return ZSEP;
            break;

         default :
            break;
         }

      }
      return 0;
   }

   return curr->value;
}

char *
capexpand(mac,mask,cap) /* ARGUMENTS:                       */
   char          *mac;  /*    Macro name to expand          */
   unsigned short mask; /*    Type of macro (see macro.h)   */
   int            cap;  /*    Capitalize string if true     */
{
   static char *buff=NULL;
   char *str;

   str = expand(mac, mask);
   if (!cap || !str)
      return str;

   if (buff)
      free(buff);
   buff = strdup(str);
   buff[0] = toupper(buff[0]);
   return buff;
}

static int
print_macros()
{
   FILE *fp;
   macro_t *curr;

   /* Display current macros */
   open_pipe();
   if (status & S_PAGER) fp=st_glob.outp;
   else                     fp=stdout;
   fprintf(fp,"What       Is Short For\n\n");
   for (curr = first_macro; curr && !(status & S_INT); curr=curr->next)
      fprintf(fp,"%-10s %3u %s\n", curr->name,  curr->mask, 
          curr->value);
   return 1;
}

/******************************************************************************/
/* PROCESS MACRO DEFINES AND UNDEFINES                                        */
/******************************************************************************/
int               /* RETURNS: (nothing)     */
define(argc,argv) /* ARGUMENTS:             */
   int    argc;   /*    Number of arguments */
   char **argv;   /*    Argument list       */
{
   int con=0;

   if (match(argv[0], "const_ant"))
      con = DM_CONSTANT;

   switch(argc) {
   case 1: /* Display current macros */
           return print_macros();

   case 2: /* Remove name from macro table */
           undef_name(argv[1]);
           return 1;

   case 3: def_macro(argv[1], DM_VAR|con, argv[2]);
           return 1;

   default:
      {    char buff[MAX_LINE_LENGTH];
           int i;

           strcpy(buff, argv[3]);
           for (i=4; i<argc; i++) {
              strcat(buff, " ");
              strcat(buff, argv[i]);
           }
           def_macro(argv[1], atoi(argv[2])|con, buff);
           return 1;
      }

   /*default: printf("Bad parameters near \"%s\"\n",argv[4]); */
   }
}

/******************************************************************************/
/* DEFINE A MACRO EXPANSION                                                   */
/******************************************************************************/
void                       /* RETURNS: (nothing)             */
def_macro(name,mask,val)   /* ARGUMENTS:                     */
   char *name;             /*    Alias/variable name         */
   int   mask;             /*    Type of macro (see macro.h) */
   char *val;              /*    (Arbitrary length) string to expand it to */
{
   unsigned short i;
   char buff[MAX_LINE_LENGTH];
   char *value;
   macro_t *curr;

   if (!mask) {
      printf("Bad mask value.\n");
      return;
   }
   value=noquote(val,0);

   for (i=0; rovars[i]; i++) 
      if (match(name, rovars[i])) {
         printf("Variable is readonly.\n");
         xfree_string(value);
         return;
      }

#ifdef HAVE_PUTENV
   if (mask & DM_ENVAR) {
      int   len = strlen(name) + strlen(value) + 2;
      char *ptr = (char*) xalloc(0, len);
      
      sprintf(ptr,"%s=%s",name,value);
      putenv(ptr);
      xfree_string(ptr);
   }
#endif

   /* Add name to macro table */
   if (mask & ~DM_ENVAR) {
      if (orig_stdin[stdin_stack_top].type & STD_SUPERSANE)
         mask |= DM_SUPERSANE;
      curr = find_macro(name, mask);
      if (!curr) {

         /* Create new element */
         curr = (macro_t *)xalloc(0, sizeof(macro_t));
         curr->name  = xstrdup(name);
         curr->mask  = mask;
         curr->value = xstrdup(value);

         /* Add to linked list */
         curr->next = first_macro;
         first_macro = curr;

      } else { /* already defined */
         if (curr->mask & DM_CONSTANT) {
            if (!(flags & O_QUIET) && strcmp(curr->value, value))
               printf("Can't redefine constant '%s'\n", curr->name);
            xfree_string(value);
            return;
         } 
         if ((curr->mask & (DM_SANE|DM_SUPERSANE))
          || !(mask & (DM_SANE|DM_SUPERSANE))) {
            curr->mask  = mask;
            xfree_string(curr->value);
            curr->value = xstrdup(value);
         }
      }
   }
   xfree_string(value);
}

/******************************************************************************/
/* REMOVE A CLASS OF MACRO EXPANSIONS                                         */
/******************************************************************************/
void                 /* RETURNS: (nothing)            */
undefine(mask)       /* ARGUMENTS:                    */
unsigned short mask; /*    Bitmask of types to remove */
{
   macro_t *curr, *prev;

   if (debug & DB_MACRO) 
      printf("undefine: mask=%hd\n",mask);

   curr=first_macro;
   prev=NULL;
   while (curr) {
      if (curr->mask & mask) {
         if (debug & DB_MACRO)
            printf("undefine: %s %hd\n", curr->name, curr->mask);
         undef_macro(prev);
         if (prev)
            curr=prev->next;
         else
            curr=first_macro;
      } else {
         prev=curr;
         curr=curr->next;
      }
   }
}

void
undef_name(name) 
   char *name;
{
   macro_t *curr = first_macro, *prev=NULL;

   /* Locate name in macro list */
   while (curr && !match(name, curr->name)) {
      prev=curr;
      curr=curr->next;
   }

   /* Remove name from macro table */
   if (!curr) {
      if (!(flags & O_QUIET))
         printf("Can't find definition for %s\n",name);
   } else if (curr->mask & DM_CONSTANT) {
      if (!(flags & O_QUIET))
         printf("Can't undefine constant '%s'\n", name);
   } else
      undef_macro(prev);
}

/******************************************************************************/
/* REMOVE A SINGLE MACRO EXPANSION                                            */
/******************************************************************************/
void               /* RETURNS: (nothing)           */
undef_macro(prev)  /* ARGUMENTS:                   */
   macro_t *prev;  /*    List element before the one to delete */
{
   macro_t *curr;

   /* Remove from linked list */
   if (prev) {
      curr = prev->next;
      prev->next = curr->next;
   } else {
      curr = first_macro;
      first_macro = curr->next;
   }

   /* Delete the actual element */
   xfree_string(curr->name);
   xfree_string(curr->value);
   xfree_string(curr);
}

char *
conference(cap)  
   int cap;
{ 
   return capexpand("conference",  DM_VAR, cap); 
}

char *
fairwitness(cap)
   int cap;
{ 
   return capexpand("fairwitness", DM_VAR, cap); 
}

char *
topic(cap)       
   int cap;
{ 
   return capexpand("item", DM_VAR, cap);
}

char *
subject(cap)
   int cap;
{
   return capexpand("subject", DM_VAR, cap);
}
