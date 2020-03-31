/* $Id: driver.c,v 1.28 1998/02/13 10:56:16 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <ctype.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif
#include <sys/stat.h>
#include <signal.h>
#if 0
#include <pwd.h>
#endif
#include "yapp.h"
#include "struct.h"
#include "driver.h"
#include "xalloc.h"
#include "lib.h"
#include "macro.h"
#include "joq.h"
#include "rfp.h"
#include "change.h"
#include "item.h" /* needed by sep.h */
#include "sep.h"
#include "help.h"
#include "conf.h"
#include "system.h"
#include "files.h"
#include "sum.h"   /* for refresh_list */
#include "edbuf.h" /* for text_cmd_dispatch */
#include "misc.h"  /* for misc_cmd_dispatch */
#include "user.h"  /* for authorize command */
#include "license.h"  /* for get_conf_param() */
#include "sysop.h" /* for cfcreate, etc commands */
#include "stats.h" /* for clear_cache() */
#include "www.h" /* for urlset() */
#include "news.h" /* for incorporate() */
#include "edit.h" /* for retitle */
#include "security.h" /* for check_acl() */
#ifndef HAVE_GETHOSTNAME
#ifdef HAVE_SYSINFO
#include <sys/systeminfo.h>
#endif
#endif

/* GLOBAL VARS */
#ifdef HAVE_INFORMIX_OPEN
extern char ** environ;     /* System environment variables */
#endif

/* Status info */
#ifdef WWW
unsigned long  ticket=0;            
#endif
flag_t         flags =0;            /* user settable parameter flags */
unsigned char  mode  =M_OK;         /* input mode (which prompt)     */
flag_t         status=0;            /* system status flags           */
flag_t         debug =0;            /* debug flags                   */
#if 0
/* real_stdin is now stdin_stack[0] */
#ifdef STUPID_REOPEN
FILE          *real_stdin=NULL;
#else
int            real_stdin=0;
#endif
#endif

/* Conference info */
int     current = -1;              /* current index to cflist       */
int     confidx = -1;              /* current index to conflist     */
int     defidx  = -1;
int     joinidx = -1;              /* current index to conflist     */
char    confname[MAX_LINE_LENGTH]; /* name of current conference    */
char  **cflist = (char **)0;       /* User's conflist               */
char   *cfliststr= (char *)0;      /* cflist in a string            */
char  **fw;                        /* List of FW's for current conf */
partentry_t part[MAX_ITEMS];       /* User participation info       */

/* System info */
char    bbsdir[MAX_LINE_LENGTH];   /* Directory for bbs files       */
char    helpdir[MAX_LINE_LENGTH];  /* Directory for help files      */
assoc_t *conflist=NULL;            /* System table of conferences   */
assoc_t *desclist=NULL;            /* System table of conference descriptions */
int     maxconf=0;                 /* maximum index to conflist     */
char    hostname[MAX_LINE_LENGTH]; /* System host name */

/* Info on the user */
int     uid;                       /* User's UID                    */
char    login[L_cuserid];          /* User's login                  */
char    fullname[MAX_LINE_LENGTH]; /* User's fullname from passwd   */
char    email[MAX_LINE_LENGTH];          /* User's login                  */
char    home[MAX_LINE_LENGTH];     /* User's home directory         */
char    work[MAX_LINE_LENGTH];     /* User's work directory         */
char    partdir[MAX_LINE_LENGTH];  /* User's participation file dir */
#if 0
struct passwd *pwd;
#endif
int     cgi_item; /* single item # in cgi mode */
int     cgi_resp; /* single resp # in cgi mode */

/* Item statistics */
status_t   st_glob;                /* statistics on current conference */
status_t   st_new;                 /* statistics on new conference to join */
sumentry_t sum[MAX_ITEMS];         /* items in current conference      */
response_t re[MAX_RESPONSES];      /* responses to current item        */

/* Variables global to this module only */
static char *cmdbuf = NULL;
       char *pipebuf = NULL;
       char *retbuf = NULL;
       char evalbuf[MAX_LINE_LENGTH];
FILE *pipe_input=NULL;

void
free_buff(buf)
   char **buf;
{
   if (*buf) {
      xfree_string(*buf);
      *buf = NULL;
   }
}

#ifdef INCLUDE_EXTRA_COMMANDS
static char *options="qulosnij:x:";    /* Command-line options          */
#else
static char *options="qulosniwj:x:";    /* Command-line options          */
#endif
option_t option[]={                /* User-definable flags          */
/*  name,       mask,        default, */
 { "quiet",     O_QUIET,     DEFAULT_OFF, },
 { "buffer",    O_BUFFER,    DEFAULT_ON, },
 { "default",   O_DEFAULT,   DEFAULT_ON, },
 { "observe",   O_OBSERVE,   DEFAULT_OFF, },
 { "st_rip",    O_STRIP,     DEFAULT_OFF, },
 { "so_urce",   O_SOURCE,    DEFAULT_ON, },
 { "inc_orporate", O_INCORPORATE, DEFAULT_OFF, },
#ifdef INCLUDE_EXTRA_COMMANDS
 { "www",       O_CGIBIN,    DEFAULT_OFF, },
#endif
 { "f_orget",   O_FORGET,    DEFAULT_ON, },
 { "sta_y",     O_STAY,      DEFAULT_OFF, },
 { "dot",       O_DOT,       DEFAULT_ON, },
 { "ed_always", O_EDALWAYS,  DEFAULT_OFF, },
 { "me_too",    O_METOO,     DEFAULT_ON, },
 { "nu_mbered", O_NUMBERED,  DEFAULT_OFF, },
 { "d_ate",     O_DATE,      DEFAULT_OFF, },
 { "u_id",      O_UID,       DEFAULT_OFF, },
 { "ma_iltext", O_MAILTEXT,  DEFAULT_OFF, },
 { "autosave",  O_AUTOSAVE,  DEFAULT_OFF, },
 { "verbose",   O_VERBOSE,   DEFAULT_OFF, },
 { "scr_ibbler",O_SCRIBBLER, DEFAULT_OFF, },
 { "sig_nature",O_SIGNATURE, DEFAULT_OFF, },
 { "readonly",  O_READONLY,  DEFAULT_OFF },
 { "sen_sitive",O_SENSITIVE, DEFAULT_OFF },
 { "unseen",    O_UNSEEN,    DEFAULT_ON },
 { "autojoin",  O_AUTOJOIN,  DEFAULT_OFF },
 { "label",     O_LABEL,     DEFAULT_ON },
 { NULL,  0,  0 }
};
option_t debug_opt[]={             /* User-definable flags          */
/*  name,          mask,        default, */
 { "mem_ory",      DB_MEMORY,   DEFAULT_OFF, },
 { "conf_erences", DB_CONF,     DEFAULT_OFF, },
 { "mac_ro",       DB_MACRO,    DEFAULT_OFF, },
 { "range",        DB_RANGE,    DEFAULT_OFF, },
 { "driv_er",      DB_DRIVER,   DEFAULT_OFF, },
 { "file_s",       DB_FILES,    DEFAULT_OFF, },
 { "part",         DB_PART,     DEFAULT_OFF, },
 { "arch",         DB_ARCH,     DEFAULT_OFF, },
 { "lib",          DB_LIB,      DEFAULT_OFF, },
 { "sum",          DB_SUM,      DEFAULT_OFF, },
 { "item",         DB_ITEM,     DEFAULT_OFF, },
 { "user",         DB_USER,     DEFAULT_OFF },
 { "pipe",         DB_PIPE,     DEFAULT_OFF },
 { "ioredir",      DB_IOREDIR,  DEFAULT_OFF },
 { NULL,  0,  0 }
};

#if 0
#ifdef STUPID_REOPEN
#if 0
int
dfileno(fp)
   FILE *fp;
{
   return fileno(fp);
}
int
fdnext()
{
   FILE *fp;
   int i;
   fp = fopen("/usr/local/etc/apache_1.0.3/yapp/.src/README", "r");
   i = fileno(fp);
   fclose(fp);
   return i;
}
#endif

FILE * /* saved fp */
new_stdin(fp)
   FILE *fp; /* new fp to copy as stdin */
{
   int old, i;
   FILE *old_fp;

   if (!fp || fileno(fp)<1) {
      printf("Invalid fp passed to new_stdin()!\n");
      return 0;
   }

   old = dup(0);
   old_fp = fdopen(old, "r");
   madd(fileno(old_fp),"stdin",O_R,0); /* Save info for debugging */
   fclose(st_glob.inp); /* close stdin */
   i = dup(fileno(fp));
   if (i)
      printf("Dup error, i=%d fd=%d old=%d\n", i, fileno(fp), old);
   st_glob.inp = fdopen(0, "r");
/*printf("New stdin=%d saved old in %d\n", fd, old);*/
   mclose(fp);
   return old_fp;
}

void
restore_stdin(fp)
   FILE *fp; /* new fp to restore */
{
   int i;

/*printf("restoring %d as stdin\n", fd);*/
   fclose(st_glob.inp);
   i = dup(fileno(fp));
   if (i)
      printf("Dup error, i=%d fd=%d\n", i, fileno(fp));
   st_glob.inp = fdopen(0, "r");
   mclose(fp);
}

#else

int /* saved fd */
new_stdin(fd)
   int fd; /* new fd to copy as stdin */
{
   int old, i;

   if (fd<1) {
      printf("Invalid fd %d passed to new_stdin()!\n", fd);
      return 0;
   }

   old = dup(0);
   close(0);  /* close stdin */
   i = dup(fd);
   if (i)
      printf("Dup error, i=%d fd=%d old=%d\n", i, fd, old);
   st_glob.inp = fdopen(0, "r");
/*printf("New stdin=%d saved old in %d\n", fd, old);*/
   return old;

}
void
restore_stdin(fd)
   int fd; /* new fd to restore */
{
/*printf("restoring %d as stdin\n", fd);*/
   close(0);
   dup(fd);
   st_glob.inp = fdopen(0, "r");
   close(fd);
}
#endif
#endif

int stdin_stack_top=0; /* 0 is always real_stdin */
stdin_t  orig_stdin[STDIN_STACK_SIZE]; /* original fp's opened */
stdin_t saved_stdin[STDIN_STACK_SIZE]; /* dup'ed fds when pushed */

void
push_stdin(fp, type) /* new fp */
   FILE *fp;
   int   type;
{
   int   old_fd, i;
   FILE *old_fp;

   if (!fp || fileno(fp)<1) {
      printf("Invalid fp passed to push_stdin()!\n");
      return;
   }
   if (stdin_stack_top+1 >= STDIN_STACK_SIZE) {
      error("out of stdin stack space", NULL);
      endbbs(1);
   }

   orig_stdin[stdin_stack_top+1].type = type;
   orig_stdin[stdin_stack_top+1].fp   = fp;
   orig_stdin[stdin_stack_top+1].fd   = fileno(fp);

/* printf("push_stdin BEFORE ftell=%d\n", ftell(st_glob.inp)); */
   old_fd = dup(0);
#ifdef STUPID_REOPEN
   old_fp = fdopen(old_fd, "r");
   madd(old_fd,"stdin",O_R,0); /* Save info for debugging */
   fclose(st_glob.inp);
   i = dup(fileno(fp));
   if (i)
      printf("Dup error, i=%d fd=%d old=%d\n", i, fileno(fp), old_fd);
   st_glob.inp = fdopen(0, "r");
   mclose(fp);
#else
   old_fp = st_glob.inp;
   close(0);  /* close stdin */
   i = dup(fileno(fp));
   if (i)
      printf("Dup error, i=%d fd=%d old=%d\n", i, fileno(fp), old_fd);
   st_glob.inp = fdopen(0, "r");
#endif
   saved_stdin[stdin_stack_top].type = orig_stdin[stdin_stack_top].type;
   saved_stdin[stdin_stack_top].fp   = old_fp;
   saved_stdin[stdin_stack_top].fd   = old_fd;
   if (debug & DB_IOREDIR) {
      printf("orig_stdin[%d]=%d   saved_stdin[%d]=%d\n",
       stdin_stack_top+1, orig_stdin[stdin_stack_top+1].fd,
       stdin_stack_top,   saved_stdin[stdin_stack_top].fd);
   }
   stdin_stack_top++;
}

void
pop_stdin()
{
   int i, fd;
   FILE *fp;

   if (stdin_stack_top<=0) {
      error("tried to pop off null stdin stack", NULL);
      endbbs(1);
   }

   switch(orig_stdin[stdin_stack_top].type & STD_TYPE) { /* close stdin */
   case STD_FILE : mclose(orig_stdin[stdin_stack_top].fp); break;
   case STD_SFILE: smclose(orig_stdin[stdin_stack_top].fp); break;
   case STD_SPIPE: spclose(orig_stdin[stdin_stack_top].fp); break;
   }
#ifdef STUPID_REOPEN
   fclose(st_glob.inp);

   fp = saved_stdin[--stdin_stack_top].fp;
   if (debug & DB_IOREDIR)
      printf("restoring %d as stdin\n", fileno(fp));
   i = dup(fileno(fp));
   if (i)
      printf("Dup error, i=%d fd=%d\n", i, fileno(fp));
   st_glob.inp = fdopen(0, "r");
   mclose(fp);
#else
   close(0);

   fd = saved_stdin[--stdin_stack_top].fd;
   if (debug & DB_IOREDIR)
      printf("restoring %d as stdin\n", fd);
   dup(fd);
   st_glob.inp = fdopen(0, "r");
   close(fd);
   if (saved_stdin[stdin_stack_top].fp)
      st_glob.inp = saved_stdin[stdin_stack_top].fp;
#endif

   clearerr(st_glob.inp);
/* printf("pop_stdin AFTER ftell=%d\n", ftell(st_glob.inp)); */
}

void
open_pipe()
{
   char *pager,*p;

   if (status & S_REDIRECT)
      return;

   /* Need to check if pager exists */
   if (!(status & S_PAGER)) {
      if (!pipebuf) {
         p = expand("pager", DM_VAR);
         if (p) {
            pipebuf = (char *)xalloc(0, strlen(p)+1);
            strcpy(pipebuf, p);
         }
      }

      if (pipebuf) {

      /* Compress it a bit */
      pager=pipebuf;
      while (*pager && *pager==' ') pager++; /* skip leading/trailing spaces */
      for (p=pager+strlen(pager)-1; *p==' ' && p>=pager; p--) *p='\0'; 
      if (*pager=='"') {
         pager++;
         if (p>=pager && *p=='"') { *p='\0'; p--; }
      }
      if (*pager=='\'') {
         pager++;
         if (p>=pager && *p=='\'') { *p='\0'; p--; }
      }

      if (!(flags & O_BUFFER))
         pager[0]='\0';
      if (pager[0]) {
         st_glob.outp =spopen(pager);
         if (st_glob.outp == NULL)
            pager[0]='\0';
         else
            status |= S_PAGER;
      }
      if (!pager[0]) 
         st_glob.outp =stdout;

      } else
         st_glob.outp =stdout;
   }
}

/******************************************************************************/
/* PRINT PROMPT FOR AN INPUT MODE                                             */
/******************************************************************************/
void               /* RETURNS: (nothing) */
print_prompt(mod)  /* ARGUMENTS:         */
unsigned char mod; /*    Input mode      */
{ 
   char *str=NULL;

   if (flags & O_QUIET) return;

   switch(mod) {
   case M_OK :  /* In a conference or not? */
                str = (confidx<0) ? "noconfp" : "prompt";
		break;
/* case M_RFP:  str = (st_glob.c_status & (CS_OBSERVER|CS_NORESPONSE))? */
   case M_RFP:  str = (!check_acl(RESPOND_RIGHT,confidx))?
                 "obvprompt":"rfpprompt";
                break;
   case M_TEXT: str="text";      break;
   case M_JOQ:  str="joqprompt"; break;
   case M_EDB:  str="edbprompt"; break;
   }
   if (debug & DB_DRIVER) 
      printf("!%s!\n",str);
   confsep(expand(str, DM_VAR),confidx,&st_glob,part,1); /* expand seps & print */
}

/******************************************************************************/
/* COMMAND LOOP: PRINT PROMPT & GET RESPONSE                                  */
/******************************************************************************/
char                 /* RETURNS: 0 on eof, 1 else    */
get_command(def,lvl) /* ARGUMENTS:                   */
   char *def;        /*    Default command (if any)  */
   int   lvl;        /*    Min level of stdin to use */
{
   char ok=1, *inbuff, *inb;

   if (status & S_INT) status &= ~S_INT; /* Clear interrupt */
   if (status & S_PAGER) spclose(st_glob.outp );
   if (cmdbuf)
      inbuff = cmdbuf;
   else {
      int c;

      /* Pop up stdin stack until we're not sitting at EOF */
      while (orig_stdin[stdin_stack_top].type!=STD_TTY 
       && ((c=getc(st_glob.inp)) == EOF) 
       && (stdin_stack_top > 0 + (orig_stdin[0].type==STD_SKIP)))
         pop_stdin();
      if (orig_stdin[stdin_stack_top].type!=STD_TTY && c!=EOF)
         ungetc(c, st_glob.inp);

      if (stdin_stack_top < lvl) {
/*
if (!ok) {
printf("Returning 0 since stdin_stack_top=%d, lvl=%d\n", stdin_stack_top, lvl);
fflush(stdout);
}
*/
         return 0;
      }
      
      /* If taking input from the keyboard, print a prompt */
      if (isatty(fileno(st_glob.inp)))
         print_prompt(mode);

      if (mode==M_OK) 
         status &= ~S_STOP;
      inbuff = xgets(st_glob.inp, lvl); 
      ok = (inbuff != NULL);
      if (ok && orig_stdin[stdin_stack_top].type!=STD_TTY && (flags & O_VERBOSE)) {
         printf("command: %s\n",inbuff); fflush(stdout);
      }
/*
printf("%d command: %s (%d)\n",stdin_stack_top, inbuff, ftell(st_glob.inp)); fflush(stdout);
*/
   }
   if (cmdbuf || ok) {

      /* Strip leading & trailing spaces */
      for (inb=inbuff+strlen(inbuff)-1; inb>=inbuff && *inb==' '; inb--) *inb=0;
      for (inb=inbuff; *inb==' '; inb++);

      if (!*inb) {
         if (status & S_BATCH) /* ignore blank lines in batch mode */
            ok = 1;
         else
            ok = command(def,0);
      } else       
         ok = command(inb,0);
   }
   xfree_string(inbuff);
/*
if (!ok) {
printf("returning 0, stdin_stack_top=%d, lvl=%d\n",
 stdin_stack_top, lvl); fflush(stdout);
}
*/
   return ok;
}

/******************************************************************************/
/* CLEAN UP MEMORY AND EXIT                                                   */
/******************************************************************************/
void        /* RETURNS: (nothing) */
endbbs(ret) /* ARGUMENTS:         */
int ret;    /*    Exit status     */
{
   int i;

#if 0
#ifdef STUPID_REOPEN
   if (real_stdin)
      mclose(real_stdin);
#endif
#endif

   if (status & S_PAGER) spclose(st_glob.outp );

   if (debug & DB_DRIVER) 
      printf("endbbs:\n");
   if (confidx>=0) leave(0,(char**)0); /* leave current conference */

   /* Must close stdins after leave() since leave opens /usr/bbs/rc for stdin */
   while (stdin_stack_top > 0)
      pop_stdin();

   /* Release license */
   release_license();
   free_config();

   /* Free up space, etc */
   free_list(conflist);
   free_list(desclist);
   mystrtok(NULL, NULL);

   for (i=0; i<MAX_RESPONSES; i++) {
      if (re[i].login)    { xfree_string(re[i].login);    re[i].login   =0; }
      if (re[i].fullname) { xfree_string(re[i].fullname); re[i].fullname=0; }
#ifdef NEWS
      if (re[i].mid)      { xfree_string(re[i].mid);      re[i].mid     =0; }
#endif
   }
#ifdef HAVE_INFORMIX_OPEN
#endif /* HAVE_INFORMIX_OPEN */

   clear_cache();   /* throw out stat cache */
   undefine(~0);    /* undefine all macros */
   xfree_array(cflist);
   xfree_string(cfliststr);
   free_buff(&cmdbuf);
   free_buff(&pipebuf);
   free_buff(&retbuf);
   mcheck();        /* verify that files are closed */
   xcheck();        /* verify that memory is clean */
   (void)exit(ret);
}

void
open_cluster(bdir,hdir)
char *bdir, /*   BBSDIR  */
     *hdir; /*   HELPDIR */
{
   int   i;

   /* Free up space, etc */
   free_list(conflist);

   /* Read in /usr/bbs/conflist */
   strcpy(bbsdir,bdir);
   if (hdir)
      strcpy(helpdir,hdir);
   else
      sprintf(helpdir, "%s/help", bbsdir);
   if ((conflist = grab_list(bbsdir,"conflist",0))==NULL)
      endbbs(2);
   desclist = grab_list(bbsdir,"desclist",0);
   maxconf = xsizeof(conflist)/sizeof(assoc_t);
   for (i=1; i<maxconf; i++)
      if (!strcmp(conflist[0].location,conflist[i].location)
       ||   match(conflist[0].location,conflist[i].name)) 
         defidx=i;
   if (defidx<0) printf("Warning: bad default conference\n");

   /* Source system rc file */
   source(bbsdir,"rc", STD_SUPERSANE, SL_OWNER);
}

static int_on=1;

void
ints_on()
{
#ifdef SV_INTERRUPT
   struct sigvec vec;
   sigvec(SIGINT,  NULL, &vec);
   vec.sv_flags |= SV_INTERRUPT;
   sigvec(SIGINT,  &vec, NULL);
#else
#ifdef SA_INTERRUPT
   struct sigaction vec;
   sigaction(SIGINT,  NULL, &vec);
   vec.sa_flags |= SA_INTERRUPT;
   sigaction(SIGINT,  &vec, NULL);
#else
#ifdef SA_RESTART
   struct sigaction vec;
   sigaction(SIGINT,  NULL, &vec);
   vec.sa_flags &= ~SA_RESTART;
   sigaction(SIGINT,  &vec, NULL);
#endif
#endif
#endif
   int_on=1;
}

void
ints_off()
{
#ifdef SV_INTERRUPT
   struct sigvec vec;
   sigvec(SIGINT,  NULL, &vec);
   vec.sv_flags &= ~SV_INTERRUPT;
   sigvec(SIGINT,  &vec, NULL);
#else
#ifdef SA_INTERRUPT
   struct sigaction vec;
   sigaction(SIGINT,  NULL, &vec);
   vec.sa_flags &= ~SA_INTERRUPT;
   sigaction(SIGINT,  &vec, NULL);
#else
#ifdef SA_RESTART
   struct sigaction vec;
   sigaction(SIGINT,  NULL, &vec);
   vec.sa_flags |= SA_RESTART;
   sigaction(SIGINT,  &vec, NULL);
#endif
#endif
#endif
   int_on=0;
}

RETSIGTYPE 
handle_alarm(sig, code, scp, addr)
int sig, code;
/* struct sigcontext */ void *scp;
char *addr;
{
   error("out of time", NULL);
   exit(1);
}

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    init
Called by:   main
Arguments:   
Returns:    
Calls:       source for .cfonce and system rc
             grab_file to get .cflist and conflist
Description: Sets up global variables, i.e. uid, login, envars,
             workdir, processes rc file for system and for user
******************************************************************************/
void            /* RETURNS: (nothing)                  */
init(argc,argv) /* ARGUMENTS:                          */
int    argc;    /*    Number of command-line arguments */
char **argv;    /*    Command-line argument list       */
{
   short c,o,i;
   extern char *optarg;
   extern int optind,opterr;
   char xfile[MAX_LINE_LENGTH];
   char *mail;
   int forcejoin = 0;

#ifdef HAVE_INFORMIX_OPEN
   char *ep;
   int size=0; /* number of elements in environ */
   char build_env [MAX_LINE_LENGTH]; /* Build environment strings */
#endif


   orig_stdin[0].type = STD_TTY;

#ifdef HAVE_GETHOSTNAME
   if (gethostname(hostname,MAX_LINE_LENGTH))
#else
#ifdef HAVE_SYSINFO
   if (sysinfo(SI_HOSTNAME, hostname, MAX_LINE_LENGTH)<0)
#endif
#endif
      error("getting host name",NULL);

   /* If hostname is not fully qualified, see what we can do to get it */
   if (!strchr(hostname, '.')) {
      FILE *fp;
      if ((fp = fopen("/etc/resolv.conf", "r")) != NULL) {
         char *buff, field[80], value[80];
         int done = 0;
         while (!done && (buff = xgets(fp, 0))) {
            if (sscanf(buff, "%s%s", field, value)==2
             && !strcmp(field,"domain")) {
               sprintf(hostname+strlen(hostname), ".%s", value);
               done++;
            }
            xfree_string(buff);
         }
         fclose(fp);
      }
   }
   strcpy(hostname, lower_case(hostname)); /* convert upper to lower case */

   read_config();

   /* Start up interrupt handling *
   for (c=1; c<=32; c++)
      signal(c, handle_other);
    * */
   signal(SIGINT,  handle_int);
   signal(SIGPIPE, handle_pipe);
   signal(SIGALRM, handle_alarm);
   if (getuid()==get_nobody_uid())
      alarm(600); /* web process will abort after 10 minutes */
   /* *
   signal(SIGINT,  handle_other);
   signal(SIGPIPE, handle_other);
    * */
   ints_off();

   /* Initialize options */
   for (o=0; option[o].name; o++)
      flags |= option[o].deflt * option[o].mask;
   for (o=0; debug_opt[o].name; o++)
      debug |= debug_opt[o].deflt * debug_opt[o].mask;

   /* Set up user variables */
   evalbuf[0] = '\0';
   free_buff(&pipebuf);
   free_buff(&cmdbuf);
   st_glob.c_status = 0;
#ifdef NEWS
   st_glob.c_article = 0;
#endif
   st_glob.inp = stdin;

   /* Get current user info */
   uid      = 0;    
   login[0] = '\0';
{
#ifdef HAVE_INFORMIX_OPEN
   /* Need to set up environment before querying 
    * user information for local users
    */
   sprintf(build_env,"%s=%s","INFORMIXDIR",
    get_conf_param(V_INFORMIXDIR,"/usr/local/informix"),0);
   ep=strcpy(malloc(strlen(build_env+1)),build_env);
   putenv(ep);
   sprintf(build_env,"%s=%s","INFORMIXSERVER",
    get_conf_param(V_INFORMIXSERVER,""),0);  
   ep=strcpy(malloc(strlen(build_env+1)),build_env);
   putenv(ep);
   sprintf(build_env,"%s=%s","ONCONFIG",
    get_conf_param(V_ONCONFIG,""),0);
   ep=strcpy(malloc(strlen(build_env+1)),build_env);
   putenv(ep);
   sprintf(build_env,"%s=%s","INFORMIXSQLHOSTS",
    get_conf_param(V_INFORMIXSQLHOSTS,""),0);
   ep=strcpy(malloc(strlen(build_env+1)),build_env);
   putenv(ep);
#endif /* HAVE_INFORMIX_OPEN */
}
   if (!get_user(&uid, login, st_glob.fullname, home, email)) {
      error("reading ","user entry");
      endbbs(2);
   }
   strcpy(fullname, st_glob.fullname);
   if (!strcmp(login, get_conf_param("nobody",NOBODY)))
      status |= S_NOAUTH;

   /* Process command line options here */
   if (match(get_conf_param("safe","true"),"true")) {
      if (!uid || uid==geteuid()) {
         printf("login %s -- invoking bbs -%s\n",login,(uid)?"n":"no");
         flags &= ~(O_SOURCE); /* for security */
         if (!uid) 
            flags |= O_OBSERVE|O_READONLY; 
      }
   }
   confname[0]=xfile[0] = '\0';
   while ((c = getopt(argc, argv, options)) != -1) {
      o = strchr(options,c)-options;
      if (o>=0 && o<8) 
         flags ^= option[o].mask;
      else if (c=='j') {
         strcpy(confname,optarg);
         forcejoin = O_AUTOJOIN;
      } else if (c=='x') {
         strcpy(xfile, optarg);
      }
      if (c=='o')
         flags ^= O_READONLY;  /* -o does observer AND readonly */
   }
#ifdef INCLUDE_EXTRA_COMMANDS
   if ((flags & O_CGIBIN) || !strcmp(argv[0]+strlen(argv[0])-8, "yapp-cgi")) {
      flags = (flags & ~O_BUFFER)|O_QUIET|O_CGIBIN|O_OBSERVE;
      argv += optind;
      argc -= optind;
      if (argc<1) {
         confname[0]=0;
         flags &= ~O_DEFAULT;
      } else
         strcpy(confname, argv[0]);
      if (argc<2)
         cgi_item = -1;
      else
         cgi_item = atoi(argv[1]);
      if (argc<3)
         cgi_resp = -1;
      else
         cgi_resp = atoi(argv[2]);
   } else 
#endif
   if (optind < argc) 
      strcpy(confname,argv[argc-1]);

   for (i=0; i<MAX_RESPONSES; i++) {
      re[i].fullname = re[i].login = NULL;
#ifdef NEWS
      re[i].mid = NULL;
      re[i].article = 0;
#endif
   }

   /* Set up user customizations */
   def_macro("today",DM_PARAM,"+0");
   mail = getenv("SHELL");
   if (mail) def_macro("shell",DM_VAR|DM_ENVAR,mail);
   mail = getenv("EDITOR");
   if (mail) def_macro("editor",DM_VAR|DM_ENVAR,mail);
   mail = getenv("MESG");
   if (mail) def_macro("mesg",DM_VAR|DM_ENVAR,mail);
   urlset(); /* do QUERY_STRING sets */

   /* Read in /usr/bbs/conflist */
   open_cluster(get_conf_param("bbsdir", BBSDIR), get_conf_param("helpdir", NULL));

   /* Verify license */
   if (!get_license()) {
      printf("Couldn't get license for %s\n", hostname);
      endbbs(0);
   }

   /* Print identification */
   if (!(flags & O_QUIET))
      command("display version", 0);

   /* Source .cfonce, etc */
   login_user();

   /* Join initial conference */
   if (debug & DB_DRIVER)
      printf("Default: %hd %s\n",defidx,conflist[defidx].name);
   if (flags & O_INCORPORATE) {
      /* Only accept -i from root, daemon, and Yapp owner */
      if (!uid || uid==1 || uid==geteuid()) {
         endbbs(!incorporate(0,sum,part,&st_glob,-1));
      }
      endbbs(2);
   } else if (!(flags & O_DEFAULT) || defidx<0) {
      current = -1;
      st_glob.i_current = 0; /* No current item */
   } else if (confname[0])
      join(confname, forcejoin, 0);
   else if (cflist && xsizeof(cflist))
      join(cflist[current=0], O_AUTOJOIN, 0); /* force join */
   else {
      join(compress(conflist[defidx].name), O_AUTOJOIN, 0); /* force join */
   }

#ifdef INCLUDE_EXTRA_COMMANDS
   /* CGI stuff */
   if (flags & O_CGIBIN) {
      source(bbsdir,"cgi_cfonce", 0, SL_OWNER);
      if (!confname[0]) {         /* GET CONFERENCE LIST             */
         command("list", 0);
      } else if (cgi_item<0) {    /* GET ITEM INDEX FOR A CONF       */
         command("browse all", 0);
      } else if (cgi_resp<0) {    /* GET RESPONSE INDEX FOR AN ITEM? */
         char cmd[80];
         sprintf(cmd, "read %d pass", cgi_item);
         command(cmd, 0);
      } else {                    /* RETRIEVE RESPONSE */
         st_glob.i_current = cgi_item;
         st_glob.r_first = st_glob.r_last = cgi_resp;
         show_header();
         show_range();
      }
      endbbs(0);
   }
#endif

   /* Batch mode */
   if (*xfile) {
      int fd;

      /* When popping stdin, don't wait for input at real_stdin */
      orig_stdin[0].type = STD_SKIP;

      def_macro("batchfile", DM_VAR, xfile);
{
      FILE *fp = mopen(xfile, O_R); /* reopen xfile as stdin */
      if (!fp) {
         printf("Couldn't open %s\n", xfile);
         endbbs(0);
      }
      if (debug & DB_IOREDIR)
         printf("Redirecting input from %s (fd %d)\n", xfile, fileno(fp));
      push_stdin(fp, STD_FILE); /* real_stdin = new_stdin(fp); */
}

#if 0
      real_stdin = dup(0);
      close(0);  /* close stdin */
#endif
printf("");
      status |= S_BATCH; /* set to ignore blank lines */
   }
}

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    char source(char *dir, char *filename, int fl)
Called by:   init
Arguments:   File to source
Returns:     
Calls:       command for each statement
Description: Executes commands in a file, does NOT grab_file since it
             only needs 1-time sequential access.
 As of 10/3/96, this just pushes the file on stdin and lets xgets/ngets
 close it upon EOF.
*******************************************************************************/
char                        /* OUT: 0 on error, 1 on success, -1 on exit */
source(dir,filename,fl,sec) 
   char *dir;               /* IN : Directory containing file        */
   char *filename;          /* IN : Filename of commands to execute  */
   int   fl;                /* IN : Extra flags to set during exec   */
   int   sec;               /* IN : open file as user or as cfadm?   */
{
   FILE *fp;
   char buff[MAX_LINE_LENGTH], *str;
   int ok=1;
#if 0
#ifdef STUPID_REOPEN
   FILE *saved_stdin;
#else
   int saved_stdin;
#endif
#endif

   if (filename)
      sprintf(buff,"%s/%s",dir,filename);
   else
      strcpy(buff,dir);
   if (debug & DB_DRIVER) 
      printf("source: %s\n",buff);
   if (sec==SL_OWNER) {
      if ((fp=mopen(buff,O_R|O_SILENT))==NULL) return 0;
   } else {
      if ((fp=smopenr(buff, O_R|O_SILENT))==NULL) return 0;
   }

   /* Save standard input */
if (!fileno(fp))
printf("save error 1\n");
   if (debug & DB_IOREDIR)
      printf("Redirecting input from %s (fd %d)\n", buff, fileno(fp));
   push_stdin(fp, STD_FILE|fl);         /* saved_stdin = new_stdin(fp); */
/*
   saved_stdin = dup(0);
   close(0);
   dup(fileno(fp));
*/
/*
 * oldstdin    = st_glob.inp;
 * st_glob.inp = fp;
 */

/**
   if (st_glob.inp != stdin)
      mclose(st_glob.inp);
   st_glob.inp = stdin; 
 **/

   /* Execute commands until we pop back to the previous level */
   {
      int lvl = stdin_stack_top;
      while (stdin_stack_top >= lvl)
         get_command(NULL, lvl);
   }
   return 1;

#if 0
   while (ok && (str = xgets(st_glob.inp, lvl))) { /* was , fp */
/* changed 7/28/95 to debug scripts
 *    if ((flags & O_VERBOSE) && mode==M_SANE)
 *       printf("command: %s\n",str);
 */
      if (flags & O_VERBOSE)
         printf("command: %s\n",str); fflush(stdout);
      ok=command(str,0);
      xfree_string(str);
   }
   
   /* Restore standard input */
/*
   i = dup(0);
   close(0);
   dup(saved_stdin);
*/
   while (stdin_stack_top > saved_stdin_stack_top)
      pop_stdin(); /* restore_stdin(saved_stdin); */
/*
 * st_glob.inp = oldstdin;
 */
#ifndef STUPID_REOPEN
   if (sec==SL_OWNER)
      mclose(fp);
   else
      smclose(fp);
#endif

   return (ok)? 1 : -1;
#endif
}

static int fd_stack[3]={-1,-1,-1};
static int fd_top=0;

void
push_fd(fd)
   int fd;
{
   if (fd_top>=3) 
      error("pushing ", "fd");
   else
      fd_stack[fd_top++] = fd;
}

int
pop_fd()
{
   int i;

   if (fd_top>0) {
      fd_top--;
      i = fd_stack[fd_top];
      fd_stack[fd_top] = -1;
      return i;
   }
   return -1;
}

/* History expansion
 * !* = all arguments
 */
char *
expand_history(mac, rest)
   char *mac;  /* Expanded macro     */
   char *rest; /* Original arguments */
{
   char *tmp, *p;
   int count=0, tmplen;
   int i;

   /* Skip leading whitespace in rest */
   while (isspace(*rest)) rest++;

   /* Count occurrences of "!*" in mac */
   for (p=mac; *p; p++) {
      if (p[0]=='!' && p[1]=='*')
         count++;
   }

   /* Allocate enough space */
   tmplen = strlen(mac)+1;
   if (count)
      tmplen += count * (strlen(rest)-2); /* net is rest minus "!*" */
   else
      tmplen += strlen(rest);

   tmp = (char*)xalloc(0, tmplen);

   if (count) {
      p = tmp;
      for (i=0; mac[i]; i++) {
         if (mac[i]=='!' && mac[i+1]=='*') {
            strcpy(p, rest);
            p += strlen(rest);
            i++;
         } else
            *p++ = mac[i];
      }
   } else
      sprintf(tmp,"%s%s",mac,rest);

   return tmp;
}

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    char command(char *command)
Called by:   main
Arguments:   command to process
Returns:     0 if done, 1 else
Calls:       join() for "join", "next" commands
             leave() for "next", "quit" commands
Description: For all command modes, this processes a user command.
             Interrupts go back to here, without changing command mode.
*******************************************************************************/
char              /* RETURNS: Done flag                  */
command(str,lvl)  /* ARGUMENTS:                          */
char *str;        /*    Command to execute               */
int   lvl;        /*    Recursion depth                  */
{
   int argc=0;    /*    Number of arguments */
   int i, skip=0;
   char *argv[MAX_ARGS],cmddel,bufdel;
   char *Sptr,*Eptr,state=1,ok=1, *newstr=NULL, *tmpstr;
   char *cmd=NULL,*cmd2;

   /* Redirection information */
   char wordfile[MAX_LINE_LENGTH];
   int saved_fd[3];
   int is_pipe[3];
   FILE *curr_fp[3];
   int prev_redir = (status & S_REDIRECT);
   int prev_nostdin = (status & S_NOSTDIN);
   int prev_top = fd_top;

   /*
    * FreeBSD needs the second line below and NOT the first, or
    * we see duplicate footers in the read command.  HP-UX, on the other
    * hand has a broken freopen() and needs to re-seek
    */
#ifndef STUPID_REOPEN
#if 0
 10/5/96
   FILE *prev_inp = st_glob.inp;  /* FreeBSD needs this or duplicate read ftr */
#endif
#else
   FILE *saved_inp = NULL;
#endif

   /* Helpcmd section */
   if (mode==M_OK && !str && !(status & S_BATCH)
    && isatty(fileno(st_glob.inp))) {
      char *helpcmd = expand("helpcmd", DM_VAR);
      if (helpcmd && helpcmd[0]) {
            str = helpcmd;
      }
   }

   for (i=0; i<3; i++) {
      saved_fd[i] = -1;
      is_pipe[i]  = 0;
   }

   if (!str || !*str) return 1;

   if (debug & DB_DRIVER) 
      printf("command: '%s' level: %d\n",str, lvl);
   if (lvl > CMD_DEPTH) {
      printf("Too many expansions.\n");
      return 0;
   }

   Sptr=str;
   while (isspace(*Sptr)) Sptr++; /* skip whitespace */

   /*
    * Skip if inside false condition, but we have to parse args to find
    * ';' for start of next command
    */
   if (!test_if() && strncmp("endif", Sptr, 5) && strncmp("else", Sptr, 4)
                  && strncmp("if ", Sptr, 3))
      skip=1;

   /* Process shell escape */
   if (str[0]=='!') {
/* Undone at request of sno and jep
 *    if (mode==M_SANE)
 *       printf("%s rc cannot exec: %s\n",conference(1),str);
 *    else {
 */
       if (!skip) {
         unix_cmd(str+1);
         printf("!\n");
       }
/*    } */
      return 1;
   } 

   /* And comments */
   if (*Sptr=='#') 
      return 1;

   cmddel = expand("cmddel",DM_VAR)[0];
   bufdel = expand("bufdel",DM_VAR)[0];

   /* Get arguments using a state machine for lexical analysis */
   free_buff(&pipebuf);
   free_buff(&cmdbuf);
   while (state && argc<MAX_ARGS) {
      switch(state) {
      case 1: /* between words */
              while (isspace(*Sptr)) Sptr++;
              if      (*Sptr==cmddel)  { Sptr++;    state=0; }
              else if (*Sptr==bufdel)  { Eptr= ++Sptr; state=6; }
              else if (*Sptr=='|')  { Eptr= ++Sptr; state=7; }
              else if (*Sptr=='(')  { Eptr=Sptr; state=10; }
              else if (*Sptr=='>')  { Eptr= ++Sptr; state=9; }
              else if (*Sptr=='<')  { Eptr= ++Sptr; state=12; }
              else if (*Sptr=='\'') { Eptr= ++Sptr; state=4; }
              else if (*Sptr=='`')  { Eptr= ++Sptr; state=8; }
/*            else if (*Sptr=='\"') { Eptr= ++Sptr; state=3; } */
              else if (*Sptr=='\"') { Eptr=Sptr; state=3; }
              else if (*Sptr=='%')  { Eptr= ++Sptr; state=11; }
              else if (*Sptr=='\\') { Eptr=Sptr; state=5; }
              else if (*Sptr)       { Eptr=Sptr; state=2; }
              else       state=0;
              break;

      case 2: /* normal word */
              while (*Eptr && !isspace(*Eptr)
               && *Eptr!=cmddel && *Eptr!=bufdel 
               && !strchr("|`'>\\\"",*Eptr)
               && !(Eptr>Sptr && *(Eptr-1)=='=') /* '=' terminates word */
               && (argc || *Sptr=='-' || isdigit(*Sptr) || !isdigit(*Eptr)))
                 Eptr++;

              argv[argc]=(char*)xalloc(0,Eptr-Sptr+1);
              strncpy(argv[argc],Sptr,Eptr-Sptr);
              argv[argc++][Eptr-Sptr]=0;
              Sptr = Eptr;

              if (argc==1) {
                 cmd2=(char *)expand(argv[0],(mode==M_RFP)? DM_RFP : DM_OK);
                 if (cmd2) {
                    char *tmp;

                    tmp = expand_history(cmd2, Eptr);
                    Sptr = tmp; /* Parse cmd instead */

                    /* Undo first argument */
                    argc=0;
                    xfree_string(argv[argc]);

                    /* Store cmd for later freeing */
                    free_buff(&cmd);
                    cmd = tmp;
                 }
              }

              state=1;
              break;

       case 3: /* "stuff" */
          {   int quot=0;
              char *p, *q;

              /* First expand backtick commands */
              for (Eptr=Sptr+1; *Eptr && (*Eptr!='\"' || *(Eptr-1)=='\\'); Eptr++) {
                 if (*Eptr=='`' && *(Eptr-1)!='\\') {
                    char *Cptr = Eptr+1;

                    /* Find end of command */
                    do { Eptr++; } while (*Eptr && *Eptr!='`');

                    free_buff(&cmd);
                    cmd = (char*)xalloc(0, Eptr-Cptr+1);
                    strncpy(cmd,Cptr,Eptr-Cptr);
                    cmd[Eptr-Cptr]=0;
                    if (*Eptr) Eptr++; /* Set Eptr to next char after end ` */

                    status |=  S_EXECUTE;
                    evalbuf[0] = '\0';
                    command(cmd,lvl+1);
                    status &= ~S_EXECUTE;
                    free_buff(&cmd);

                    /* We want to end up with Sptr pointing to a buffer
                     * which contains the inital text, followed by the output,
                     * followed by whatever was left in the original buffer.
                     */
                    tmpstr = (char*)xalloc(0, (Cptr-Sptr-1)+2*strlen(evalbuf)
                     + strlen(Eptr)+1);
                    strncpy(tmpstr, Sptr, Cptr-Sptr-1); /* before */
                    p=tmpstr+(Cptr-Sptr)-1;
                    q=evalbuf;
                    while (*q) {
                       if (*q=='\n' || *q=='\r') {
                          *p++ = ' ';
                          q++;
                       } else if (*q=='"' && (q==evalbuf || q[-1]!='\\')) { 
                          /* escape quotes */
                          *p++ = '\\';
                          *p++ = *q++;
                       } else
                          *p++ = *q++;
                    }
                    *p = '\0';

                    strcat(tmpstr, Eptr); /* after */
                    free_buff(&newstr);
                    Sptr = newstr = tmpstr;
                 }
              }

              /* Count occurrences of \" and set Eptr to end of string */
              Eptr = Sptr; /* reset to start of buffer after " */
              do {
                 Eptr++;
                 if (*Eptr=='\"' && *(Eptr-1)=='\\')
                    quot++;
              } while (*Eptr && (*Eptr!='\"' || *(Eptr-1)=='\\'));

              /* Include the quotes in the arg */
              argv[argc]=(char*)xalloc(0,Eptr-Sptr+2-quot);
              p = argv[argc], q=Sptr;
              while (q <= Eptr) {
                 if (*q=='\\' && q[1]=='"') q++;
                 *p++ = *q++;
              }
              *p = '\0';
              argc++;


              if (*Eptr) Eptr++;
              Sptr = Eptr;
              state=1;
              break;
          }

#if 0
      case 3: /* "stuff" */
          {   int quot=0;
              char *p, *q;

              do { 
                 Eptr++; 
                 if (*Eptr=='\"' && *(Eptr-1)=='\\')
                    quot++;
              } while (*Eptr && (*Eptr!='\"' || *(Eptr-1)=='\\'));

              /* Include the quotes in the arg */
              argv[argc]=(char*)xalloc(0,Eptr-Sptr+2-quot);
              p = argv[argc], q=Sptr;
              while (q <= Eptr) {
                 if (*q=='\\' && q[1]=='"') q++;
                 *p++ = *q++;
              }
              *p = '\0';
              argc++;


              if (*Eptr) Eptr++;
              Sptr = Eptr;
              state=1;
              break;
          }
#endif

      case 4: /* 'stuff' */
/*
              do { Eptr++; } while (*Eptr && *Eptr!='\'');
              argv[argc]=(char*)xalloc(0,Eptr-Sptr+1);
              strncpy(argv[argc],Sptr,Eptr-Sptr);
              argv[argc++][Eptr-Sptr]=0;
*/
          {   int quot=0;
              char *p, *q;

              while (*Eptr && (*Eptr!='\'' || *(Eptr-1)=='\\')) {
                 Eptr++; 
                 if (*Eptr=='\'' && *(Eptr-1)=='\\')
                    quot++;
              } 
              argv[argc]=(char*)xalloc(0,Eptr-Sptr+2-quot);
              p = argv[argc], q=Sptr;
              while (q < Eptr) {
                 if (*q=='\\' && q[1]=='\'') q++;
                 *p++ = *q++;
              }
              *p = '\0';
              argc++;

              if (*Eptr) Eptr++;
              Sptr = Eptr;
              state=1;
              break;
          }

      case 5: /* \\ */
              argv[argc]=(char*)xalloc(0,2);
              strcpy(argv[argc++],"\\");
              Sptr = Eptr+1;
              state=1;
              break;

      case 6: /* ,stuff */
              do { Eptr++; } while (*Eptr && *Eptr!=cmddel);
              free_buff(cmdbuf);
              cmdbuf = (char *)xalloc(0, Eptr-Sptr+1);
              strncpy(cmdbuf,Sptr,Eptr-Sptr);
              cmdbuf[Eptr-Sptr]=0;
              Sptr = Eptr;
              state=1;
              break;

      case 7: /* | stuff */
          {   /* Also |& */
#define OP_STDOUT 0x00
#define OP_STDERR 0x01
              int op=OP_STDOUT;
              char pcmd[MAX_LINE_LENGTH], *str;

              if (*Eptr=='&') {
                 op |= OP_STDERR;
                 Eptr++;
              }
              i = (op & OP_STDERR)? 2 : 1;

              /* Skip whitespace */
              while (isspace(*Eptr)) Eptr++;

              /* Get pcmd */
              str=pcmd;
              while (*Eptr && *Eptr!=cmddel && *Eptr!=bufdel) {
                 *str++ = *Eptr++;
              }
              *str='\0';

              if (!skip) {

                 /* Expand command if sep */
                 if (pcmd[0]=='%') {
                    char *f;
   
                    str = pcmd+1;
                    f = get_sep(&str);
                    strcpy(pcmd, f);
                 }

                 /* Open the pipe */
                 if (sdpopen((status & S_EXECUTE)? &pipe_input : NULL, 
                             &curr_fp[i], pcmd)) {
                    is_pipe[i] = 1;

                    /* Close the old std file descriptor */
                    saved_fd[i] = dup(i);
                    if (debug & DB_PIPE) {
                       fprintf(stderr, "saved fd %d in fd %d\n", i,saved_fd[i]);
                       fflush(stderr);
                    }
                    close(i);

                    /* Open the new std file descriptor (as USER) */
                    dup( fileno(curr_fp[i]) );
                    if (debug & DB_PIPE) {
                       fprintf(stderr, "installed fd %d as new fd %d\n", 
                        fileno(curr_fp[i]), i);
                       fflush(stderr);
                    }
    
                    /* Push i on the "stack" */
                    push_fd(i);
                    status |= S_REDIRECT;
                 }
              }
              Sptr = Eptr;
              state=1;
              break;
          }

      case 8: /* `command` */
/*printf("`` Eptr=!%s!%d\n", Eptr, strlen(Eptr));*/
              do { Eptr++; } while (*Eptr && *Eptr!='`');

              if (!skip) {

              free_buff(&cmd);
              cmd = (char*)xalloc(0, Eptr-Sptr+1);
              strncpy(cmd,Sptr,Eptr-Sptr);
              cmd[Eptr-Sptr]=0;
              if (*Eptr) Eptr++; /* Set Eptr to next char after end ` */

              status |=  S_EXECUTE;
              evalbuf[0] = '\0';
              command(cmd,lvl+1);
              status &= ~S_EXECUTE;
              free_buff(&cmd);

              /* evalbuf should now contain all the output
               * even if obtained via a pipe 
               */

              /* We want to end up with Sptr pointing to a buffer
               * which contains the output, followed by whatever
               * was left in the original buffer.  This way, the
               * output will be split into fields as well, as sh does
               */
              tmpstr = (char*)xalloc(0, strlen(evalbuf) + strlen(Eptr) + 1);
              strcpy(tmpstr, evalbuf);
              strcat(tmpstr, Eptr);
              free_buff(&newstr);
              Sptr = newstr = tmpstr;

/*printf("Sptr=!%s!%d\n", Sptr, strlen(Sptr));*/
/*
              argv[argc]=(char*)xalloc(0,strlen(evalbuf)+1);
              strcpy(argv[argc++],evalbuf);

              Sptr = Eptr;
*/
              } else
                 Sptr = Eptr;
              state=1;
              break;

      case 9: /* > file */
          {   /* Also >>, >&, >>& */
#define OP_STDOUT 0x00
#define OP_STDERR 0x01
#define OP_APPEND 0x10
              int op=OP_STDOUT, tmp;
              int fd=1; /* stdout */
              char filename[MAX_LINE_LENGTH], *str;

              /* Get actual operator */
              if (*Eptr=='>') {
                 op |= OP_APPEND;
                 Eptr++;
              }
              if (*Eptr=='&') {
                 op |= OP_STDERR;
                 fd = 2; /* stderr */
                 Eptr++;
              }

              /* Skip whitespace */
              while (isspace(*Eptr)) Eptr++;

              /* Get filename */
              str=filename;
              while (*Eptr && *Eptr!=cmddel && *Eptr!=bufdel && *Eptr!=' ') {
                 *str++ = *Eptr++;
              }
              *str='\0';

              if (!skip) {

              /* Expand filename if sep */
              if (filename[0]=='%') {
                 char *f;

                 str = filename+1;
                 f = get_sep(&str);
                 strcpy(filename, f);
              }

              /* Open the file */
              curr_fp[fd]=smopenw(filename, (op & OP_APPEND)? O_A : O_W);
              if (!curr_fp[fd]) {
                 error("redirecting output to ", filename);
              } else {
                 is_pipe[fd] = 0; /* just a normal file */

                 /* Close the old std file descriptor */
                 saved_fd[fd] = dup(fd);
                 close(fd);

                 tmp = dup( fileno(curr_fp[fd]) );
/*printf("duped %d to get %d\n", fileno(curr_fp[fd]), tmp);*/
              
                 /* Push fd on the "stack" */
                 push_fd(fd);

                 status |= S_REDIRECT;
              }
              }
              Sptr = Eptr;
              state=1;
              break;
          }

      case 10: /* (stuff) */
              do { 
                 Eptr++; 
              } while (*Eptr && (*Eptr!=')' || *(Eptr-1)=='\\'));
              argv[argc]=(char*)xalloc(0,Eptr-Sptr+2);
              strncpy(argv[argc],Sptr,Eptr-Sptr+1);
              argv[argc++][Eptr-Sptr+1]=0;

              if (*Eptr) Eptr++;
              Sptr = Eptr;
              state=1;
              break;

      case 11: /* %separator stuff */
          {   char *str;
              str = get_sep(&Eptr);

              if (str && *str) { /* only make an arg if something there */
                 argv[argc]=(char*)xalloc(0,strlen(str)+1);
                 strcpy(argv[argc++],str);
              }

              Sptr = Eptr;
              state=1;
              break;
          }

      case 12: /* < file, << word */
          {    
              char filename[MAX_LINE_LENGTH], *str;
              char word[MAX_LINE_LENGTH];
#define OP_FILEIN 0x00
#define OP_WORDIN 0x01
              int op=OP_FILEIN;

              /* Get actual operator */
              if (*Eptr=='<') {
                 op |= OP_WORDIN;
                 Eptr++;
              }

              /* Skip whitespace */
              while (isspace(*Eptr)) Eptr++;

              /* Get word */
              str=word;
              while (*Eptr && *Eptr!=cmddel && *Eptr!=bufdel && *Eptr!=' ') {
                 *str++ = *Eptr++;
              }
              *str='\0';

              if (op & OP_WORDIN) { /* << word */
                 FILE *fp;
                 char *buff;

                 /* Save lines in a temp file until we see word or EOF */
                 sprintf(wordfile, "/tmp/word.%d", getpid());
                 fp = smopenw(wordfile, O_W);
                 while ((buff = xgets(st_glob.inp, stdin_stack_top)) != NULL) {
                    if (!strcmp(buff, word))
                       break;
                    fprintf(fp, "%s\n", buff);
                    xfree_string(buff);
                 }
                 if (buff)
                    xfree_string(buff);
                 smclose(fp);

                 strcpy(filename, wordfile);
              } else {              /*  < file */
                 wordfile[0]='\0';
                 strcpy(filename, word);

                 /* Expand filename if sep */
                 if (filename[0]=='%') {
                    char *f, *str;

                    str = filename+1;
                    f = get_sep(&str);
                    strcpy(filename, f);
                 }
              }

              if (!skip) {

              /* Open the file */
              curr_fp[0]=smopenr(filename, O_R);
if (!curr_fp[0])
printf("smopenr returned null\n");
              is_pipe[0] = 0;

              if (debug & DB_IOREDIR)
                 printf("Redirecting input from %s (fd %d)\n", filename, fileno(curr_fp[0]));
              push_stdin(curr_fp[0], STD_FILE);

#if 0
              /* Close the old std file descriptor */
if (!fileno(curr_fp[0])) {
printf("save error 2, wordfile=%s filename=%s\n", wordfile, filename);
printf("st_glob.inp fd=%d\n", fileno(st_glob.inp));
}
#ifdef STUPID_REOPEN
              saved_inp = new_stdin(curr_fp[0]);
#else
              saved_fd[0] = new_stdin( fileno(curr_fp[0]) );
#endif
/*
              saved_fd[0] = dup(0);
              close(0);
              dup( fileno(curr_fp[0]) );
*/
              
              /* Push 0 on the "stack" */
              push_fd(0);
#endif

              /*
               * We don't want to set S_REDIRECT here since we want
               * the pager to be used when we're only redirecting input.
               * This is used by (for example) the change htmlheader
               * script to filter HTML text from within a template.
               */
              status |= S_NOSTDIN;
/*              status |= S_REDIRECT|S_NOSTDIN; 9/8/95 */
/*
              st_glob.inp = stdin;
*/
              }
              Sptr = Eptr;
              state=1;
              break;
          }
      }
   }
   if (argc && !skip) {
   
      /* Execute command */
      switch(mode) {
      case M_OK:   ok=  ok_cmd_dispatch(argc,argv); break;
      case M_JOQ:  ok= joq_cmd_dispatch(argc,argv); break;
      case M_TEXT: ok=text_cmd_dispatch(argc,argv); break;
      case M_RFP:  ok= rfp_cmd_dispatch(argc,argv); break;
      case M_EDB:  ok= edb_cmd_dispatch(argc,argv); break;
      default: printf("Unknown mode %d\n",mode); break;
      }
   
   } else
      ok=1; /* don't abort on null command */

   /* Free args */
   for (i=0; i<argc; i++) xfree_string(argv[i]);

   /* Now restore original file descriptor state */
   while (fd_top > prev_top) {
      i = pop_fd();
/*
if (!i) 
printf("Restoring %d as stdin\n", saved_fd[i]);
*/
#ifdef STUPID_REOPEN
      if (i)
#endif
      close(i);

      /* Remove temporary file for "<< word" syntax */
      if (!i && wordfile[0]) {
         rm(wordfile, SL_USER);
         wordfile[0]='\0';
      } 

#ifdef STUPID_REOPEN
      if (i) {
         if (is_pipe[i]) {
            sdpclose( pipe_input, curr_fp[i] );
            pipe_input = NULL;
         } else
            smclose( curr_fp[i] );

         /* Restore saved file descriptor */
         dup(saved_fd[i]);
         close(saved_fd[i]);
      } else
         restore_stdin(saved_inp);
#else
      if (is_pipe[i]) {
         sdpclose( pipe_input, curr_fp[i] );
         pipe_input = NULL;
      } else
         smclose( curr_fp[i] );

      /* Restore saved file descriptor */
      dup(saved_fd[i]);
      close(saved_fd[i]);
#endif
      if (debug & DB_PIPE) {
         fprintf(stderr, "Restored fd %d to fd %d\n", saved_fd[i], i); 
         fflush(stderr);
      }

      /* Clear saved info */
      saved_fd[i] = -1;
      is_pipe[i] = 0;
   }
   if (!prev_redir)
      status &= ~S_REDIRECT;
   if (!prev_nostdin)
      status &= ~S_NOSTDIN;
#ifdef STUPID_REOPEN
/*printf("loc=%d ftell=%d eof=%d\n", loc, ftell(st_glob.inp), feof(st_glob.inp));*/
#if 0
   if (ftell(st_glob.inp)!=loc) /* HP-UX doesn't restore locn after reopen */
      fseek(st_glob.inp, loc, SEEK_SET);
#endif
#else
#if 0
   st_glob.inp = prev_inp;      /* FreeBSD has problems without this */
#endif
#endif

   /* Do next ; cmd unless EOF or command says to halt (ok==2) */
   if (ok==1 && *Sptr && !(status & S_STOP)) {
/*printf("Do next ; so doing !%s!%d lvl %d\n", Sptr, strlen(Sptr), lvl+1);*/
      /*
       * 2/2/96: this was lvl+1 below, but it broke 'if' nesting since
       * commands after the ';' appeared to be a level deeper, so
       * 'else' and 'endif' couldn't appear after a ';'
       */
      ok=command(Sptr, lvl); 
   } 
/* else if (*Sptr) status &= ~S_STOP; */
   free_buff(&newstr);
   free_buff(&cmd);
/*
if (!ok) {
printf("command returning 0 for Sptr=!%s!\n", Sptr);
}
*/
   return ok;
}

/* Commands available at the Ok: prompt only */
static dispatch_t ok_cmd[]={
 { "i_tem",     do_read, },
 { "r_ead",     do_read, },
 { "pr_int",    do_read, },
 { "e_nter",    enter, },
 { "s_can",     do_read, },
 { "b_rowse",   do_read, },
   /* j_oin */
 { "le_ave",    leave, },
 { "n_ext",     do_next, },
 { "che_ck",    check, },
 { "rem_ember", remember, },
 { "forget",    forget, },
 { "unfor_get", remember, },
 { "k_ill",     do_kill, },
 { "retitle",   retitle, },
 { "freeze",    freeze, },
 { "thaw",      freeze, },
   /* sync_hronous */
   /* async_hronous */
 { "retire",    freeze, },
 { "unretire",  freeze, },
 { "f_ind",     do_find, },
 { "l_ocate",   do_find, },
 { "seen",      fixseen, },
 { "fix_seen",  fixseen, },
 { "fixto",     fixto, },
 { "re_spond",  respond, },
   /* lpr_int */
 { "li_nkfrom", linkfrom, },
 { "abort",     leave, },
/* ex_it q_uit st_op good_bye log_off log_out h_elp exp_lain sy_stem unix al_ias
 * def_ine una_lias und_efine ec_ho echoe echon echoen echone so_urce m_ail 
 * t_ransmit sen_dmail chat write d_isplay que_ry */
 { "p_articipants", participants, },
 { "desc_ribe", describe, },
/* w_hoison am_superuser */
 { "resign",    resign, },
/* chd_ir uma_sk sh_ell f_iles dir_ectory ty_pe e_dit cdate da_te t_est 
 * clu_ster 
 */
 { "ps_eudonym",respond, },
 { "list",      check, },
 { "index",     show_conf_index, },
#ifdef WWW
#ifdef INCLUDE_EXTRA_COMMANDS
 { "authenticate", authenticate, },
#endif
#endif
 { "cfcreate",  cfcreate, },
 { "cfdelete",  cfdelete, },
 { 0, 0 },
};

/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE MISC. COMMAND FUNCTION                     */
/******************************************************************************/
char                       /* RETURNS: 0 to quit, 1 else */
ok_cmd_dispatch(argc,argv) /* ARGUMENTS:                 */
int    argc;               /*    Number of arguments     */
char **argv;               /*    Argument list           */
{
   int i;

   for (i=0; ok_cmd[i].name; i++)
      if (match(argv[0],ok_cmd[i].name))
         return ok_cmd[i].func(argc,argv);

   /* Command dispatch */
   if (match(argv[0],"j_oin"))    {
      if (argc==2) join(argv[1],0, 0); 
      else if (confidx>=0) {
         confsep(expand("joinmsg", DM_VAR),confidx,&st_glob,part,0); 
      } else 
         printf("Not in a %s!\n", conference(0));
   } else return misc_cmd_dispatch(argc,argv);
   return 1;
}

/******************************************************************************/
/* PROCESS A GENERIC SIGNAL (if enabled)                                      */
/******************************************************************************/
RETSIGTYPE 
handle_other(sig, code, scp, addr)
int sig, code;
/* struct sigcontext */ void *scp;
char *addr;
{
   if (status & S_PIPE) printf("%d Pipe interrupt %d!\n",getpid(),sig);
   else printf("%d Interrupt %d!\n",getpid(),sig);
   status |= S_INT;
}

/******************************************************************************/
/* PROCESS A USER INTERRUPT SIGNAL                                            */
/******************************************************************************/
RETSIGTYPE         /* RETURNS: (nothing) */
#ifdef SIG_RESTART
handle_int(sig,code,scp)
int sig, code;
struct sigcontext *scp;
#else
handle_int(sig) /* ARGUMENTS: (none)  */
   int sig;
#endif
{
   if (!(status & S_PIPE)) {
      printf("Interrupt!\n");
      status |= S_INT;
   }
   signal(SIGINT,  handle_int);

#ifdef SIG_RESTART
   scp->sc_syscall_action=(int_on)?SIG_RETURN:SIG_RESTART;
#endif
   /* ints_on(); why?  took this out on 3/25/94 */
}

/******************************************************************************/
/* PROCESS AN INTERRUPT CAUSED BY A PIPE ABORTING                             */
/******************************************************************************/
RETSIGTYPE          /* RETURNS: (nothing) */
#ifdef SIG_RESTART
handle_pipe(sig,code,scp)
int sig, code;
struct sigcontext *scp;
#else
handle_pipe(sig) /* ARGUMENTS: (none)  */
int sig;
#endif
{
/*fprintf(stderr, "code=%d\n", code); fflush(stderr);*/
   if (status & S_PAGER)
      printf("Pipe interrupt?\n");
   /* spclose(st_glob.outp );*/
   signal(SIGPIPE, handle_pipe);
   status |= S_INT;
#ifdef SIG_RESTART
   scp->sc_syscall_action=(int_on)?SIG_RETURN:SIG_RESTART;
#endif
}
