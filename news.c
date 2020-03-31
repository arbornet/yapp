/* $Id: news.c,v 1.16 1998/02/10 13:25:26 kaylene Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for fork, etc */
#endif
#include "yapp.h"
#include "struct.h"
#include "news.h"
#include "arch.h"
#include "item.h"
#include "sep.h"
#include "range.h"
#include "sum.h"
#include "files.h"
#include "globals.h"
#include "xalloc.h"
#include "lib.h"
#include "stats.h"
#include "rfp.h"   /* for add_response */
#include "macro.h" /* for DM_VAR */
#include "license.h" /* for get_conf_param() */
#include "dates.h"   /* for do_getdate() */
#include "conf.h"    /* for security_type() */
#include "user.h"    /* for get_user() */

/******************************************************************************/
/* ADD .SIGNATURE TO A MAIL MESSAGE                                           */
/******************************************************************************/
int
make_emtail()
{
   FILE        *fp,*sp;
   char         buff[MAX_LINE_LENGTH];
   register int cpid,wpid;
   int          statusp;

   fflush(stdout);
   if (status & S_PAGER)
      fflush(st_glob.outp);

   /* Fork & setuid down when creating cf.buffer */
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
      if ((fp=mopen(buff,O_A))==NULL)
         return 1;

      sprintf(buff,"%s/.signature",home);
      if ((sp = mopen(buff,O_R|O_SILENT)) != NULL) {
         fprintf(fp,"--\n");
         while (ngets(buff,sp))
            fprintf(fp,"%s\n",buff);
         mclose(sp);
      }
      mclose(fp);
      exit(0);
   }
   return 1;
}

/******************************************************************************/
/* CREATE EMAIL HEADER                                                        */
/******************************************************************************/
int
make_emhead(re,par)
response_t *re;
short par;
{
   FILE        *fp,*pp;
   flag_t       ss;
   char         buff[MAX_LINE_LENGTH];
   register int cpid,wpid;
   int          statusp;
   char        *sub;
   char       **config;

   sub = get_subj(confidx, st_glob.i_current-1, sum);
   if (!(config = get_config(confidx)))
      return 0;

   fflush(stdout);
   if (status & S_PAGER)
      fflush(st_glob.outp);

   /* Fork & setuid down when creating cf.buffer */
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
/*         xfree_array( re[i].text ); */
         return 1;
      }

      pp = st_glob.outp;
      ss = status;
      st_glob.r_current = par-1;
      st_glob.outp = fp;
      status |= S_PAGER;
 
      itemsep(expand("mailheader", DM_VAR),0);
      if (par)
         dump_reply("mailsep");

      st_glob.outp = pp;
      status     = ss;
      mclose(fp);
      exit(0);
   }
   return 1;
}
/*
      if (par>0)
         fprintf(fp,"References: <%s>\n", message_id(compress(
          conflist[confidx].name), st_glob.i_current, curr,re));
*/

#ifdef NEWS

/* So far each item can only have 1 response */

/******************************************************************************/
/* CREATE NEWS HEADER                                                         */
/******************************************************************************/
int
make_rnhead(re,par)
response_t *re;
short par;
{
   FILE *fp,*pp,*sp;
   flag_t ss;
   char buff[MAX_LINE_LENGTH];
   short curr;
   register int cpid,wpid;
   int statusp;
   char *sub;
   char **config;

   sub = get_subj(confidx, st_glob.i_current-1, sum);
   if (!(config = get_config(confidx)))
      return 0;

   fflush(stdout);
   if (status & S_PAGER)
      fflush(st_glob.outp);

   /* Fork & setuid down when creating .rnhead */
   if (cpid=fork()) { /* parent */
      if (cpid<0) return -1; /* error: couldn't fork */
      while ((wpid = wait(&statusp)) != cpid && wpid != -1);
      /* post = !statusp; */
   } else { /* child */
      if (setuid(getuid())) error("setuid","");
      setgid(getgid());

      sprintf(buff,"%s/.rnhead",home);
      if (!(fp = mopen(buff,O_W))) exit(1);
      fprintf(fp,"Newsgroups: %s\n",config[CF_NEWSGROUP]);
      fprintf(fp,"Subject: %s%s\n",(sub)?"Re: ":"", 
       (sub)? sub : "" );
      fprintf(fp,"Summary: \nExpires: \n");
   
curr = par-1;
      /* add parents mids:
      fprintf(fp,"References:"); 
      buff[0]='\0';
      for (curr = par-1; curr>=0; curr = re[curr].parent-1) {
         sprintf(buff2," <%s>%s", message_id(compress(conflist[confidx].name),
             st_glob.i_current,curr,re), buff); 
         strcpy(buff,buff2);   
      }
      fprintf(fp,"%s\n",buff);
      */
      if (par>0)
         fprintf(fp,"References: <%s>\n", message_id(compress(
     conflist[confidx].name), st_glob.i_current, curr,re));

      fprintf(fp,"Sender: \nFollowup-To: \nDistribution: world\n");
      fprintf(fp,"Organization: \nKeywords: \n\n");
      if (par > 0) { /* response to something? */
         pp = st_glob.outp;
         ss = status;
         st_glob.r_current = par-1;
         st_glob.outp = fp;
         status |= S_PAGER;
   
         dump_reply("newssep");
   
         st_glob.outp = pp;
         status     = ss;
   
         /* dump_reply("newssep"); don't dump to screen */
      }
      sprintf(buff,"%s/.signature",home);
      if (sp = mopen(buff,O_R|O_SILENT)) {
         fprintf(fp,"--\n");
         while (ngets(buff,sp))
       fprintf(fp,"%s\n",buff);
         mclose(sp);
      }
      mclose(fp);
      exit(0);
   }
   return 1;
}
#endif

/*
 * Take some test and header info, and attempt to put it into a
 * given conference.  If the conference type has the CT_REGISTERED
 * bit set, make sure the author is a registered user first.
 */
static void
incorporate2(art, sum, part, stt, idx, this, sj, text, mid, fromL, fromF)
   long         art;      /*    Article number to incorporate */
   sumentry_t  *sum;      /*    Item summary array to fill in */
   partentry_t *part;     /*    Participation info            */
   short        idx;      /*    Conference index              */
   status_t    *stt;
   sumentry_t  *this;
   char        *sj,       /*    Subject */
               *mid,      /*    Unique message ID */
               *fromL,    /*    Login of author */
               *fromF,    /*    Fullname of author */
              **text;     /*    The actual body of text */
{
   int i, sec;
   char **config;

   /* Get conference security type */
   if (!(config=get_config(idx)))
      return;
   sec = security_type(config, idx);

   /* If registered bit is set, make sure fromL is a registered user */
   if (sec & CT_REGISTERED) {
      char **parts = explode(fromL, "@", 1);
      char fullname[MAX_LINE_LENGTH];
      char home[MAX_LINE_LENGTH];
      char email[MAX_LINE_LENGTH];
      int  found=0;
      int  uid = 0;

      /* Check if fromL is a Unix user */
      if (get_user(&uid, parts[0], fullname, home, email)) {
         /* a local user was found with that login */

         if (xsizeof(parts)==1 || !strcasecmp(parts[1], hostname)) {
            /* Unix user found */
            found = 1;
         } else if (!strcasecmp(fromL, email)) {
            /* Local user found the fast way: local login == remote login */
            found = 2;
         }
      }

      /* Check if fromL is a registered user with that email address */
      if (!found && email2login(fromL))
         found = 3;
      
      xfree_array(parts);

      /* If not registered, skip this conference (don't post) */
      if (!found)
         return;
   }

         /* Find what item it should go in */
         i=stt->i_last+1;
      /* Duplicate subjects are separate items */
      /* if (!strncmp(sj,"Re: ",4)) */
         for (i=stt->i_first; 
              i<=stt->i_last && (!sum[i-1].nr || !sum[i-1].flags
             || (strncasecmp(sj+4, get_subj(idx,i-1,sum), MAX_LINE_LENGTH) 
              && strncasecmp(sj,   get_subj(idx,i-1,sum), MAX_LINE_LENGTH))); 
            i++);
      /* Duplicate subjects in same item 
         for (i=stt->i_first; 
              i<=stt->i_last && (!sum[i-1].nr 
          || ((strcmp(sj+4, get_subj(idx,i-1,sum)) || strncmp(sj,"Re: ",4)) 
           && strcmp(sj, get_subj(idx, i-1, sum)))); 
         i++);
       */
         if (i>stt->i_last) {
            i=stt->i_last+1;
      
            /* Enter a new item */
      /* printf("%d Subject '%s' is new %s %d\n",art,sj,topic(0),i); */
				if (!(flags & O_INCORPORATE))
               printf(".");
            this->nr = 1;
            this->flags = IF_ACTIVE;
            do_enter(this,sj,text,idx,sum,part,stt,art,mid,
             uid,fromL,fromF);
            store_subj(idx, i-1, sj);
         } else {
            short resp=0;
      
            /* KKK Find previous reference for parent */
      
            /* Response to item i */
      /* printf("%d Subject '%s' is response to %s %d\n",art,sj,topic(0),i); */
				if (!(flags & O_INCORPORATE))
               printf(".");
            stt->i_current = i;
            add_response(this,text,idx,sum,part,stt,art,
             mid,uid,fromL,fromF,resp);
      
         }
}

/******************************************************************************/
/* INCORPORATE: Incorporate a new article (art) into an item                */
/******************************************************************************/
int                    /* RETURNS: 1 on valid, 0 else      */
incorporate(art,sum,part,stt,idx) /* ARGUMENTS: */
long         art;      /*    Article number to incorporate */
sumentry_t  *sum;      /*    Item summary array to fill in */
partentry_t *part;     /*    Participation info            */
short        idx;      /*    Conference index              */
status_t    *stt;
{
   FILE       *fp=NULL;
   char        path[MAX_LINE_LENGTH];
   char        buff[MAX_LINE_LENGTH];
   char        sj2[MAX_LINE_LENGTH], mid[MAX_LINE_LENGTH],*sj,
               fromF[MAX_LINE_LENGTH],fromL[MAX_LINE_LENGTH];
   short       i, j, k;
   sumentry_t  this;
   char      **tolist[3],
             **text=NULL,
              *str;
   assoc_t     *maillist=NULL;
   int         maxmail=0;
   int         placed=0;

	if (flags & O_INCORPORATE) {
		if ((maillist=grab_list(bbsdir,"maillist",0))==NULL)
		   return 0;
                maxmail = xsizeof(maillist)/sizeof(assoc_t);
		fp = st_glob.inp; /* st_glob.inp; */
		tolist[0] = tolist[1] = tolist[2] = 0;
#ifdef NEWS
	} else {
      /* Load in Subject and Date */
      if (!(config = get_config(idx)))
         return 0;
      sprintf(path,"%s/%s/%ld",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]),art);
      if ((fp = mopen(path,O_R))==NULL) return 0;
		text = NULL;
#endif
	}

   sj = sj2;
   do {
      ngets(buff,fp); 
      if (!strncmp(buff,"Subject: ",9)) {

         if (!strncasecmp(buff+9,"Re: ",4))
            strcpy(sj, buff+13);
         else
            strcpy(sj, buff+9);
         while (sj[ strlen(sj)-1 ]==' ')
            sj[ strlen(sj)-1 ]=0;
         while (*sj==' ') 
            sj++;

      } else if ((flags & O_INCORPORATE) && !strncmp(buff,"To: ",3))  {
				tolist[0] = explode(buff+4, ", ", 1);
      } else if ((flags & O_INCORPORATE) && !strncmp(buff,"Cc: ",3))  {
				tolist[1] = explode(buff+4, ", ", 1);
      } else if ((flags & O_INCORPORATE) && !strncmp(buff,"Resent-To: ",10))  {
				tolist[2] = explode(buff+11, ", ", 1);
			/* KKK similarly for newsgroups? */

      } else if (!strncmp(buff,"Date: ",6)) {
            do_getdate(&(this.last),buff+6);
            this.first = this.last;
      } else if (!strncmp(buff,"From: ",6)) {
         char *p,*q;
         char *ptr; /* Arbitrary length string from noquote */

         if ((p = strchr(buff+6,'(')) != NULL) { /* login (fullname) */
            sscanf(buff+6,"%s",fromL);
            q = strchr(p,')');
            strncpy(fromF,p+1, q-p-1);
            fromF[q-p-1]='\0';
         } else if ((p = strchr(buff+6,'<')) != NULL) { /* "fullname" <login> */
            strncpy(path,buff+6, p-buff-7);
            path[p-buff-7]='\0';
            ptr =noquote( path, 0 );
            strcpy(fromF, ptr);
            xfree_string(ptr);
            q = strchr(p,'>');
            strncpy(fromL,p+1, q-p-1);
            fromL[q-p-1]='\0';
         } else { /* login */
            strcpy(fromL,buff+6);
            strcpy(fromF,fromL);
         }
      } else if (!strncasecmp(buff,"Message-ID: <",13)) {
         char *p;
         p = strchr(buff,'>');
         *p='\0';
         strcpy(mid,buff+13);
      }
   } while (strlen(buff)); /* until blank line */
	if (flags & O_INCORPORATE)
		text = grab_more(fp,NULL, 0,NULL);
   else /* for INCORPORATE, fp is stdin */
      mclose(fp);

   /* Incorporate message into each conference */
	if (flags & O_INCORPORATE) {
	for (k=0; k<=2; k++) {
		if (!tolist[k])
			continue;

   	for (j=0; j<xsizeof(tolist[k]); j++) {

         for (str = tolist[k][j]; *str == '<'; str++);
         while (str[ strlen(str)-1 ]== '>')
            str[ strlen(str)-1 ] = 0;

   	   i = get_idx(str, maillist, maxmail);
     		if (i<0) 
   			continue;
         idx = get_idx(maillist[i].location, conflist, maxconf);
   		if (idx<0) 
     			continue;

   		get_status(&st_glob,sum,part,idx); /* read in sumfile */
         incorporate2(art, sum, part, stt, idx, &this,sj,text,mid,fromL,fromF);
           placed=1;
   	}
		xfree_array(tolist[k]);
	}
        if (!placed) {
           if ((idx = get_idx(maillist[0].location, conflist, maxconf)) > 0) {
              get_status(&st_glob,sum,part,idx); /* read in sumfile */
              incorporate2(art, sum, part, stt, idx, &this, sj, text, mid, 
               fromL, fromF);
           }
        }
   } else 
      incorporate2(art, sum, part, stt, idx, &this, sj, text, mid, fromL,fromF);

	/* Free up text, mail list */
	if (text)
		xfree_array(text);

   free_list(maillist);
							 
   return 1;
}

#ifdef NEWS
void
news_show_header()
{
   short pr;
   FILE *fp;
   char  buff[MAX_LINE_LENGTH];
   char **config;

   open_pipe();
   if (!(config = get_config(confidx)))
      return;
   sprintf(buff,"%s/%s/%d",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]),st_glob.i_current);
   if (fp=mopen(buff,O_R)) {
      st_glob.r_current = 0; /* current resp = header */
      get_resp(fp,re,(short)GR_HEADER,(short)0); /* Get header of #0 */
/* The problem here is that itemsep uses r_current as an index to
   the information in re, so we can't show # new responses
      st_glob.r_current = sum[st_glob.i_current-1].nr
       - abs(part[st_glob.i_current-1].nr);
 */

      /* Get info about the actual item text if possible */
      if (re[st_glob.r_current].flags & (RF_EXPIRED|RF_SCRIBBLED))
    pr = 0;
      else if (re[st_glob.r_current].flags & RF_CENSORED)
         pr = ((st_glob.opt_flags & OF_NOFORGET) ||!(flags & O_FORGET));
      else
     pr = 1;

      if (pr) get_resp(fp,&(re[st_glob.r_current]),(short)GR_ALL,st_glob.r_current);
      if ((re[st_glob.r_current].flags & RF_SCRIBBLED)
       && re[st_glob.r_current].numchars>7) {
         fseek(fp,re[st_glob.r_current].textoff,0);
         ngets(buff,fp);
         re[st_glob.r_current].text = (char**)buff;
      }

      if (flags & O_LABEL)
         sepinit(IS_CENSORED|IS_UID|IS_DATE);
      itemsep(expand((st_glob.opt_flags & OF_SHORT)?"ishort":"isep", DM_VAR),0);
      if (pr) xfree_array(re[st_glob.r_current].text);
      st_glob.r_current = 0; /* current resp = header */
      mclose(fp);
   }
}

/******************************************************************************/
/* GENERATE UNIQUE MESSAGE ID FOR A RESPONSE                                  */
/******************************************************************************/
char *
message_id(c,i,r,re)
char       *c;
short       i,r;
response_t *re;      /*    Response to make id for     */
{
   char str[MAX_LINE_LENGTH];

   if (!re[r].mid) {
/*    sprintf(str,"%d.%d.%X@%s",r,re[r].uid,re[r].date,hostname); */
      sprintf(str,"%s.%d@%s", get_date(re[r].date,14), re[r].uid, hostname);
      re[r].mid = xstrdup(str);
   }
   return re[r].mid;
}

/******************************************************************************/
/* GET AN ACTUAL ARTICLE                                                      */
/******************************************************************************/
void                     /* RETURNS: (nothing)             */
get_article(re)      /* ARGUMENTS                      */
response_t *re;          /*    Response to fill in         */
{                        /* LOCAL VARIABLES:               */
   char       buff[MAX_LINE_LENGTH];
   char       done=0;
   FILE      *fp;        /*    Article file pointer        */
   char     **config;

   re->text = NULL;
   if (!(config = get_config(confidx))) {
      re->flags  |= RF_EXPIRED;
      return;
   }
   sprintf(buff,"%s/%s/%d",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]),re->article);
   if ((fp=mopen(buff,O_R|O_SILENT))==NULL) {
      re->flags  |= RF_EXPIRED;
      /* anything else? */
      return;
   }

   /* Get response */
   re->flags   = 0;
   if (re->mid) { xfree_string(re->mid); re->mid = NULL; }
   re->parent = re->article = 0;

   while (!done && !(status & S_INT)) {
      if (!ngets(buff,fp)) {
         done=1; 
         break; 
      }

#if 0
      if (!strncmp(buff,"From: ",6)) {
         char *p,*q;

         if (p = strchr(buff+6,'(')) { /* login (fullname) */
            sscanf(buff+6,"%s",n1);
            q = strchr(p,')');
            strncpy(n2,p+1, q-p-1);
            n2[q-p-1]='\0';
    } else if (p = strchr(buff+6,'<')) { /* fullname <login> */
            strncpy(n2,buff+6, p-buff-6);
            n2[p-buff-6]='\0';
            q = strchr(p,'>');
            strncpy(n1,p+1, q-p-1);
            n1[q-p-1]='\0';
    } else { /* login */
            strcpy(n1,buff+6);
            strcpy(n2,n1);
    }

         re->uid = 0;
         if (re->login) xfree_string(re->login);
         re->login = xstrdup(n1);
         if (re->fullname) xfree_string(re->fullname);
         re->fullname=xstrdup(n2);
      } else if (!strncmp(buff,"Date: ",6)) {
         do_getdate(&(re->date),buff+11);
      } else 
#endif
      if (!strncmp(buff,"Message-ID: <",13)) {
       char *p;
         p = strchr(buff,'>');
       *p='\0';
       re->mid = xstrdup(buff+13);
      } else if (!strlen(buff)) {
     long textoff;

          textoff = ftell(fp); 
          re->text = grab_more(fp,(flags & O_SIGNATURE)?NULL:"--",0,NULL);
          re->numchars= ftell(fp) - textoff;
          done=1;
          break;
      }
   }
   mclose(fp);
}

/******************************************************************************/
/* DOT2SLASH: Return directory/string/form of news.group.string passed in     */
/******************************************************************************/
char *         /* RETURNS: Slash-separated string */
dot2slash(str) /* ARGUMENTS:                      */
char *str;     /*    Dot-separated string         */
{
static char buff[MAX_LINE_LENGTH];
       char *f,*t;

   for (f=str,t=buff; *f; f++,t++) {
      *t = (*f == '.')? '/' : *f;
   }
   *t = '\0';
   return buff;
}

/******************************************************************************/
/* REFRESH_NEWS: Look for any new articles, and if any are found, incorporate */
/* them into item files                                                       */
/******************************************************************************/
void                                     /* RETURNS: (nothing)             */
refresh_news(sum,part,stt,idx) /* ARGUMENTS:                     */
sumentry_t  *sum;                        /*    Summary array to update     */
partentry_t *part;                       /*    Participation info          */
short        idx;                        /*    Conference index to update  */
status_t    *stt;                        /*    Status info to update       */
{
   char           path[MAX_LINE_LENGTH],
                  artpath[MAX_LINE_LENGTH],
                  fmt[MAX_LINE_LENGTH];
   struct stat    st;
   char         **config;
   long           article;
   DIR           *fp;
   FILE          *artp;
   struct dirent *dp;
   int            i;

   strcpy(fmt,"%d");
   if (!(config = get_config(idx)))
      return;

   sprintf(path,"%s/%s",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]));
   if (stat(path,&st)) {
      error("refreshing ",path);
      return;
   }

   /* Is there new stuff? */
   if (st.st_mtime!=stt->sumtime) {
      long mod;
      struct stat artst;

      sprintf(artpath,"%s/article",conflist[idx].location);

      /* Create if doesn't exist, else update */
      if (stat(artpath,&artst)) mod = O_W;
      else                      mod = O_RPLUS;

      /* if (stt->c_security & CT_BASIC) mod |= O_PRIVATE;*/
      if ((artp=mopen(artpath, mod))==NULL) return;  /* can't lock */

      if ((fp = opendir(path))==NULL) {
         error("opening ",path);
         return;
      }
      refresh_stats(sum,part,stt); /* update stt */
     
      /* Load in stats 1 piece at a time - the slow stuff */
      article = stt->c_article;
      for (dp = readdir(fp); dp != NULL && !(status & S_INT); dp=readdir(fp)) {
         long i2;
         if (sscanf(dp->d_name,fmt,&i2)==1 && i2>stt->c_article) {
            incorporate(i2,sum,part,stt,idx);
            if (i2>article) {
               article=i2;
               fseek(artp, 0L, 0);
               fprintf(artp,"%ld\n",article);
            }
            refresh_stats(sum,part,stt); /* update stt */
         }
      }
      closedir(fp);

      /* Check for expired */
      for (i=stt->i_first; i<=stt->i_last; i++) {
         response_t re;
         FILE *fp2;
         char buff[MAX_LINE_LENGTH];

         sprintf(buff,"%s/_%d",conflist[idx].location,i);
         if (fp2=mopen(buff,O_R)) {
            re.fullname = re.login = re.mid = 0;
            re.text = 0;
            re.offset = -1;

            get_resp(fp2, &re, GR_ALL, 0);
            if (re.flags & RF_EXPIRED) {
               sum[i-1].flags |= IF_EXPIRED;
               dirty_sum(i-1);
            }
            mclose(fp2);

            if (re.fullname) xfree_string(re.fullname);
            if (re.login)    xfree_string(re.login);
            if (re.text)     xfree_array(re.text);
            if (re.mid)      xfree_string(re.mid);
         }
      }

      stt->sumtime   = st.st_mtime; 
      stt->c_article = article;
      refresh_stats(sum,part,stt); /* update stt */
      save_sum(sum,0,idx,stt);

      mclose(artp); /* release lock on article file */
   }
}
#endif

