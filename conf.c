/* $Id: conf.c,v 1.25 1998/06/17 17:49:11 kaylene Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h> /* for atoi */
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <pwd.h> /* for participants */
#include <netinet/in.h> /* for inet_addr */
#include <arpa/inet.h> /* for inet_addr */
#include <unistd.h>    /* for F_OK, etc */
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "conf.h"
#include "lib.h"
#include "joq.h"
#include "sum.h"
#include "item.h"
#include "range.h"
#include "macro.h"
#include "system.h"
#include "change.h"
#include "sep.h"
#include "xalloc.h"
#include "driver.h"  /* for source */
#include "stats.h"   /* for get_config */
#include "user.h"    /* for get_sysop_login */
#include "main.h"    /* for wputs() */
#include "sysop.h"   /* for is_sysop() */
#include "security.h" /* for check_acl() */

#ifdef HAVE_FSTAT
#include "files.h"
FILE *conffp;
#endif

unsigned int
security_type(config, idx)
   char **config;
   short  idx;
{
   unsigned int sec = 0;
   char **fields;
   register int i, n;

   if (xsizeof(config)<=CF_SECURITY) 
      return 0;

   fields = explode(config[CF_SECURITY], ", ", 1);
   n = xsizeof(fields);
   for (i=0; i<n; i++) {
      if (isdigit(fields[i][0]))
         sec |= atoi(config[CF_SECURITY]);
      else if (match(fields[i], "pass_word"))
         sec = (sec & ~CT_BASIC) | CT_PASSWORD;
      else if (match(fields[i], "ulist"))
         sec = (sec & ~CT_BASIC) | CT_PRESELECT;
      else if (match(fields[i], "pub_lic"))
         sec = (sec & ~CT_BASIC) | CT_OLDPUBLIC;
      else if (match(fields[i], "prot_ected"))
         sec = (sec & ~CT_BASIC) | CT_PUBLIC;
      else if (match(fields[i], "read_only"))
         sec |= CT_READONLY;
#ifdef NEWS
      else if (match(fields[i], "news_group"))
         sec |= CT_NEWS;
#endif
      else if (match(fields[i], "mail_list"))
         sec |= CT_EMAIL;
      else if (match(fields[i], "reg_istered"))
         sec |= CT_REGISTERED;
      else if (match(fields[i], "noenter"))
         sec |= CT_NOENTER;
   }
   xfree_array(fields);

   if ((sec & CT_EMAIL) 
    && (xsizeof(config)<=CF_EMAIL || !config[CF_EMAIL][0])) {
      sec &= ~CT_EMAIL;
      error("email address for ",compress(conflist[idx].name));
   }
#ifdef NEWS
   if (xsizeof(config)<=CF_NEWSGROUP)
      sec &= ~CT_NEWS;
#endif      

#ifdef WWW
{  /* If we find an originfile, then turn on originlist flag */
   struct stat st;
   char originfile[MAX_LINE_LENGTH];
   sprintf(originfile, "%s/originlist", conflist[idx].location);
   if (!stat(originfile,&st)) /* was access(), but that uses uid not euid */
      sec |= CT_ORIGINLIST;
}
#endif
   return sec;
}

int
is_fairwitness(idx)
   int idx;  /* conference index */
{
   char buff[MAX_LINE_LENGTH];
   char **config, **fw;
   int ok;
      
   if (!(config = get_config(idx)) || xsizeof(config)<=CF_FWLIST)
      ok = 0;
   else {
      sprintf(buff,"%d",uid);
      fw = explode(config[CF_FWLIST],", ", 1);
      ok = (searcha(login,fw,0)>=0 || searcha(buff,fw,0)>=0 || is_sysop(1));
      xfree_array(fw);
   }
   return ok;
}

#define MAX_LINES 5000
char **
grab_recursive_list(dir, filename)
   char *dir;
   char *filename;
{
   char **line = grab_file(dir, filename, GF_SILENT|GF_WORD|GF_IGNCMT);
   int i, nl;

   if (!line)
      return line;

   nl = xsizeof(line);
   line = xrealloc_array(line, MAX_LINES);

   /* Step through each element in the current list */
   for (i=0; i<nl; i++) {

      /* If it's another file, replace element with first element in list 
       * from file, and append the rest of the file's list to the end of
       * the current list 
       */
      if (line[i][0]=='/') {
         char **line2 = grab_file(line[i], NULL, GF_SILENT|GF_WORD|GF_IGNCMT);
         char **oldline;
         int i2, nl2;

         xfree_string(line[i]);

         if (!line2 || (nl2=xsizeof(line2))==0) {
            /* Replace file element with last element of combined list */
            line[i] = line[--nl];
            line[nl] = 0;
         } else {

            /* Replace file element with first element of its list */
            line[i] = line2[0];
            line2[0] = 0;

            /* Append rest of file's list to combined list */
            for (i2=1; i2<nl2; i2++) {
               line[nl++] = line2[i2];
               line2[i2] = 0;
            }
         }
         
         xfree_array(line2);
      }
   }
   while (i<MAX_LINES)
      line[i++] = 0;
   line = xrealloc_array(line, nl);
   return line;
}

int                   /* RETURNS: 1 if login or uid is in the file, 0 if not */
is_inlistfile(idx, file) /* ARGUMENTS:           */
   int idx;              /*    conference index  */
   char *file;           /*    filename to check */
{
    char **line;
    int    ok = 0;

    if (file[0]=='/')
       line = grab_recursive_list(file, NULL);
    else
       line = grab_recursive_list(conflist[idx].location, file);
    if (line) {
       char buff[MAX_LINE_LENGTH];

       sprintf(buff,"%d",uid);
       if (searcha(login,line,0)>=0 || searcha(buff,line,0)>=0)
          ok = 1;
       xfree_array(line);
    }
    return ok;
}

char *
fullname_in_conference(stt)  
   status_t *stt;
{
   if (!stt->fullname[0] || !strcmp(stt->fullname, DEFAULT_PSEUDO))
      return fullname;
   return stt->fullname;
}

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    join(char *conference, short idx, int force)
Called by:   command, main
Arguments:   cf name or conference # to join, force flag
Returns:   
Calls:       source() for CONF/rc and WORK/.cfrc files
             cat() for login
             get_idx to find cf name in conflist
Description: 
*******************************************************************************/
char                   /* RETURNS: 1 on success, 0 else       */
join(conf, force, secure)      /* ARGUMENTS:                          */
char *conf;            /*    Conference name to join          */
int   force;           /*    Force Observe/join flags         */
int   secure;
{
    char        buff[MAX_LINE_LENGTH];
    struct stat st;
    time_t      t1;
    char      **config;
    int         cp;

    /* Initialize st_new structure */
    st_new.outp       = st_glob.outp;
    st_new.inp        = st_glob.inp;
    st_new.mailsize   = st_glob.mailsize;
    st_new.c_security = st_new.i_current = st_new.c_status = 0;
    st_new.sumtime    = 0;
#ifdef NEWS
    st_new.c_article = 0;
#endif

   /* Reset name */
   strcpy(st_new.fullname, fullname);
#if 0
   if (!get_user(&uid, login, st_new.fullname, home, email)) {
      error("reading ","user entry");
      endbbs(2);
   }
#endif

    /* Check for existence */
    if (!conf) return 0; 
    joinidx=get_idx(conf,conflist,maxconf);
    if (joinidx<0) { 
       if (!(flags & O_QUIET))
          printf("Cannot access %s %s.\n",conference(0), conf);
       return 0; 
    } 
    if (debug & DB_CONF) 
       printf("join: %hd dir=%s\n",joinidx,conflist[joinidx].location);

    /* Read in config file */
    if (!(config=get_config(joinidx)))
       return 0; 

    /* Pass security checks */
    st_new.c_security = security_type(config, joinidx);

    if (secure) 
       cp = secure;
    else {
       cp = check_acl(JOIN_RIGHT, joinidx);
       if (!cp) 
          if (!(flags & O_QUIET)) {
             printf("You are not allowed to access the %s %s.\n", 
              compress(conflist[joinidx].name), conference(0));
          }

/*
 *     cp = checkpoint(joinidx, st_new.c_security, 0);
 */
    }

    if (!cp) {
       if (st_new.c_security & CT_READONLY)
          force |= O_READONLY;
       else {
          return 0;
       }
    }

    /* Force READONLY if login is in observer file */
    if (is_inlistfile(joinidx, "observers"))
       force |= O_READONLY;

    /* Do stuff with PARTDIR/.name.cf */
    sprintf(buff,"%s/%s",partdir,config[CF_PARTFILE]);
    if (debug & DB_CONF) 
       printf("join: Partfile=%s\n",buff);
    if (stat(buff,&st)) { /* Not a member */
       if (!((flags|force) & O_OBSERVE)) {

          /* Main JOQ cmd loop */
          mode = M_JOQ;
          if ((flags|force) & O_AUTOJOIN) {
             if (!(flags & O_QUIET)) {
                sprintf(buff,"You are being automatically registered in %s\n",
                 conflist[joinidx].location);
                wputs(buff);
             }
             command("join",0);
          } else {
             if (!(flags & O_QUIET)) {
                printf("You are not a member of %s\nDo you wish to:",
                 conflist[joinidx].location);
             }
             while (mode==M_JOQ && get_command(NULL, 0));
          }

          if (status & S_QUIT) {
             printf("registration aborted (didn't leave)\n");
             status &= ~S_QUIT;
             return 0;
          }
       }
       t1 = (time_t)0;
    } else {
       t1 = st.st_mtime; /* last time .*.cf was touched */
    }
    if (debug & DB_CONF) 
       printf("join: t1=%x\n",t1);

    if (confidx>=0) leave(0,(char**)0);

/* was ifdef STUPID_REOPEN */
#if 1
{
   FILE *inp = st_glob.inp;
   memcpy(&st_glob,&st_new,sizeof(st_new));
   st_glob.inp = inp;
}
#else
    memcpy(&st_glob,&st_new,sizeof(st_new));
#endif

    confidx =joinidx; /* passed all security checks */
#ifdef HAVE_FSTAT
    sprintf(buff,"%s/%s",conflist[confidx].location,"config");
    conffp = mopen(buff, O_R);
#endif
    read_part(config[CF_PARTFILE],part,&st_glob,confidx);

    /* Set status */
    if ((flags|force) & O_OBSERVE)
       st_glob.c_status |=  CS_OBSERVER;
    if ((flags|force) & O_READONLY)
       st_glob.c_status |=  CS_NORESPONSE;

    /* Allow FW to be specified by login or by UID */
    if (is_fairwitness(confidx))
       st_glob.c_status |=  CS_FW;
    if (debug & DB_CONF) 
       printf("join: Status=%hd\n",(short)status);

    st_glob.sumtime = 0;
    refresh_sum(0,confidx,sum,part,&st_glob);

    /* Source CONF/rc file and WORK/.cfrc files */
    if (flags & O_SOURCE) {
       source(conflist[confidx].location,"rc", STD_SANE, SL_OWNER);
/*     source(work,".cfrc", 0, SL_USER); */
    }

    /* Display login file */
    if (!(flags & O_QUIET)) {
       sepinit(IS_START|IS_ITEM);
       confsep(expand("linmsg", DM_VAR),confidx,&st_glob,part,0);
       check_mail(1);
    }
    custom_log("join", M_OK);

    /* Source WORK/.cfrc files */
    if (flags & O_SOURCE) {
       mode = M_OK;
       if (st_glob.c_status & CS_JUSTJOINED) {
          source(bbsdir, ".cfjoin", 0, SL_OWNER);
          source(work,   ".cfjoin", 0, SL_USER);
       }
       source(work,".cfrc", 0, SL_USER);
    }
    return 1;
}

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    leave
Called by:   command
Arguments:   
Returns:   
Calls:       cat() for logout
Description: 
*******************************************************************************/
int                         /* RETURNS: error flag                 */
leave(argc,argv)            /* ARGUMENTS: (none)                   */
int argc;
char **argv;
{
   char **config;
   if (debug & DB_CONF) 
      printf("leave: %hd\n",confidx);
   if (confidx<0) return 1; /* (noconf) */

   if (!argc || argv[0][0]!='a') { /* not "abort" */

      /* Display logout */
      /* more(conflist[confidx].location,"logout"); */
      if (!(flags & O_QUIET)) {
         sepinit(IS_START|IS_ITEM);
         confsep(expand("loutmsg", DM_VAR),confidx,&st_glob,part,0);
      }
      custom_log("leave", M_OK);

      /* Write participation file unless observing */
      if (!(st_glob.c_status & CS_OBSERVER)) {
         if (!(config = get_config(confidx)))
            return 1;
         write_part(config[CF_PARTFILE]);
      }
   }

   st_glob.sumtime = 0;
   st_glob.c_status = 0; /* |= CS_OTHERCONF; */

   confidx = -1;
#ifdef HAVE_FSTAT
   mclose(conffp);
#endif
   undefine(DM_SANE);

   /* Re-source system rc file */
   source(bbsdir,"rc", STD_SUPERSANE, SL_OWNER);

   return (!argc || argv[0][0]!='a');
}

/******************************************************************************/
/* CHECK A LIST OF CONFERENCES FOR NEW ITEMS                                  */
/******************************************************************************/
int              /* RETURNS: (nothing)     */
check(argc,argv) /* ARGUMENTS:             */
   int    argc;     /*    Number of arguments */
   char **argv;     /*    Argument list       */
{
   int i;
   unsigned int sec;
   char **list=NULL,*cfname,buff[MAX_LINE_LENGTH];
   short size,idx,all=0,count=0,argidx=1;
   partentry_t part2[MAX_ITEMS],*part3;
   sumentry_t  sum2[MAX_ITEMS];
   status_t   *st,st_temp;
   struct stat stt;
   char      **config;
   long        force;

#ifdef INCLUDE_EXTRA_COMMANDS
   /* Simple cfname dump for WWW */
   if (flags & O_CGIBIN) {
      sepinit(IS_START);
      for (idx=1; idx<maxconf; idx++) {
         sepinit(IS_ITEM);
         if (idx==maxconf-1)
            sepinit(IS_CFIDX);
         confsep(expand("listmsg", DM_VAR),idx,&st_glob,part,0);
      }
      return 1;
   }
#endif

   /* Check for before/since dates */
   st_glob.since = st_glob.before = 0;
   if (argc>argidx) {
      if (match(argv[argidx],"si_nce") 
       || match(argv[argidx],"S=")) {
         st_glob.since  = since(argc,argv,&argidx);
         argidx++;
      } else if (match(argv[argidx],"before") 
       ||        match(argv[argidx],"B=")) {
         st_glob.before = since(argc,argv,&argidx);
         argidx++;
      }
   }

   if (argc>argidx) {            /* list given by user */
      size = argc-argidx;
      list = argv+argidx;
   } else if (argv[0][0]=='l') { /* use conflist */
      all  = 1;
      size = maxconf-1;
   } else {                      /* use .cflist */
      refresh_list();
      size = xsizeof(cflist);
      list = cflist;
   }

   sepinit(IS_START);
   for (i=0; i<size && !(status & S_INT); i++) {
      force = 0;
      idx=(all)? i+1 : get_idx(list[i],conflist,maxconf);
      cfname = (all)? compress(conflist[idx].name) : list[i];

      if (idx<0 || !(config=get_config(idx))) {
         printf("Cannot access %s %s.\n",conference(0),cfname);
         continue;
      } 

      /* Pass security checks */
      if (!check_acl(JOIN_RIGHT, idx))
         continue;
      if (!check_acl(RESPOND_RIGHT, idx))
         force |= O_READONLY;

      sec = security_type(config, idx);
/*
      if (!checkpoint(idx,sec, 1)) {
         if (sec & CT_READONLY) {
            force |= O_READONLY;
         } else {
            continue;
         }
      }
*/

/*    if (idx==confidx)  */
      if (confidx>=0 
       && !strcmp(conflist[idx].location,conflist[confidx].location)) {
         refresh_sum(0,confidx,sum,part,&st_glob);
         st = &st_glob;
         part3=part;
      } else {
         st = &st_temp;
#ifdef NEWS
         st->c_article = 0;
#endif
         read_part(config[CF_PARTFILE],part2,st,idx); /* Read in partfile */

         /* Initialize c_status */
         st->c_status = 0;
         if (is_fairwitness(idx))
            st->c_status |=  CS_FW;
         else
            st->c_status &= ~CS_FW;
         if ((flags|force) & O_OBSERVE)
            st->c_status |=  CS_OBSERVER;
         else
            st->c_status &= ~CS_OBSERVER;
         if ((flags|force) & O_READONLY)
            st->c_status |=  CS_NORESPONSE;
         else
            st->c_status &= ~CS_NORESPONSE;

         sprintf(buff,"%s/%s",partdir,config[CF_PARTFILE]);
         st->parttime   = (stat(buff,&stt))? 0 : stt.st_mtime;
         st->c_security = security_type(config, idx);

         /* Read in sumfile */
         get_status(st,sum2,part2,idx); 
         part3=part2;
   
      }
      st->c_security = sec;

      if ((!st_glob.before || st->sumtime<st_glob.before)
       &&  (st_glob.since <= st->sumtime)) {
         st->count = (++count);
         sepinit(IS_ITEM);
         if (argc<2 && current==i) 
            sepinit(IS_CFIDX);
         confsep(expand((argv[0][0]=='l')?"listmsg":"checkmsg", DM_VAR),
          idx,st,part3,0);
      }
   }
   return 1;
}

/******************************************************************************/
/* ADVANCE TO NEXT CONFERENCES WITH NEW ITEMS                                 */
/******************************************************************************/
int                                   /* RETURNS: (nothing)     */
do_next(argc,argv)                    /* ARGUMENTS:             */
int    argc;                          /*    Number of arguments */
char **argv;                          /*    Argument list       */
{                                     /* LOCAL VARIABLES:       */
   char      **config,
               buff[MAX_LINE_LENGTH];
#if 0
   unsigned int sec;                   /*    Conference's security type     */
#endif
   short       idx;
   partentry_t part2[MAX_ITEMS];
   sumentry_t  sum2[MAX_ITEMS];
   status_t    st;
   struct stat stt;
   int         cp=0;

   if (argc>1) {
      printf("Bad parameters near \"%s\"\n",argv[1]);
      return 2;
   }

   refresh_list(); /* make sure .cflist is current */
   for (; current+1 < xsizeof(cflist) && !(status & S_INT); current++) {
      idx=get_idx(cflist[current+1],conflist,maxconf);
      if (idx<0 || !(config=get_config(idx))) {
         printf("Cannot access %s %s.\n",conference(0),
          cflist[current+1]);
         continue;
      } 

      /* Check security */
      cp = check_acl(JOIN_RIGHT, idx);
      if (!cp) {
         if (!(flags & O_QUIET)) {
            printf("You are not allowed to access the %s %s.\n", 
             compress(conflist[joinidx].name), conference(0));
         }
         continue;
      }

/*
      sec=security_type(config, idx);
      cp=checkpoint(idx,sec, 0);
      if (!(sec & CT_READONLY) && !cp)
         continue;
*/

/*    if (idx==confidx)  */
      if (confidx>=0  /* don't segfault if in no conf */
       && !strcmp(conflist[idx].location,conflist[confidx].location)) {
         refresh_sum(0,confidx,sum,part,&st_glob);
         if (st_glob.i_newresp || st_glob.i_brandnew) {
            join(cflist[++current],0,cp);
            return 1;
         }
      } else {
         read_part(config[CF_PARTFILE],part2,&st,idx); /* Read in partfile */
         sprintf(buff,"%s/%s",partdir,config[CF_PARTFILE]);
         st.parttime   = (stat(buff,&stt))? 0 : stt.st_mtime;
         st.c_security = security_type(config, idx);
#ifdef NEWS
         st.c_article = 0;
#endif
         get_status(&st,sum2,part2,idx);  /* Read in sumfile */
         st.sumtime = 0;
         if (st.i_newresp || st.i_brandnew) {
            join(cflist[++current],0,cp);
            return 1;
         }
      }
      printf("No new %ss in %s\n",topic(0), cflist[current+1]);
   }
   printf("No more %ss left.\n", conference(0));
   return 2;
}

/******************************************************************************/
/* RESIGN FROM (UNJOIN) THE CURRENT CONFERENCE                                */
/******************************************************************************/
int              /* RETURNS: (nothing)     */
resign(argc,argv) /* ARGUMENTS:             */
int    argc;      /*    Number of arguments */
char **argv;      /*    Argument list       */
{
   char    buff[MAX_LINE_LENGTH];
   char **config;

   if (argc>1) {
      printf("Bad parameters near \"%s\"\n",argv[1]);
      return 2;
   }
   if (st_glob.c_status & CS_OBSERVER) {
      printf("But you don't belong to this %s!\n", conference(0));
      return 1;
   }
   if (get_yes("Are you sure you wish to resign? ", DEFAULT_OFF)) {
      if (!(config = get_config(confidx)))
         return 1;

      /* Delete participation file */
      sprintf(buff,"%s/%s",partdir,config[CF_PARTFILE]);
      rm(buff, partfile_perm());

      /* Remove from .cflist if current is set */
      if (current>=0)
         del_cflist(cflist[current]);

      /* Become an observer */
      st_glob.c_status |= CS_OBSERVER;
      printf("You are now just an observer.\n");

      /* Remove login/uid from ulist file:
       * In this case, we don't remove it from recursive files, only
       * if it's in the top level ulist
       */
      if (is_auto_ulist(confidx)) {
         char   uidstr[MAX_LINE_LENGTH];
         char   buff[MAX_LINE_LENGTH];
         char   file[MAX_LINE_LENGTH];
         char   file2[MAX_LINE_LENGTH];
         char **ulst;
         int    j;

         sprintf(file,"%s/ulist",conflist[confidx].location);
         sprintf(file2,"%s.tmp",file);
         ulst=grab_file(conflist[confidx].location,"ulist",
          GF_WORD|GF_SILENT|GF_IGNCMT);
         sprintf(uidstr,"%d",uid);

         for (j=0; j<xsizeof(ulst); j++) {
            if (strcmp(login,ulst[j]) && strcmp(uidstr,ulst[j])<0) {
               sprintf(buff,"%s\n",ulst[j]);
               if (!write_file(file2,buff))
                  break; 
            }
         }     
         if (rename(file2,file)) 
            error("renaming ",file);
         xfree_array(ulst);

         custom_log("resign", M_OK);
      }
   } 
   return 1;
}

#ifdef WWW
/******************************************************************************/
/* SEE IF A HOSTNAME ENDS WITH A SPECIFIED DOMAIN NAME                        */
/******************************************************************************/
static int            /* RETURNS: 1 on match, 0 on failure */
matchhost(spec, host) /* ARGUMENTS: */
   char *spec;        /*    Domain name allowed     */
   char *host;        /*    Origin host to validate */
{
   return !strcmp(spec, host+strlen(host)-strlen(spec));
}

/******************************************************************************/
/* SEE IF AN ORIGIN IP ADDRESS MATCHES A SPECIFIED ADDRESS PREFIX             */
/******************************************************************************/
static int                  /* RETURNS: 1 on match, 0 on failure */
matchaddr(specstr, addrstr) /* ARGUMENTS:                        */
   char *specstr;           /*    IP prefix allowed              */
   char *addrstr;           /*    Origin IP addr to validate     */
{
   u_int32 subnetaddr, subnetmask, hostaddr;
   int   masklen=0;
   char **field;
   char **num;

   hostaddr = ntohl(inet_addr(addrstr));
   field = explode(specstr, "/", 1);
   num   = explode(field[0], ".", 1);
   if (xsizeof(field) > 1)
      masklen = atoi(field[1]);
   else
      masklen = 8*xsizeof(num);
   subnetmask = 0xFFFFFFFF << (32-masklen);
   subnetaddr = inet_network(field[0]) << (32-8*xsizeof(num));

   xfree_array(field);
   xfree_array(num);
   return (hostaddr & subnetmask)==(subnetaddr & subnetmask);
}

int /* RETURNS: 1 if no originlist or user is from valid origin, 0 otherwise */
is_validorigin(idx) /* ARGUMENTS:              */
   int idx;         /*    IN: Conference index */
{
   char **originlist;
   int    i, n, ok = 1;

   originlist=grab_file(conflist[idx].location,"originlist",
    GF_WORD|GF_IGNCMT);
   if (originlist) {
      n = xsizeof(originlist);
      if (!n)
         xfree_array(originlist);
      else {
         ok = 0;

         for (i=0; !ok && i<n; i++) {
            if (isdigit(originlist[i][0])) {     /* Check IP address */
               if (matchaddr(originlist[i], expand("remoteaddr", DM_VAR)))
                  ok=1;
            } else {                             /* Check hostname */
               if (matchhost(originlist[i], expand("remotehost", DM_VAR)))
                  ok=1;
            }
         }
         
         xfree_array(originlist);
      } 
   }
   return ok;
}
#endif

int                 /* RETURNS: 1 if user passes password check, 0 if not */
check_password(idx) /* ARGUMENTS: */
   int idx;         /*    IN: conference index */
{
   char **password;
   int    ok = 1;

   if (!(password=grab_file(conflist[idx].location,"secret",0)))
      return 0;

   if (xsizeof(password)>0) {
      printf("Password for %s: ",compress(conflist[idx].name));
      if (strcmp(get_password(), password[0])) {
         printf("Invalid password.\n");
         ok = 0;
      }
   }
   xfree_array(password);
   return ok;
}


#if 0
/******************************************************************************/
/* DETERMINE IF USER IS ALLOWED TO JOIN A CONFERENCE                          */
/******************************************************************************/
char                /* RETURNS: 1 if passed, 0 if failed */
checkpoint(idx,sec,silent) /* ARGUMENTS:                        */
   short idx;          /*    Conference # to checkpoint     */
   unsigned int sec;       /*    Conference's security type     */
   int   silent;
{
   unsigned int osec;

    /* Fail if not authenticated */
    if (status & S_NOAUTH)
       return 0;
    
    /* Do Security checks */
    osec = sec;
    sec &= CT_BASIC;
#ifdef INCLUDE_EXTRA_COMMANDS
    if ((flags & O_CGIBIN) && (sec==CT_PRESELECT || sec==CT_PARANOID || sec==CT_PASSWORD))
       return 0;
#endif
    if (sec==CT_PRESELECT || sec==CT_PARANOID) {
       if (!is_inlistfile(idx, "ulist")) {
          if (!(osec & CT_READONLY) && !silent)
             printf("You are not allowed to access the %s %s.\n", 
              compress(conflist[idx].name), conference(0));
          return 0;
       }
    }
    if (sec==CT_PASSWORD || sec==CT_PARANOID) {
       if (!check_password(idx))
          return 0;
    }

#ifdef WWW
   if (osec & CT_ORIGINLIST) {
      if (!is_validorigin(idx)) {
         if (!(osec & CT_READONLY) && !silent)
            printf("You are not allowed to access the %s %s.\n", 
             compress(conflist[idx].name), conference(0));
         return 0;
      }
   }
#endif
   return 1;
}
#endif

/******************************************************************************/
/* Describe a conference: "describe <conf>"                                   */
/******************************************************************************/
int                     /* RETURNS: (nothing)     */
describe(argc,argv)     /* ARGUMENTS:             */
   int    argc;         /*    Number of arguments */
   char **argv;         /*    Argument list       */
{
   if (argc!=2) {
      printf("usage: describe <%s>\n", conference(0));
      return 1;
   }

   printf("%s\n", get_desc(argv[1]));
   return 1;
}

/******************************************************************************/
/* GET INFORMATION ON CONFERENCE PARTICIPANTS                                 */
/******************************************************************************/
int                     /* RETURNS: (nothing)     */
participants(argc,argv) /* ARGUMENTS:             */
int    argc;            /*    Number of arguments */
char **argv;            /*    Argument list       */
{
   char         **ulst,**namestr,
                **config;
#if 0
   struct passwd *pwd;
#endif
   struct stat    st;
   short          j,all=0,dump=0;
   time_t         tparttime;
   int            tuid;
   char           tlogin[L_cuserid];
   char           tfullname[MAX_LINE_LENGTH];
   char           twork[MAX_LINE_LENGTH];
   char           tpartdir[MAX_LINE_LENGTH];
   char           temail[MAX_LINE_LENGTH];
   char           buff[MAX_LINE_LENGTH];
   char           file[MAX_LINE_LENGTH],file2[MAX_LINE_LENGTH];
   FILE          *fp;

   /* Save user info */
   tuid = uid;                 strcpy(tlogin,login); 
   strcpy(temail,email); 
   strcpy(tfullname,st_glob.fullname); 
   strcpy(twork,work);
   strcpy(tpartdir, partdir);
   tparttime = st_glob.parttime;
   st_glob.count = 0;

   if (argc>1) { /* User list specified */
      ulst = xalloc(argc-1,sizeof(char *));
      for (j=1; j<argc; j++)
         ulst[j-1] = xstrdup(argv[j]);
   } else {
      sprintf(file,"%s/ulist",conflist[confidx].location);
      if (!(ulst = grab_recursive_list(conflist[confidx].location, "ulist"))) {
         all = 1;
         setpwent();
      } else 
#if 0
      if (!((st_glob.c_security & CT_BASIC)==CT_PRESELECT 
       ||   (st_glob.c_security & CT_BASIC)==CT_PARANOID)) 
#endif
      if (is_auto_ulist(confidx)) {
         dump=1;
         sprintf(file2,"%s/ulist.tmp",conflist[confidx].location);
      }
   }

   open_pipe();

   if (status & S_EXECUTE)    fp = 0;
   else if (status & S_PAGER) fp = st_glob.outp;
   else                       fp = stdout;

   /* Process items */
   sepinit(IS_START);
   confsep(expand("partmsg", DM_VAR),confidx,&st_glob,part,0);

   for (j=0; !(status & S_INT); j++) {
      if (all) {
         struct passwd *pwd;

         if (!(pwd = getpwent())) break;

         uid = pwd->pw_uid;
         strcpy(login, pwd->pw_name);
         namestr=explode(pwd->pw_gecos,expand("gecos",DM_VAR), 0);
         if (xsizeof(namestr))
            strcpy(st_glob.fullname,namestr[0]);
         else
            st_glob.fullname[0]=0;
         xfree_array(namestr);
         strcpy(home, pwd->pw_dir);
      } else {
         if (j>=xsizeof(ulst)) break;
         if (isdigit(ulst[j][0])) {
            uid = atoi(ulst[j]);    /* search by uid */
            login[0] = '\0'; 
         } else {
            uid = 0;
            strcpy(login, ulst[j]); /* search by login */
         }
         if (!get_user(&uid, login, st_glob.fullname, home, email)) {
            printf(" User %s not found\n",ulst[j]);
            if (dump) {
               xfree_string(ulst[j]);
               ulst[j] = 0;
               dump = 2;
            }
            continue;
         } 
      }

      if (!(config = get_config(confidx)))
         return 1;

      sprintf(work,"%s/.cfdir",home);
      get_partdir(partdir, login);
      sprintf(buff,"%s/%s",partdir,config[CF_PARTFILE]);
      if (stat(buff,&st)) {
         strcpy(work,home);
         sprintf(buff,"%s/%s",partdir, config[CF_PARTFILE]);
         if (stat(buff,&st)) {
            if (dump) { /* someone resigned or deleted a part file */
               xfree_string(ulst[j]);
               ulst[j] = 0;
               dump = 2;
            } 

            sprintf(buff,"User %s not a member\n",login);
            wfputs(buff,fp);
            continue;
         }
      }

      st_glob.parttime = st.st_mtime;
      st_glob.count++;
      sepinit(IS_ITEM);
      confsep(expand("partmsg", DM_VAR),confidx,&st_glob,part,0);

      if (all) {
         sprintf(buff,"%s\n",login);
         write_file(file,buff);
      }
   }
   sepinit(IS_CFIDX);
   confsep(expand("partmsg", DM_VAR),confidx,&st_glob,part,0);

   if (dump==2) { /* reset ulist file */
      for (j=0; j<xsizeof(ulst); j++) {
         if (ulst[j]) {
            sprintf(buff,"%s\n",ulst[j]);
            if (!write_file(file2,buff)) {
               dump=3; 
               break;
            }
         }
      }
   }
   if (dump==2)
      if (rename(file2,file)) error("renaming ",file);

   if (all) endpwent();
   else xfree_array(ulst);

   /* Restore user info */
   uid = tuid;                 
   strcpy(login,tlogin); 
   if (!get_user(&uid, login, st_glob.fullname, home, email)) {
      error("reading ","user entry");
      endbbs(2);
   }
   strcpy(st_glob.fullname,tfullname); 
   st_glob.parttime = tparttime;
   strcpy(work,twork);
   strcpy(partdir,tpartdir);

   return 1;
}

void
log(idx,str)
   short idx;
   char *str;
{
   char buff[MAX_LINE_LENGTH],
        file[MAX_LINE_LENGTH],
        timestamp[MAX_LINE_LENGTH];
   time_t t;

   sprintf(file, "%s/log", conflist[idx].location);
   time(&t);
   strcpy(timestamp,ctime(&t)+4);
   timestamp[20]='\0';
   sprintf(buff,"%s %s %s\n", timestamp, login, str);
   write_file(file, buff);
}

char *
nextconf()
{
   int i, idx;

   for (i=0; i<xsizeof(cflist); i++) {
      idx=get_idx(cflist[i],conflist,maxconf);
      if (idx==confidx)
         break;
   }

   if (i+1 >= xsizeof(cflist))
      return "";
   else
      return cflist[i+1];
}


/* find the next conference in conference list with new items in it,
 * wrap to around to the begining of conference list only once
 * to check for new items.  Returns the name of the conference 
 */
static char *
findnewconf(wrapped)
   int wrapped;
{                                     /* LOCAL VARIABLES:       */
   char      **config,
               buff[MAX_LINE_LENGTH];
   short       idx;
   partentry_t part2[MAX_ITEMS];
   sumentry_t  sum2[MAX_ITEMS];
   status_t    st;
   struct stat stt;
   int         cp=0;
   int         nextconference = current+1;

   refresh_list(); /* make sure .cflist is current */
   for (; nextconference < xsizeof(cflist) && !(status & S_INT); 
    nextconference++) {
      idx=get_idx(cflist[nextconference],conflist,maxconf);
      if (idx<0 || !(config=get_config(idx))) {
         if (!(flags & O_QUIET)) {
            printf("Cannot access %s %s.\n",conference(0),
             cflist[nextconference]);
         }
         continue;
      } 

      /* Check security */
      cp = check_acl(JOIN_RIGHT, idx);
      if (!cp) {
         if (!(flags & O_QUIET)) {
            printf("You are not allowed to access the %s %s.\n", 
             compress(conflist[joinidx].name), conference(0));
         }
         continue;
      }

      if (confidx>=0  /* don't segfault if in no conf */
       && !strcmp(conflist[idx].location,conflist[confidx].location)) {
         refresh_sum(0,confidx,sum,part,&st_glob);
         if (st_glob.i_newresp || st_glob.i_brandnew) {
            return cflist[nextconference];
         }
      } else {
         read_part(config[CF_PARTFILE],part2,&st,idx); /* Read in partfile */
         sprintf(buff,"%s/%s",partdir,config[CF_PARTFILE]);
         st.parttime   = (stat(buff,&stt))? 0 : stt.st_mtime;
         st.c_security = security_type(config, idx);
#ifdef NEWS
         st.c_article = 0;
#endif
         get_status(&st,sum2,part2,idx);  /* Read in sumfile */
         st.sumtime = 0;
         if (st.i_newresp || st.i_brandnew) {
            return cflist[nextconference];
         }
      }
   if (!(flags & O_QUIET)) {
         printf("No new %ss in %s\n",topic(0), cflist[nextconference]);
      }
   }
   if (!(flags & O_QUIET)) {
      printf("No more %ss left.\n", conference(0));
   }
   if(!wrapped)/* Found end of list, wrap to begining and check again */
      return findnewconf(1); 

   /* No conference with new items left in the conference list */
   return "";
}

char *
nextnewconf()
{
   return findnewconf(0);
}

char *
prevconf()
{
   int i, idx;

   for (i=0; i<xsizeof(cflist); i++) {
      idx=get_idx(cflist[i],conflist,maxconf);
      if (idx==confidx)
         break;
   }

   if (i>=xsizeof(cflist) || i<1)
      return "";
   else
      return cflist[i-1];
}

/* Get short description of a conference based on confidx */
char *
get_desc(name)
   char *name;
{
   int i;
   int sz = xsizeof(desclist)/sizeof(assoc_t);

   for (i=0; i<sz; i++) {
      if (match(name, desclist[i].name))
         return desclist[i].location;
   }
   return desclist[0].location; /* use the default */
}

/******************************************************************************/
/* CHECK A LIST OF CONFERENCES FOR NEW ITEMS                                  */
/******************************************************************************/
int                        /* RETURNS: (nothing)     */
show_conf_index(argc,argv) /* ARGUMENTS:             */
   int    argc;            /*    Number of arguments */
   char **argv;            /*    Argument list       */
{
   int i;
   unsigned int sec;
   char  buff[MAX_LINE_LENGTH];
   int   idx,count=0;
   short argidx=1;
   partentry_t part2[MAX_ITEMS],*part3;
   sumentry_t  sum2[MAX_ITEMS];
   status_t   *st,st_temp;
   struct stat stt; 
   char      **config;
   long        force=0;

   int size = xsizeof(desclist) / sizeof(assoc_t);

   /* Check for before/since dates */
   st_glob.since = st_glob.before = 0;
   if (argc>argidx) {
      if (match(argv[argidx],"si_nce")
       || match(argv[argidx],"S=")) {
         st_glob.since  = since(argc,argv,&argidx);
         argidx++;
      } else if (match(argv[argidx],"before")
       ||        match(argv[argidx],"B=")) {
         st_glob.before = since(argc,argv,&argidx);
         argidx++;
      }
   }  

   sepinit(IS_START);
   for (i=1; i<size && !(status & S_INT); i++) {

      if (!strcmp(desclist[i].name, "group")) {
         strcpy(st_glob.string, desclist[i].location);
/*
         printf("\n");
         for (j=strlen(desclist[i].location); j<80; j+=2)
            putchar(' ');
         printf("**%s**\n", desclist[i].location);
*/
         confsep(expand("groupindexmsg", DM_VAR),-1,&st_glob,part,0);
      } else {
         idx = get_idx(desclist[i].name, conflist, maxconf);
  
         /* 
          * The following code was copied from check()
          */

         if (idx<0 || !(config=get_config(idx))) {
            error("reading config for ", desclist[i].name);
            continue;
         }
      
         /* Pass security checks */
         if (!check_acl(JOIN_RIGHT, idx))
            continue;
         if (!check_acl(RESPOND_RIGHT, idx))
            force |= O_READONLY;

         sec= security_type(config, idx);
/*
         if (!checkpoint(idx,sec, 1)) {
            if (sec & CT_READONLY) {
               force |= O_READONLY;
            } else {
               continue;
            }
         }
*/

         if (confidx>=0
          && !strcmp(conflist[idx].location,conflist[confidx].location)) {
            refresh_sum(0,confidx,sum,part,&st_glob);
            st = &st_glob;
            part3=part;
         } else {
            st = &st_temp;
#ifdef NEWS
            st->c_article = 0;
#endif
            read_part(config[CF_PARTFILE],part2,st,idx); /* Read in partfile */

            /* Initialize c_status */
            st->c_status = 0;
            if (is_fairwitness(idx))
               st->c_status |=  CS_FW;
            else
               st->c_status &= ~CS_FW;
            if ((flags|force) & O_OBSERVE)
               st->c_status |=  CS_OBSERVER;
            else 
               st->c_status &= ~CS_OBSERVER;
            if ((flags|force) & O_READONLY)
               st->c_status |=  CS_NORESPONSE;
            else
               st->c_status &= ~CS_NORESPONSE;
   
            sprintf(buff,"%s/%s",partdir,config[CF_PARTFILE]);
            st->parttime   = (stat(buff,&stt))? 0 : stt.st_mtime;
            st->c_security = security_type(config, idx);
               
            /* Read in sumfile */
            get_status(st,sum2,part2,idx);
            part3=part2;
  
         }
         st->c_security = sec;

         if ((!st_glob.before || st->sumtime<st_glob.before)
          &&  (st_glob.since <= st->sumtime)) {
            strcpy(st->string, desclist[i].location);
            st->count = (++count);
            sepinit(IS_ITEM);
            if (argc<2 && current==i)
               sepinit(IS_CFIDX);
/* End of code copied from check */
            def_macro("indexconfname", DM_VAR, compress(conflist[idx].name));
            confsep(expand("confindexmsg", DM_VAR), idx,st,part3,0);
         }


/*
         printf("%s", desclist[i].name);
         for (j=strlen(desclist[i].name); j<22; j++)
            putchar('.');
         printf("%s\n", desclist[i].location);
         confsep(expand("confindexmsg", DM_VAR),idx,st,part,0);
*/
      }
      st_glob.string[0]='\0';
   }
   return 1;
}
