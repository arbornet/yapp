/* $Id: rfp.c,v 1.18 1997/08/28 00:07:49 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/wait.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/stat.h>
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "rfp.h"
#include "item.h"
#include "macro.h"
#include "driver.h"
#include "lib.h"
#include "sum.h"
#include "xalloc.h"
#include "arch.h"
#include "system.h"
#include "files.h"
#include "range.h"
#include "sep.h"
#include "news.h"
#include "stats.h" /* for get_config */
#include "license.h" /* for get_conf_param() */
#include "main.h" /* for wputs() */
#include "misc.h" /* for misc_cmd_dispatch() */
#include "edbuf.h" /* for text_loop() */
#include "user.h"  /* for get_nobody_uid() */
#include "edit.h"  /* for retitle() */
#include "conf.h"  /* for fullname_in_conference() */
#include "security.h"/* for check_acl() */

extern FILE *ext_fp;
extern short ext_first,ext_last;

/* Commands available in RFP mode */
static dispatch_t rfp_cmd[]={
 { "edit",       modify, },
 { "cen_sor",    censor, },
 { "scr_ibble",  censor, },
 { "uncen_sor",  uncensor, },
 { "e_nter",     enter, },
 { "pr_eserve",  preserve, },
 { "po_stpone",  preserve, },
 { "hide",       preserve, },
 { "p_ass",      preserve, },
 { "n_ew",       preserve, },
 { "wait",       preserve, },
#ifdef INCLUDE_EXTRA_COMMANDS
 { "tree",       tree, },
#endif
 { "*freeze",    freeze, },
 { "*thaw",      freeze, },
 { "*forget",    freeze, },
 { "*retire",    freeze, },
 { "*unretire",  freeze, },
 { "r_espond",   rfp_respond, },
 { "ps_eudonym", rfp_respond, },
 { "*retitle",   retitle, },
 { "*rem_ember", remember, },
 { "*unfor_get", remember, },
 { "*kill",      do_kill, },
 { "*f_ind",     do_find, },
 { "*lo_cate",   do_find, },
/* "*fix_seen",  fixseen,  this by itself doesn't work */
 { "reply",      reply, },
 { 0, 0 },
};

/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE RFP MODE FUNCTION                          */
/******************************************************************************/
char                        /* RETURNS: 0 on abort, 1 else */
rfp_cmd_dispatch(argc,argv) /* ARGUMENTS:                  */
int    argc;                /*    Number of arguments      */
char **argv;                /*    Argument list            */
{
   short a,b,c;
   int i,j;

   for (i=0; rfp_cmd[i].name; i++) {
      if (match(argv[0],rfp_cmd[i].name))
         return rfp_cmd[i].func(argc,argv);
      else if (rfp_cmd[i].name[0]=='*' && match(argv[0],rfp_cmd[i].name+1)) {
         char buff[MAX_LINE_LENGTH];

         mode = M_OK; 
         st_glob.r_first = st_glob.r_last+1;
         sprintf(buff,"%s %d",compress(rfp_cmd[i].name+1),st_glob.i_current);
         for (j=1; j<argc; j++) {
            strcat(buff," ");
            strcat(buff,argv[j]);
         }
         return command(buff,0);
      }
   }

   /* Command dispatch */
   if (match(argv[0],"h_eader")) {
      show_header();
   } else if (match(argv[0],"text"))     { /* re-read from 0 */
      st_glob.r_first = 0;
      st_glob.r_last  = MAX_RESPONSES;
      show_range();
   } else if (match(argv[0],"a_gain"))   { /* re-read same range */
      sepinit(IS_ITEM);
      if (flags & O_LABEL)
         sepinit(IS_ALL);
      show_header();
      st_glob.r_first = ext_first;
      st_glob.r_last  = ext_last;
      if (st_glob.r_first>0)
         show_nsep(ext_fp); /* nsep between header and responses */
      show_range();
   } else if (match(argv[0],"^")
    ||        match(argv[0],"fi_rst"))   {
      st_glob.r_first = 1;
      st_glob.r_last  = MAX_RESPONSES;
      show_range();
   } else if (match(argv[0],".")
    ||        match(argv[0],"th_is")
    ||        match(argv[0],"cu_rrent")) {
      st_glob.r_first = st_glob.r_current;
      st_glob.r_last  = MAX_RESPONSES;
      show_range();
   } else if (match(argv[0],"$")
    ||        match(argv[0],"l_ast"))  {
      st_glob.r_first = st_glob.r_max;
      st_glob.r_last  = MAX_RESPONSES;
      show_range();
   }
#ifdef INCLUDE_EXTRA_COMMANDS
   else if (match(argv[0],"up")
    ||        match(argv[0],"par_ent")) {
      short a;

      if ((a=parent(st_glob.r_current)) < 0)
         printf("No previous response\n");
      else {
         st_glob.r_first = a;
         st_glob.r_last  = a;
         show_range();
      }
   } else if (match(argv[0],"chi_ldren")
     ||       match(argv[0],"do_wn")) {
      short a;

      /* Find 1st child */
      if ((a = child(st_glob.r_current)) < 0)
         printf("No children\n");
      else {
         st_glob.r_first = a;
         st_glob.r_last  = a;
         show_range();
      }
   } else if (match(argv[0],"sib_ling")
    ||        match(argv[0],"ri_ght")) {
      short a;

      /* Find next sibling */
      if ((a = sibling(st_glob.r_current)) < 0)
         printf("No more siblings\n");
      else {
         st_glob.r_first = a;
         st_glob.r_last  = a;
         show_range();
      }
   } else if (match(argv[0],"sync_hronous"))   { mode = M_OK; /* KKK */ }
   else   if (match(argv[0],"async_hronous"))  { mode = M_OK; /* KKK */ }
#endif
   else if (match(argv[0],"si_nce"))         {   
      time_t t;
      short i;

      i=0;
      t=since(argc,argv,&i);
      for (i=st_glob.r_max; i>=0; i--) {
         if (!re[i].date) get_resp(ext_fp,&(re[i]),(short)GR_HEADER,i);
         if (re[i].date<t) break;
      }
      st_glob.r_first = i+1;
      st_glob.r_last  = MAX_RESPONSES;
      show_range();
   } else if (match(argv[0],"onl_y"))          { 
      if (argc>2) {
         printf("Bad parameters near \"%s\"\n",argv[2]);
         return 2;
      } else if (argc>1 && sscanf(argv[1],"%hd",&a)==1) {
         int prev_opt_flags = st_glob.opt_flags;
         st_glob.r_first = a;
         st_glob.r_last  = a;
         st_glob.opt_flags |= OF_NOFORGET; /* force display of hidden resp */
         show_range();
         st_glob.opt_flags = prev_opt_flags;
      } else
         wputs("You must specify a comment number\n");
   } else if (argc && sscanf(argv[0],"%hd-%hd",&a,&b)==2) {
      if (b<a) { c=a; a=b; b=c; }
      if (a<0) {
         printf("Response #%d is too small\n",a);
      } else if (b>st_glob.r_max) {
         printf("Response #%d is too big (last %d)\n",a,st_glob.r_max);
      } else {
         st_glob.r_first = a;
         st_glob.r_last  = b;
         show_range();
      }
   } else if (argc && (sscanf(argv[0],"%hd",&a)==1
    || !strcmp(argv[0],"-") || !strcmp(argv[0],"+"))) {
      if (!strcmp(argv[0],"-")) a = -1; 
      if (!strcmp(argv[0],"+")) a =  1;
      if (argv[0][0]=='+' || argv[0][0]=='-')
         a += st_glob.r_current;
      if (a<0) {
         printf("Response #%d is too small\n",a);
      } else if (a>st_glob.r_max) {
         printf("Response #%d is too big (last %d)\n",a,st_glob.r_max);
      } else {
         st_glob.r_first = a;
         st_glob.r_last  = MAX_RESPONSES;
         show_range();
      }
   } else {
      a=misc_cmd_dispatch(argc,argv);
      if (!a) preserve(argc,argv);
      return a;
   }
   return 1;
}

/******************************************************************************/
/* ADD A NEW RESPONSE                                                         */
/******************************************************************************/
void
add_response(this,text,idx,sum,part,stt,art,mid,uid,login,fullname,resp)
sumentry_t  *this; /*   New item summary */
char       **text; /*   New item text    */
short        idx;
sumentry_t  *sum;
partentry_t *part;
status_t    *stt;
long         art;
char        *mid;
int          uid;
char        *login;
char        *fullname;
short        resp;
{
   short item,line,nr;
   char  buff2[MAX_LINE_LENGTH];
   FILE *fp;

   item = stt->i_current;
   nr = sum[ item-1 ].nr;
   sprintf(buff2, "%s/_%d", conflist[idx].location, item);

   /* Prevent duplicate responses: */
   if ((fp=mopen(buff2, O_R)) != NULL) {
      int prev, nl_prev, nl_new, dup;

      prev = sum[item-1].nr-1;
      get_resp(fp, &re[prev], (short)GR_ALL, sum[item-1].nr-1);
      mclose(fp);
      nl_prev = xsizeof(re[prev].text);
      nl_new  = xsizeof(text);
      dup = (nl_prev == nl_new && !strcmp(login,re[prev].login));
      for (line=0; dup && line<nl_new; line++) 
         dup = !strcmp(text[line], re[prev].text[line]);
      xfree_array(re[prev].text);
      if (dup) {
         if (!(flags & O_QUIET))
            printf("Duplicate response aborted\n");
         return;
      }
   }

   /* Begin critical section */
   if ((fp=mopen(buff2, O_A|O_NOCREATE)) != NULL) {
      short n;

      /* was: update before open, in case of a link - why? (was wrong) */
      if (!art)
         refresh_sum(item,idx,sum,part,stt);

      n = sum[item-1].nr - nr;
      if (n>1) {
         printf("Warning: %d comments slipped in ahead of yours at %d-%d!\n",
          n,nr,sum[item-1].nr-1);
      } else if (n==1) {
         printf("Warning: a comment slipped in ahead of yours at %d!\n",
          sum[item-1].nr-1);
      } else if (!(flags & O_STAY))
         stt->r_last = -1; 

      re[sum[item-1].nr].offset   = ftell(fp);
      fprintf(fp,",R%04X\n,U%d,%s\n,A%s\n,D%08X\n",
         RF_NORMAL,uid,login,fullname,(art)? this->last : time((time_t *)0));
      if (resp)
         fprintf(fp,",P%d\n",resp-1);
      fprintf(fp,",T\n");
      re[sum[item-1].nr].parent   = resp;
      re[sum[item-1].nr].textoff  = ftell(fp);
      re[sum[item-1].nr].numchars = -1;
      if (art) {
         fprintf(fp,",N%06ld\n",art);
         fprintf(fp,",M%s\n",mid);
      } else {
         for (line=0; line<xsizeof(text); line++) {
            if (text[line][0]==',') fprintf(fp,",,%s\n",text[line]);
            else                    fprintf(fp,"%s\n",  text[line]);
         }
      }
      if (fprintf(fp,",E%s\n", spaces(atoi(get_conf_param("padding",PADDING))))>=0) {

         /* Update seen */
         stt->r_current = stt->r_max = sum[item-1].nr;
   /*    if (!(flags & O_METOO) && stt->r_lastseen==sum[item-1].nr)  */
         time(&(part[item-1].last));
         if (!(flags & O_METOO)) {
            part[item-1].nr    = sum[item-1].nr;
            stt->r_lastseen = stt->r_current+1;
         }
         dirty_part(item-1);

         sum[item-1].last  = time((time_t *)0);
         sum[item-1].nr++;
         save_sum(sum,(short)(item-1),idx,stt);
         dirty_sum(item-1);

         skip_new_response(confidx, item-1, sum[item-1].nr);
      } else 
    error("writing response","");
      mclose(fp);

   } 
   /* End critical section */
}

/******************************************************************************/
/* ENTER A RESPONSE INTO THE CURRENT ITEM                                     */
/******************************************************************************/
void                /* RETURNS: (nothing)              */
do_respond(ps,resp) /* ARGUMENTS:                      */
int   ps;           /*    Use a pseudo?                */
short resp;         /*    Response to prev. response # */
{
   short         nr;
   char          buff[MAX_LINE_LENGTH],
                 pseudo[MAX_LINE_LENGTH],
               **file;
   unsigned char omode;
   FILE         *fp;
   char        **config;
   int           ok;

#if 0
   /* Check for valid permissions and arguments */
   if (st_glob.c_status & CS_NORESPONSE) {
      printf("You only have read access.\n");
      return;
   }
#endif

   if (!check_acl(RESPOND_RIGHT,confidx)){
      printf("You only have read access.\n");
      return;
   }

   if (sum[st_glob.i_current-1].flags & IF_FROZEN) {
      sprintf(buff, "%s is frozen!\n", topic(1));
      wputs(buff);
      return;
   }
   nr = sum[ st_glob.i_current-1 ].nr;
   if (resp>nr) {
      printf("Highest response # is %d\n",nr-1);
      return;
   }
   
   /* Get pseudo */
   if (ps) {
      if (!(flags & O_QUIET))
         printf("What's your handle? ");
      if (!ngets(buff, st_glob.inp)) /* st_glob.inp */
         return;
      if (buff[0]=='%') {
         char *f, *str;
              
         str = buff+1;
         f = get_sep(&str);
         strcpy(buff, f);
      }
      if (strlen(buff)) 
         strcpy(pseudo,buff);
      else {
         sprintf(buff,"Response aborted!  Returning to current %s.\n",topic(0));
         wputs(buff);
         return;
      }
   } else 
      strcpy(pseudo, fullname_in_conference(&st_glob));
   
   if (nr >= MAX_RESPONSES) {
      sprintf(buff, "Too many responses on this %s!\n", topic(0));
      wputs(buff);
      return;
   }
   
#ifdef NEWS
   if (st_glob.c_security & CT_NEWS) {
      char rnh[MAX_LINE_LENGTH];

      if (!resp) resp=nr;

      if (!(config = get_config(confidx)))
         return;
      sprintf(buff,"%s/%s/%d",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]),
       st_glob.i_current);

      if (resp>0) {
         if ((fp=mopen(buff,O_R))==NULL) return;
         get_resp(fp,&(re[resp-1]),GR_ALL,resp-1);
         mclose(fp);
      }

      make_rnhead(re,resp);
      if (resp>0)
         xfree_array(re[resp-1].text);
      sprintf(rnh,"%s/.rnhead",home);
      sprintf(buff,"Pnews -h %s",rnh);
      unix_cmd(buff);
      rm(rnh,SL_USER);
      return;
   }
#endif

   if (st_glob.c_security & CT_EMAIL) {
		/* Load parent for inclusion */
      if (resp>0) {
         sprintf(buff,"%s/_%d",conflist[confidx].location,st_glob.i_current);
         if ((fp=mopen(buff,O_R))==NULL) return;
         get_resp(fp,&(re[resp-1]),GR_ALL,resp-1);
         mclose(fp);
      }
      make_emhead(re,resp);
      if (resp>0)
         xfree_array(re[resp-1].text);
   }

   /* Delete old if not EMAIL */
   if (text_loop(!(st_glob.c_security & CT_EMAIL), "response")
    && get_yes("Ok to enter this response? ", DEFAULT_OFF)) { /* success */
      omode = mode;
      mode = M_OK;

      if (st_glob.c_security & CT_EMAIL)  {
         if (!(config = get_config(confidx)))
            return;
         make_emtail();
         sprintf(buff,"%s -t < %s/cf.buffer",get_conf_param("sendmail",SENDMAIL),work);
/*
printf("Execute: %s\n", buff);
return;
*/
         unix_cmd(buff);

      } else if (!(file = grab_file(work,"cf.buffer",0)))
         wputs("The file cf.buffer doesn't seem to exist.\n");
      else if (!xsizeof(file)) {
         wputs("No text in buffer!\n");
         xfree_array(file);
      } else {
         add_response(&(sum[st_glob.i_current-1]),file,confidx,sum,part,
          &st_glob,0, NULL,uid,login,pseudo,resp);
        xfree_array(file);
        custom_log("respond", M_RFP);
      }

      if (flags & O_STAY)
         mode = omode;
   } else {
      sprintf(buff,"Response aborted!  Returning to current %s.\n",topic(0));
      wputs(buff);
   }
   
   /* Delete text buffer */
   sprintf(buff,"%s/cf.buffer",work);
   rm(buff,SL_USER);
}

/******************************************************************************/
/* ADD A RESPONSE TO THE CURRENT ITEM                                         */
/******************************************************************************/
int                /* RETURNS: (nothing)          */
respond(argc,argv) /* ARGUMENTS:                  */
int    argc;       /*    Number of arguments      */
char **argv;       /*    Argument list            */
{
   char buff[MAX_LINE_LENGTH];
   char act[MAX_ITEMS];
   short j,fl;
#if 0
   /* Check for valid permissions and arguments */
   if (st_glob.c_status & CS_NORESPONSE) {
      printf("You only have read access.\n");
      return 1;
   }
#endif

   if (!check_acl(RESPOND_RIGHT,confidx)){
      printf("You only have read access.\n");
      return 1;
   }

   rangeinit(&st_glob,act);
   refresh_sum(0,confidx,sum,part,&st_glob);
   st_glob.r_first = -1;

   fl = 0;
   if (argc<2) {
      printf("Error, no %s specified! (try HELP RANGE)\n", topic(0));
   } else { /* Process args */
      range(argc,argv,&fl,act,sum,&st_glob,0);
   }

   /* Process items */
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      if (!act[j-1] || !sum[j-1].flags) continue;

#ifdef NEWS
      if (st_glob.c_security & CT_NEWS) {
         char **config;

         if (!(config = get_config(confidx)))
           return 1;
         sprintf(buff,"%s/%s/%d",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]),
     j);
      } else
#endif
         sprintf(buff,"%s/_%d",conflist[confidx].location,j);
      st_glob.i_current=j;
      if (!(flags & O_QUIET))
         show_header();
      if (status & S_PAGER) spclose(st_glob.outp);
      do_respond(argc>0 && match(argv[0],"ps_eudonym"),st_glob.r_first+1);
   }
   return 1;
}

/******************************************************************************/
/* ENTER A RESPONSE IN THE CURRENT ITEM                                       */
/******************************************************************************/
int                    /* RETURNS: (nothing)          */
rfp_respond(argc,argv) /* ARGUMENTS:                  */
int    argc;           /*    Number of arguments      */
char **argv;           /*    Argument list            */
{
   short a= -1;

   if (argc>2 || (argc>1 && (sscanf(argv[1],"%hd",&a)<1 || a<0))) {
      printf("Bad parameters near \"%s\"\n",argv[(argc>2)?2:1]);
      return 2;
   } else
      do_respond(argc>0 && match(argv[0],"ps_eudonym"),(short)a+1);

   return 1;
}

void
dump_reply(sep) 
char *sep;
{
      sepinit(IS_START);
      itemsep(expand(sep, DM_VAR),0);
      for (st_glob.l_current=0;
           st_glob.l_current<xsizeof(re[st_glob.r_current].text)
        && !(status & S_INT);
           st_glob.l_current++) {
         sepinit(IS_ITEM);
         itemsep(expand(sep, DM_VAR),0);
      }
      sepinit(IS_CFIDX);
      itemsep(expand(sep, DM_VAR),0);
}

/******************************************************************************/
/* MAIL A REPLY TO THE AUTHOR OF A RESPONSE                                   */
/******************************************************************************/
int              /* RETURNS: (nothing)          */
reply(argc,argv) /* ARGUMENTS:                  */
int    argc;     /*    Number of arguments      */
char **argv;     /*    Argument list            */
{
   char buff[MAX_LINE_LENGTH];
   short i;
   FILE *fp,*pp;
   flag_t ss;
   register int cpid,wpid;
   int statusp;

   /* Validate arguments */
   if (argc<2 || sscanf(argv[1],"%hd",&i)<1) {
      printf("You must specify a comment number.\n");
      return 1;
   } else if (argc>2) {
      printf("Bad parameters near \"%s\"\n",argv[2]);
      return 2;
   }
   refresh_sum(0,confidx,sum,part,&st_glob);
   if (i<0 || i>=sum[st_glob.i_current - 1].nr) {
      wputs("Can't go that far! near \"<newline>\"\n");
      return 1;
   }

   if (re[i].flags & RF_CENSORED) {
      wputs("Cannot reply to censored response!\n");
      return 1;
   }
   if (re[i].offset < 0) {
      printf("Offset error.\n"); /* should never happen */
      return 1;
   }

   /* Get complete text */
#ifdef NEWS
   if (st_glob.c_security & CT_NEWS) {
      char **config;

      if (!(config = get_config(confidx)))
         return 1;
      sprintf(buff,"%s/%s/%d",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]),st_glob.i_current);
   } else
#endif
      sprintf(buff,"%s/_%d",conflist[confidx].location,st_glob.i_current);
   if ((fp=mopen(buff,O_R))==NULL) return 1;
   get_resp(fp,&(re[i]),GR_ALL,i);
   mclose(fp);

   /* Fork & setuid down when creating cf.buffer */
   fflush(stdout);
   if (status & S_PAGER)
      fflush(st_glob.outp);

   cpid=fork();
   if (cpid) { /* parent */
      if (cpid<0) return -1; /* error: couldn't fork */
      while ((wpid = wait(&statusp)) != cpid && wpid != -1);
      /* post = !statusp; */
   } else { /* child */
      if (setuid(getuid())) error("setuid","");
      setgid(getgid());

      /* Save to cf.buffer */
      sprintf(buff,"%s/cf.buffer",work);
      if ((fp=mopen(buff,O_W))==NULL) {
         xfree_array( re[i].text );
         return 1;
      }

      pp = st_glob.outp;
      ss = status;
      st_glob.r_current = i;
      st_glob.outp = fp;
      status |= S_PAGER;
 
      dump_reply("replysep");

      st_glob.outp = pp;
      status     = ss;

      dump_reply("replysep");

      mclose(fp);
      exit(0);
   }

   /* Invoke mailer */
   sprintf(buff,"mail %s",re[i].login);
   command(buff,0);

   xfree_array( re[i].text );
   mode = M_RFP;
   return 1;
}

/******************************************************************************/
/* CENSOR A RESPONSE IN THE CURRENT ITEM                                      */
/******************************************************************************/
int               /* RETURNS: (nothing)          */
censor(argc,argv) /* ARGUMENTS:                  */
int    argc;      /*    Number of arguments      */
char **argv;      /*    Argument list            */
{
   char buff[MAX_LINE_LENGTH],over[MAX_LINE_LENGTH];
   short i,typ,j,k;
   FILE *fp;
   char **text=NULL; short len;
   int frozen=0;  /* 1 if we need to unfreeze,censor,freeze */
   struct stat stt;

   typ = (match(argv[0],"scr_ibble"))? RF_CENSORED|RF_SCRIBBLED : RF_CENSORED;

   /* Validate arguments */
   if (argc<2 || sscanf(argv[1],"%hd",&i)<1) {
      printf("You must specify a comment number.\n");
      return 1;
   } else if (argc>2) {
      printf("Bad parameters near \"%s\"\n",argv[2]);
      return 2;
   }
   refresh_sum(0,confidx,sum,part,&st_glob);
   if (i<0 || i>=sum[st_glob.i_current - 1].nr) {
      wputs("Can't go that far! near \"<newline>\"\n");
      return 1;
   }

   /* Check for permission to censor */
   if (!re[i].date) get_resp(ext_fp,&(re[i]),GR_HEADER,i);
   if (!(st_glob.c_status & CS_FW) && (uid!=re[i].uid 
    || (uid==get_nobody_uid() && strcmp(login,re[i].login)))) {
      printf("You don't have permission to affect response %d.\n", i);
      return 1;
   }
   if (sum[st_glob.i_current-1].flags & IF_FROZEN) {
      if (!match(get_conf_param("censorfrozen",CENSOR_FROZEN),"true")) {
         sprintf(buff,"Cannot censor frozen %ss!\n", topic(0));
         wputs(buff);
         return 1;
      } else
         frozen=1;
   }

   if ((re[i].flags & typ)==typ) return 1; /* already done */
   if (re[i].offset < 0) {
      printf("Offset error.\n"); /* should never happen */
      return 1;
   }

   if (typ & RF_SCRIBBLED && !get_yes(expand("scribok",DM_VAR), DEFAULT_OFF))
      return 1;

   sprintf(buff,"%s/_%d",conflist[confidx].location,st_glob.i_current);
   if (frozen) {
      stat(buff,&stt);
      chmod(buff,stt.st_mode | S_IWUSR);
   }
   if ((fp=mopen(buff,O_RPLUS))!=NULL) {
      if (frozen)
         chmod(buff,stt.st_mode & ~S_IWUSR);
      if (fseek(fp,re[i].offset,0))
         error("fseeking in ",buff);
      fprintf(fp,",R%04d\n",typ);

      /* log it and overwrite it, unless it's a news article */
#ifdef NEWS
      if (!re[i].article) {
#endif
         get_resp(fp,&(re[i]),GR_ALL,i);
         fseek(fp,re[i].textoff,0);
         text = re[i].text;
         if (typ & RF_SCRIBBLED) {
            sprintf(over,"%s %s %s ",login,get_date(time((time_t *)0),0),
             fullname_in_conference(&st_glob));
            len = strlen(over);
            /* was j=re[i].numchars below */
            for (j=(re[i].endoff-3) - re[i].textoff; j>76; j-=76) {
               for (k=0; k<75; k++)
                  fputc(over[k%len],fp);
               fputc('\n',fp);
            }
            for (k=0; k<j-1; k++)
               fputc(over[k%len],fp);
            fprintf(fp, "\n,E\n");
         }
#ifdef NEWS
      }
#endif
      mclose(fp);

      /* Added 4/18, since sum file wasn't being updated, causing set
       * sensitive to fail.
       */
      sum[ st_glob.i_current-1 ].last  = time((time_t *)0);
      save_sum(sum, (short)(st_glob.i_current-1), confidx, &st_glob);
      dirty_sum(st_glob.i_current-1);
   } else if (frozen)
      chmod(buff,stt.st_mode & ~S_IWUSR);

   /* free_sum(sum); unneeded, always SF_FAST */

   /* Write to censorlog */
   sprintf(buff,"%s/censored",bbsdir);
   if ((fp=mopen(buff,O_A|O_PRIVATE)) != NULL) {
      fprintf(fp,",C %s %s %d resp %d rflg %d %s,%d %s date %s\n",
       conflist[confidx].location, topic(0), st_glob.i_current, i, typ, login, 
       uid, get_date(time((time_t *)0),0), fullname_in_conference(&st_glob));
      fprintf(fp,",R%04X\n,U%d,%s\n,A%s\n,D%08X\n,T\n",
       re[i].flags,re[i].uid,re[i].login,re[i].fullname,re[i].date);
      for (j=0; j<xsizeof(text); j++) 
         fprintf(fp,"%s\n",text[j]);
      fprintf(fp,",E\n");
      mclose(fp);
   }
   xfree_array( re[i].text );
   re[i].flags=typ;

   custom_log((typ==RF_CENSORED)? "censor":"scribble", M_RFP);
   return 1;
}

/******************************************************************************/
/* UN-CENSOR A RESPONSE IN THE CURRENT ITEM                                   */
/******************************************************************************/
int                 /* RETURNS: (nothing)          */
uncensor(argc,argv) /* ARGUMENTS:                  */
   int    argc;     /*    Number of arguments      */
   char **argv;     /*    Argument list            */
{
   char buff[MAX_LINE_LENGTH];
   short i,typ;
   FILE *fp;

   typ = (match(argv[0],"scr_ibble"))? RF_CENSORED|RF_SCRIBBLED : RF_CENSORED;

   /* Validate arguments */
   if (argc<2 || sscanf(argv[1],"%hd",&i)<1) {
      printf("You must specify a comment number.\n");
      return 1;
   } else if (argc>2) {
      printf("Bad parameters near \"%s\"\n",argv[2]);
      return 2;
   }
   refresh_sum(0,confidx,sum,part,&st_glob);
   if (i<0 || i>=sum[st_glob.i_current - 1].nr) {
      wputs("Can't go that far! near \"<newline>\"\n");
      return 1;
   }

   /* Check for permission to uncensor */
   if (!re[i].date) get_resp(ext_fp,&(re[i]),GR_HEADER,i);
   if (!(st_glob.c_status & CS_FW) && (uid!=re[i].uid 
    || (uid==get_nobody_uid() && strcmp(login,re[i].login)))) {
      printf("You don't have permission to affect response %d.\n", i);
      return 1;
   }
   if (sum[st_glob.i_current-1].flags & IF_FROZEN) {
      sprintf(buff,"Cannot uncensor frozen %ss!\n", topic(0));
      wputs(buff);
      return 1;
   }

   if (!(re[i].flags & (RF_CENSORED|RF_SCRIBBLED))) 
      return 1; /* already done */
   if (re[i].offset < 0) {
      printf("Offset error.\n"); /* should never happen */
      return 1;
   }

   sprintf(buff,"%s/_%d",conflist[confidx].location,st_glob.i_current);
   if ((fp=mopen(buff,O_RPLUS))!=NULL) {
      if (fseek(fp,re[i].offset,0))
         error("fseeking in ",buff);
      fprintf(fp,",R%04d\n",RF_NORMAL);

      mclose(fp);

      /* Added 4/18, since sum file wasn't being updated, causing set
       * sensitive to fail.
       */
      sum[ st_glob.i_current-1 ].last  = time((time_t *)0);
      save_sum(sum, (short)(st_glob.i_current-1), confidx, &st_glob);
      dirty_sum(st_glob.i_current-1);
   }
   re[i].flags=RF_NORMAL;

   /* free_sum(sum); unneeded, always SF_FAST */

   return 1;
}

/******************************************************************************/
/* PRESERVE RESPONSES IN THE CURRENT ITEM                                     */
/******************************************************************************/
int                 /* RETURNS: (nothing)          */
preserve(argc,argv) /* ARGUMENTS:                  */
int    argc;        /*    Number of arguments      */
char **argv;        /*    Argument list            */
{
   short i;
   short i_i;
   char buff[MAX_LINE_LENGTH];

   if (match(argv[0],"pr_eserve") || match(argv[0],"po_stpone")
    || match(argv[0],"n_ew") ||      match(argv[0],"wait")) {
      if (!(flags & O_QUIET)) {
         sprintf(buff, "This %s will still be new.\n", topic(0));
         wputs(buff);
      }
      st_glob.r_last = -2; 
   } else
      st_glob.r_last = -1; /* re-read nothing */

   /* Lots of ways to stop, so check inverse */
   i_i = st_glob.i_current - 1;
   if (!match(argv[0],"pr_eserve") && !match(argv[0],"po_stpone")
    && !match(argv[0],"p_ass") && !match(argv[0],"hide")) {
      if (st_glob.opt_flags & OF_REVERSE)
         st_glob.i_current = st_glob.i_first;
      else
        st_glob.i_current = st_glob.i_last;
      if (!(flags & O_QUIET))
         wputs("Stopping.\n");
      status |= S_STOP;
   }

   if (argc<2) {
      mode = M_OK;
      return 1;
   }

   /* Validate arguments */
   if (sscanf(argv[1],"%hd",&i)<1) {
      printf("You must specify a comment number.\n");
      return 1;
   } else if (argc>2) {
      printf("Bad parameters near \"%s\"\n",argv[2]);
      return 2;
   }
   refresh_sum(0,confidx,sum,part,&st_glob);
   if (i<0 || i>=sum[i_i].nr) {
      wputs("Can't go that far! near \"<newline>\"\n");
      return 1;
   }
   
   /* Do it */
   part[i_i].nr = st_glob.r_lastseen = i;
   if (st_glob.r_last == -2) { /* preserve/new */
      st_glob.r_last  =  -1;
      part[i_i].last  =  sum[i_i].last-1;
   } else
      time(&(part[i_i].last));
   dirty_part(i_i);
   mode = M_OK;
   
   return 1;
}

#ifdef INCLUDE_EXTRA_COMMANDS
short stack[MAX_RESPONSES],top=0;
void
traverse(i)
short i;
{
   short c,l,s;

   printf("%s%3d", (top)?"-":" ", i);
   stack[top++]=i;
   c=child(i);
   if (c<0) putchar('\n');
   else     traverse(c);
   
   top--;
   if (!top) return;

   c=sibling(i);
   if (c>=0) {
      for (l=1; l<=top; l++) {
         printf("   "); /* printf("(%d)",stack[l]); */
         s=sibling(stack[l]);
         if (s<0) putchar(' ');
         else if (l<top) putchar('|');
         else putchar( (sibling(s)<0)?'\\':'+' );
      }
      traverse(c);
   }
}

/******************************************************************************/
/* DISPLAY RESPONSE TREE                                                      */
/******************************************************************************/
int                 /* RETURNS: (nothing)          */
tree(argc,argv) /* ARGUMENTS:                  */
int    argc;        /*    Number of arguments      */
char **argv;        /*    Argument list            */
{
   short i=0;

   /* Validate arguments */
   if (argc>2 || (argc>1 && sscanf(argv[1],"%hd",&i)<1)) {
      printf("Bad parameters near \"%s\"\n",argv[2]);
      return 2;
   }
   refresh_sum(0,confidx,sum,part,&st_glob);
   if (i<0 || i>=sum[st_glob.i_current-1].nr) {
      wputs("Can't go that far! near \"<newline>\"\n");
      return 1;
   }
   traverse(i);
   return 1;
}

short
sibling(r)
short r;
{
      short a,p;

      /* Find next sibling */
      p = parent(r);
      a=r+1;
      if (!re[a].date) get_resp(ext_fp,&(re[a]),GR_HEADER,a);
      while (a<=st_glob.r_max && parent(a)!=p) {
         a++;
         if (!re[a].date) get_resp(ext_fp,&(re[a]),GR_HEADER,a);
      }
      if (a>st_glob.r_max) return -1;
      else return a;
}

short 
parent(r)
short r;
{
      return (re[r].parent < 1)? r-1 : re[r].parent-1;
}

short
child(r)
short r;
{
      short a,p;

      /* Find 1st child */
      a = p = r+1;
      if (!re[a].date) get_resp(ext_fp,&(re[a]),GR_HEADER,a);
      if (re[a].parent && re[a].parent!=p) {
         a++;
         if (!re[a].date) get_resp(ext_fp,&(re[a]),GR_HEADER,a);
         while (a<=st_glob.r_max && re[a].parent!=p) {
            a++;
            if (!re[a].date) get_resp(ext_fp,&(re[a]),GR_HEADER,a);
         }
      }
      return (a>st_glob.r_max)? -1 : a;
}
#endif
