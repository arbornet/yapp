/* $Id: misc.c,v 1.21 1998/02/10 11:36:16 kaylene Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h> /* for umask() */
#include <time.h>
#include <ctype.h>
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>  /* for X_OK */
#endif
#ifdef HAVE_SYS_FILE_H
# include <sys/file.h> /* for X_OK */
#endif
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "misc.h"
#include "change.h"
#include "help.h"
#include "system.h"
#include "macro.h"
#include "range.h"
#include "item.h"
#include "sum.h"
#include "sep.h"
#include "files.h"
#include "arch.h"
#include "driver.h"  /* for open_pipe */
#include "lib.h" /* for noquote() */
#include "user.h" /* for do_cflist */
#include "www.h" /* for www_parse_post */
#include "xalloc.h" /* for xalloc() */
#include "edbuf.h" /* for text_loop() */
#include "main.h" /* for wfputs() */
#include "log.h" /* for logevent() */

/* Misc. commands available at all modes */
static dispatch_t misc_cmd[]={
 { "chfn",      chfn, },
 { "newuser",   newuser, },
 { "passw_d",   passwd, },
 { "arg_set",   argset, },
 { "www_parsepost", www_parse_post, },
 { "url_encode", url_encode, },
 { "if",        do_if, },
 { "else",      do_else, },
 { "endif",     do_endif, },
 { "for_each",  foreach, },
 { "c_hange",   change, },
 { "se_t",      change, },
 { "?",         help, },
 { "h_elp",     help, },
 { "exp_lain",  help, },
 { "al_ias",    define, },
 { "def_ine",   define, },
 { "una_lias",  define, },
 { "und_efine", define, },
 { "const_ant", define, },
 { "log",       logevent, },
 { "ec_ho",     echo, },
 { "echoe",     echo, },
 { "echon",     echo, },
 { "echoen",    echo, },
 { "echone",    echo, },
 { "m_ail",     mail, },
 { "t_ransmit", mail, },
 { "sen_dmail", mail, },
 { "d_isplay",  display, },
 { "que_ry",    display, },
 { "sh_ow",     display, },
 { "load",      load_values, },
 { "save",      save_values, },
 { "t_est",     test, },
 { "rm",        do_rm, },
 { "cd",        cd, },
 { "chd_ir",    cd, },
 { "uma_sk",    do_umask, },
 { "cdate",     date, },
 { "da_te",     date, },
/* "clu_ster",  cluster,  */
#ifdef INCLUDE_EXTRA_COMMANDS
 { "cfdir",     set_cfdir, },
#endif
 { "eval_uate", eval, },
 { "evali_n",   eval, },
 { "source",    do_source, },
 { "debug",     set_debug, },
 { "cf_list",   do_cflist, },
/* ex_it q_uit st_op good_bye log_off log_out h_elp exp_lain sy_stem unix al_ias
 * def_ine una_lias und_efine ec_ho echoe echon echoen echone so_urce m_ail 
 * t_ransmit sen_dmail chat write d_isplay que_ry p_articipants w_hoison 
 * am_superuser resign chd_ir uma_sk sh_ell f_iles dir_ectory ty_pe e_dit 
 * cdate da_te t_est clu_ster 
 */
 { 0, 0 },
};

/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE MISC. COMMAND FUNCTION                     */
/******************************************************************************/
char                         /* RETURNS: 0 to quit, 1 else */
misc_cmd_dispatch(argc,argv) /* ARGUMENTS:                 */
int    argc;                 /*    Number of arguments     */
char **argv;                 /*    Argument list           */
{
   int i;
   char * ptr=NULL;  /* Hold arbitrary length string from noquote */

   for (i=0; misc_cmd[i].name; i++)
      if (match(argv[0],misc_cmd[i].name))
         return misc_cmd[i].func(argc,argv);

   /* Command dispatch */
   if (match(argv[0],"q_uit")     
    || match(argv[0],"st_op")
    || match(argv[0],"ex_it")) {
      status |= S_STOP;
      return 0;
   } else if (match(argv[0],"unix_cmd")) {
      char *buff, i;
      int buflen;

      if (argc<2) printf("syntax: unix_cmd \"command\"\n");
      else {
         /* Calculate max buflen */
         buflen = strlen(argv[1])+1;
         for (i=2; i<argc; i++)
            buflen += 1+strlen(argv[i]);

         buff = (char *)xalloc(0, buflen);
         /* implode(buff,argv," ",1); */
         ptr = noquote(argv[1],0);
         strcpy(buff,ptr);
         xfree_string(ptr);
         for (i=2; i<argc; i++) {
            strcat(buff," ");
            ptr = noquote(argv[i],0);
            strcat(buff,ptr);
            xfree_string(ptr);
         }
/* Undone at request of sno and jep
 *       if (mode==M_SANE) printf("%s rc cannot exec: %s\n",conference(1),buff);
 *       else              
 */
         unix_cmd(buff);
         xfree_string(buff);
      }
   } else if (argc) {
      char *p;

      /* Check for commands of the form: variable=value */
      p = strchr(argv[0], '=');
      if (p || (argc>1 && argv[1][0]=='=')) {
         char *val; /* Arbitrary length value */
         int i, vallen;

         /* Compute max vallen */
         if (p) {
            vallen = strlen(p+1)+1;
            i=1;
         } else {
            i=2;
            vallen = strlen(argv[1]+1)+1;
         }
         while (i<argc) {
            if (vallen>1)
               vallen++;
            vallen += strlen(argv[i++]);
         }

         /* Compose val */
         val = (char *)xalloc(0, vallen);
         if (p) {
            *p = '\0';
            strcpy(val, p+1);
            i=1;
         } else {
            strcpy(val, argv[1]+1);
            i=2;
         }
         while (i<argc) {
            if (val[0])
               strcat(val, " ");
            strcat(val, argv[i++]);
         }

         /* Execute command */
         def_macro(argv[0], DM_VAR, val);
         xfree_string(val);
         
      } else {
         printf("Invalid command: %s\n",argv[0]); 
      }
   }
   return 1;
}

/******************************************************************************/
/* SET UMASK VALUE                                                            */
/******************************************************************************/
int                 /* RETURNS: (nothing)     */
do_umask(argc,argv) /* ARGUMENTS:             */
int    argc;        /*    Number of arguments */
char **argv;        /*    Argument list       */
{
   int i;

   if (argc<2) {
      umask(i=umask(0));
      printf("%03o\n",i);
   } else if (!isdigit(argv[1][0])) {
      printf("Bad umask \"%s\"specified (must be octal)\n",argv[1]);
   } else {
      sscanf(argv[1],"%o",&i);
      umask(i);
   }
   return 1;
}

/******************************************************************************/
/* SEND MAIL TO ANOTHER USER                                                  */
/******************************************************************************/
int            /* RETURNS: (nothing)     */
mail(argc,argv) /* ARGUMENTS:             */
int    argc;    /*    Number of arguments */
char **argv;    /*    Argument list       */
{
   char buff[MAX_LINE_LENGTH],buff2[MAX_LINE_LENGTH];

   if (argc<2) {
      unix_cmd("mail");
   } else if (flags & O_MAILTEXT) {
      sprintf(buff,"mail %s",argv[1]);
      unix_cmd(buff);
   } else {
      if (text_loop(0,"mail")) { /* dont clear buffer, for reply cmd */
         strcpy(buff,argv[1]);
         while (strlen(buff)) {
            sprintf(buff2,"mail %s < %s/cf.buffer",buff,work);
            unix_cmd(buff2);
            printf("Mail sent to %s.\n",buff);
            if (!(flags & O_QUIET))
               printf("More recipients (or <return>)? ");
            ngets(buff, st_glob.inp);
         }
      }
      sprintf(buff,"%s/cf.buffer",work);
      rm(buff,SL_USER);
   }
   return 1;
}

int            /* RETURNS: (nothing)     */
do_rm(argc,argv)  /* ARGUMENTS:             */
   int    argc;   /*    Number of arguments */
   char **argv;   /*    Argument list       */
{  
   int i;

   if (argc<2) {
      printf("Usage: rm filename ...\n");
      return 2; 
   } 
   for (i=1; i<argc; i++) {
      if (rm(argv[i], SL_USER)) {
         if (!(flags & O_QUIET))
            error("removing ",argv[1]);
      }
   }
   return 1;    
}

/******************************************************************************/
/* CHANGE CURRENT WORKING DIRECTORY                                           */
/******************************************************************************/
int            /* RETURNS: (nothing)     */
cd(argc,argv)  /* ARGUMENTS:             */
int    argc;   /*    Number of arguments */
char **argv;   /*    Argument list       */
{
   if (argc>2) {
      printf("Bad parameters near \"%s\"\n",argv[2]);
      return 2;
   } else if (chdir((argc>1)?argv[1]:home))
      error("cd'ing to ",argv[1]);
   return 1;
}

/******************************************************************************/
/* ECHO ARGUMENTS TO OUTPUT                                                   */
/******************************************************************************/
int            /* RETURNS: (nothing)     */
echo(argc,argv) /* ARGUMENTS:             */
int    argc;    /*    Number of arguments */
char **argv;    /*    Argument list       */
{
   short i;
   FILE *fp;

   /* If `echo...` but not `echo...|cmd`  (REDIRECT bit takes precedence) */
   if ((status & S_EXECUTE) && !(status & S_REDIRECT)) {
      fp = NULL;
   } else if (match(argv[0],"echoe") || match(argv[0],"echoen") 
    || match(argv[0],"echone")) {
      fp = stderr;
   } else {
      if (status & S_REDIRECT) {
         fp = stdout;
      } else {
         open_pipe();
         fp = st_glob.outp;
         if (!fp) {
            fp = stdout;
         }
      }
   }

   for (i=1; i<argc; i++) {
      wfputs(argv[i],fp);
      if (i+1 < argc)
         wfputc(' ',fp);
   }
   if (!strchr(argv[0],'n'))
      wfputc('\n',fp);
   
   if (fp)
      fflush(fp); /* flush when done with wfput stuff */
   return 1;
}

/******************************************************************************/
/* SAVE VALUES                                                                */
/******************************************************************************/
int                    /* RETURNS: (nothing)     */
load_values(argc,argv) /* ARGUMENTS:             */ 
int    argc;           /*    Number of arguments */ 
char **argv;           /*    Argument list       */ 
{
   register int i;
   char buff[MAX_LINE_LENGTH];
   int suid;
   char *userfile = get_userfile(login, &suid);

   for (i=1; i<argc; i++) {
#ifdef HAVE_DBM_OPEN
      strcpy(buff, get_dbm(userfile, argv[i], SL_USER));
#endif
      if (buff[0])
         def_macro(argv[i], DM_VAR, buff);
      else
         undef_name(argv[i]);
   }
   return 1;
}

/******************************************************************************/
/* SAVE VALUES                                                                */
/******************************************************************************/
int                    /* RETURNS: (nothing)     */
save_values(argc,argv) /* ARGUMENTS:             */
int    argc;           /*    Number of arguments */   
char **argv;           /*    Argument list       */
{
   register int i;
   int suid;
   char *userfile = get_userfile(login, &suid);
   char buff[MAX_LINE_LENGTH];
#ifdef HAVE_DBM_OPEN

   for (i=1; i<argc; i++) {
      char *p = expand(argv[i], DM_VAR);
      if (p && p[0] && !save_dbm(userfile, argv[i], p, SL_USER)) {
         error("modifying userfile ", userfile);
         return 1;
      }
   }
#endif
   return 1;
}

/******************************************************************************/
/* CHECK THE DATE                                                             */
/******************************************************************************/
int            /* RETURNS: (nothing)     */
date(argc,argv) /* ARGUMENTS:             */
int    argc;    /*    Number of arguments */
char **argv;    /*    Argument list       */
{
   short i;
   time_t t;

   i=0;
   t = since(argc,argv,&i);
   if (t<LONG_MAX) {
      if (argv[0][0]=='c')
         printf("%X\n",t); /* cdate command */
      else
         printf("\nDate is: %s\n",get_date(t,13));
   }
   return 1;
}

/******************************************************************************/
/* SOURCE A FILE OF COMMANDS                                                  */
/******************************************************************************/
int             /* RETURNS: (nothing)     */
do_source(argc,argv) /* ARGUMENTS:             */
int    argc;    /*    Number of arguments */
char **argv;    /*    Argument list       */
{
   short i;
   int   ok=1;
   char  arg[10];

   if (argc<2 || argc>20) 
      printf("usage: source filename [arg ...]\n");
   else {
      for (i=1; i<argc; i++) {
         sprintf(arg,"arg%d",i-1);
         def_macro(arg,DM_VAR,argv[i]);
      }
      
      ok = source(argv[1],NIL, 0, SL_USER);
      if (!ok && !(flags & O_QUIET))
         printf("Cannot access %s\n",argv[1]);

      for (i=1; i<argc; i++) {
         sprintf(arg,"arg%d",i-1);
         if (find_macro(arg, DM_VAR))
            undef_name(arg);
      }
   }
   return (ok<0)? 0 : 1;
}

/******************************************************************************/
/* TEST RANGES                                                                */
/******************************************************************************/
int             /* RETURNS: (nothing)     */
test(argc,argv) /* ARGUMENTS:             */
int    argc;    /*    Number of arguments */
char **argv;    /*    Argument list       */
{
   int i;
   char act[MAX_ITEMS];
   short j,k=0,fl=0;

   rangeinit(&st_glob,act);
   refresh_sum(0,confidx,sum,part,&st_glob);

   if (argc>1) /* Process argc */
      range(argc,argv,&fl,act,sum,&st_glob,0);
   if (!(fl & OF_RANGE)) {
      printf("Error, no %s specified! (try HELP RANGE)\n", topic(0));
      return 1;
   }

   j = A_SKIP;
   for (i=0; i<MAX_ITEMS; i++) {
      if (act[i] == A_SKIP && j==A_COVER) printf("%d]",i);
      if (act[i] == A_FORCE) printf("%s%d",(k++)?",":"",i+1);
      if (act[i] == A_COVER && j!=A_COVER) printf("%s[%d-",(k++)?",":"",i+1);
      j = act[i];
   }
   if (j==A_COVER) printf("%d]",i);
   printf(".\n");
   printf("newflag: %d\n",fl);
   printf("since  date is %s",ctime(&(st_glob.since)));
   printf("before date is %s",ctime(&(st_glob.before)));
   if (st_glob.string[0]) printf("String is: %s\n",st_glob.string);
   if (st_glob.author[0]) printf("Author is: %s\n",st_glob.author);
 
   return 1;
}

int               /* RETURNS: (nothing)     */
set_debug(argc,argv) /* ARGUMENTS:             */
int    argc;      /*    Number of arguments */
char **argv;      /*    Argument list       */
{
   int i,j;

   if (argc<2) {
      for (i=0; debug_opt[i].name; i++)
         printf("%s %s\n", compress(debug_opt[i].name), 
          (debug & debug_opt[i].mask)? "on":"off");
   } else {

   for (j=1; j<argc; j++) {
      if (!strcmp(argv[j], "off")) 
         debug = 0;
      else
         for (i=0; debug_opt[i].name; i++)
            if (match(argv[j], debug_opt[i].name)) {
                debug ^=  debug_opt[i].mask;
                printf("%s %s\n", compress(debug_opt[i].name), 
                 (debug & debug_opt[i].mask)? "on":"off");
            }
   }
   }
   return 1;
}

#ifdef INCLUDE_EXTRA_COMMANDS
int               /* RETURNS: (nothing)     */
set_cfdir(argc,argv) /* ARGUMENTS:             */
int    argc;      /*    Number of arguments */
char **argv;      /*    Argument list       */
{
   char buff[MAX_LINE_LENGTH],cmd[MAX_LINE_LENGTH];

   if (!(flags & O_QUIET))
      printf("User name: ");
   ngets(buff, st_glob.inp);
   sprintf(cmd,"%s/%s",home,buff);
   if (access(cmd,X_OK)) 
      printf("No such directory.\n");
   else
      strcpy(work,cmd);
   return 1;
}
#endif

/* PROCESS A NEW SEP (of arbitrary length) */
int               /* RETURNS: (nothing)     */
eval(argc,argv)   /* ARGUMENTS:             */
   int    argc;   /*    Number of arguments */
   char **argv;   /*    Argument list       */
{
   int i;
   char act[MAX_ITEMS], buff[MAX_LINE_LENGTH], *string;
   short      i_s,i_i, i_lastseen, shown=0, rfirst=0;
   FILE      *fp;
   status_t tmp;

   /* Save state */
   memcpy(&tmp, &st_glob, sizeof(status_t));

   refresh_sum(0,confidx,sum,part,&st_glob);
   for (i=0; i<MAX_ITEMS; i++) act[i]=0;
   st_glob.string[0] ='\0';
   st_glob.since     = st_glob.before = st_glob.r_first = 0;
   st_glob.opt_flags = 0;

   if (argc<2) {

      /* Read from stdin until EOF */
      while ((string = xgets(st_glob.inp, stdin_stack_top)) != NULL) {
         if (mode==M_OK || mode==M_JOQ)
            confsep(string, confidx,&st_glob,part,0);
         else if (mode==M_RFP || mode==M_EDB || mode==M_TEXT)
            itemsep(string, 0);
         else
            printf("Unknown mode\n");
         xfree_string(string);
      }
      if (debug & DB_IOREDIR) 
         printf("Detected end of eval input\n");
      return 1;
   } 

   if (match(argv[0], "evali_n")) {
      string = NULL;
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   } else {
      string = argv[argc-1];

      /* Strip quotes */
      if (string[0]=='"') {
         string++;
         if (string[ strlen(string)-1 ]=='"')
             string[ strlen(string)-1 ]='\0';
      }
   
      if (argc>2) /* Process argc */
         range(argc-1,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   }

   open_pipe();

   /* Transfer current pipe info to global saved state */
   tmp.outp = st_glob.outp;

   if (string && !(st_glob.opt_flags & OF_RANGE)) {
 
      if (mode==M_OK || mode==M_JOQ)
         confsep(string, confidx,&st_glob,part,0);
      else if (mode==M_RFP || mode==M_EDB || mode==M_TEXT)
         itemsep(string, 0);
      else
         printf("Unknown mode\n");

   } else {

      if (mode==M_OK) {

/* Removed 4/11/96 because eval 4 'christmas' "%{nextitem}"
 * wasn't processing the search string.
 *       st_glob.string[0]='\0';
 */
         if (st_glob.opt_flags & OF_REVERSE) {
            i_s = st_glob.i_last;
            i_i = -1;
         } else {
            i_s = st_glob.i_first;
            i_i = 1;
         }
 
         /* Process items */
         sepinit(IS_START); fp = NULL;
         for (st_glob.i_current = i_s;
                  st_glob.i_current >= st_glob.i_first
               && st_glob.i_current <= st_glob.i_last
               && !(status & S_INT);
              st_glob.i_current += i_i) {
            if (cover(st_glob.i_current,confidx,st_glob.opt_flags,
             act[st_glob.i_current-1],sum, part, &st_glob)) {
               st_glob.i_next = nextitem(1);
               st_glob.i_prev = nextitem(-1);
               st_glob.r_first = rfirst;

               if (match(argv[0], "evali_n")) {
                  while ((string=xgets(st_glob.inp, stdin_stack_top)) != NULL) {
                     itemsep(string, 0);
                     xfree_string(string);
                  }
               } else
                  itemsep(string, 0);
               shown++;
            }
         }
         if (!shown && (st_glob.opt_flags & (OF_BRANDNEW|OF_NEWRESP))) {
            sprintf(buff, "No new %ss matched.\n", topic(0));
            wputs(buff);
         }

      } else if (mode==M_RFP) {
         /* Open file */
         sprintf(buff,"%s/_%d",conflist[confidx].location,st_glob.i_current);
         if (!(fp=mopen(buff,O_R))) return 1;

         i_lastseen = st_glob.i_current-1;
         if (st_glob.opt_flags & (OF_NEWRESP|OF_NORESPONSE))
            st_glob.r_first = part[i_lastseen].nr;
         else if (st_glob.since) {
            st_glob.r_first=0;
            while (st_glob.since > re[st_glob.r_first].date) {
               st_glob.r_first++;
               get_resp(fp,&(re[st_glob.r_first]),(short)GR_HEADER,st_glob.r_first);
               if (st_glob.r_first>=sum[i_lastseen].nr) break;
            }
         }
         st_glob.r_last = MAX_RESPONSES;
         st_glob.r_max=sum[i_lastseen].nr-1;

         /* For each response */
         for (st_glob.r_current = st_glob.r_first;
              st_glob.r_current<= st_glob.r_last
               && st_glob.r_current<=st_glob.r_max
               && !(status & S_INT);
              st_glob.r_current++) {
            get_resp(fp,&(re[st_glob.r_current]),(short)GR_HEADER,st_glob.r_current);
            itemsep(string, 0);
         }

         mclose(fp);
      } else
         printf("bad mode\n");
   }

   /* Restore state */
{
   FILE *inp = st_glob.inp;
   memcpy(&st_glob, &tmp, sizeof(status_t));
   st_glob.inp = inp;
}
/*
printf("eval: fdnext=%d\n", fdnext());
if (!fdnext())
   abort();
*/

   return 1;
}

#if 0
/* CHANGE CLUSTER */
int               /* RETURNS: (nothing)     */
cluster(argc,argv) /* ARGUMENTS:             */
int    argc;      /*    Number of arguments */
char **argv;      /*    Argument list       */
{
   leave(0,(char**)0);
   open_cluster(argv[1],HELPDIR);
   return 1;
}
#endif

/*****************************************************************************/
/* FUNCTIONS FOR CONDITIONAL EXPRESSIONS                                     */
/*****************************************************************************/
static int if_stat[100], if_depth = -1;

int
test_if() 
{
   if (if_depth<0)
      if_stat[++if_depth]=1;
   return if_stat[if_depth];
}

int                  /* RETURNS: (nothing)     */
do_if(argc,argv)     /* ARGUMENTS:             */
int    argc;         /*    Number of arguments */
char **argv;         /*    Argument list       */
{
   char buff[MAX_LINE_LENGTH], *sp=buff;
   int i;

   if (!test_if()) {
      if_stat[++if_depth]=0;
      return 1;
   }

   /* Put buffer together */
   buff[0] = '\0';
   for (i=1; i<argc; i++) 
      strcat(buff, argv[i]);

/*printf("if buffer='%s'\n", buff);*/

   if (if_depth==99) {
      printf("Too many nested if's\n");
      return 0;
   }

   init_show();
   switch(mode) {
   case M_RFP: 
   case M_TEXT: 
   case M_EDB:
      if_stat[++if_depth]=itemcond(&sp, st_glob.opt_flags); 
      break;

   case M_OK:   
   case M_JOQ: 
   default:
      if_stat[++if_depth]=confcond(&sp, confidx, &st_glob); 
      break;
   }

   return 1;
}

int                  /* RETURNS: (nothing)     */
do_endif(argc,argv)  /* ARGUMENTS:             */
int    argc;         /*    Number of arguments */
char **argv;         /*    Argument list       */
{
   if (if_depth)
      if_depth--;
   else
      printf("Parse error: endif outside if construct\n");
   return 1;
}

int                  /* RETURNS: (nothing)     */
do_else(argc,argv)   /* ARGUMENTS:             */
int    argc;         /*    Number of arguments */
char **argv;         /*    Argument list       */
{
   if (!if_depth)
      printf("Parse error: else outside if construct\n");
   else if (if_stat[if_depth-1]) 
      if_stat[if_depth]= !if_stat[if_depth];
   return 1;
}

int                  /* RETURNS: (nothing)     */
foreach(argc,argv)   /* ARGUMENTS:             */
   int    argc;      /*    Number of arguments */
   char **argv;      /*    Argument list       */
{
   char **list;
   int i, num;

   if (argc!=6) {
      printf("usage: foreach <varname> in <listvar> do \"command\"\n");
      return 1;
   }
   
   /* Get list */
   list = explode(expand(argv[3], DM_VAR), ", ", 1);
   num = xsizeof(list);

   for (i=0; i<num; i++) {
      def_macro(argv[1], DM_VAR, list[i]);
      command(argv[5], 1);
   }

   xfree_array(list);
   return 1;
}

int
argset(argc,argv)   /* ARGUMENTS:             */
int    argc;         /*    Number of arguments */
char **argv;         /*    Argument list       */
{
   char **fields;
   int i;
   char arg[20];

   if (argc<3) {
      /* printf("usage: argset delimeter string\n"); */
      def_macro("argc", DM_VAR, "0");
   } else {
      fields = explode(argv[2], argv[1], 0);
/*printf("pi=!%s! fields=%d\n", argv[2], xsizeof(fields));*/
      for (i=0; i<xsizeof(fields); i++) {
         sprintf(arg,"arg%d",i+1);
         def_macro(arg, DM_VAR, fields[i]);
      }
      sprintf(arg,"%d", xsizeof(fields));
      def_macro("argc", DM_VAR, arg);
      xfree_array(fields);
   }
   return 1;
}
