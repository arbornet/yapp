/* $Id: item.c,v 1.30 1998/02/09 19:46:03 kaylene Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h> /* for sys/stat.h on ultrix */
#include <sys/stat.h> /* for stat() */
#ifdef HAVE_STDLIB_H
# include <stdlib.h> /* for getenv() */
#endif
#include <ctype.h>  /* for tolower() */
#include <errno.h>
#include <memory.h> /* for memcpy() */
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for link() */
#endif
#include "yapp.h"
#include "struct.h"
#include "macro.h"
#include "item.h"
#include "range.h"
#include "globals.h"
#include "lib.h"
#include "arch.h"
#include "system.h"
#include "sum.h"
#include "sep.h"
#include "conf.h"   /* for security_type() */
#include "security.h"
#include "xalloc.h"
#include "edbuf.h"
#include "driver.h"
#include "files.h"
#include "news.h"
#include "stats.h" /* for get_config */
#include "license.h" /* for get_conf_param() */
#include "main.h" /* for wgets() */
#include "joq.h" /* for write_part() */

FILE *ext_fp;
short ext_first,ext_last; /* for 'again' */

/*
 * Test to see if the current user entered the given item in the current
 * conference 
 */
int
is_enterer(item)
   int item; /* item number      */
{
   char buff[MAX_LINE_LENGTH];
   response_t tempre[MAX_RESPONSES];
   FILE *fp;

   sprintf(buff,"%s/_%d",conflist[confidx].location,item);
   if (!(fp=mopen(buff,O_R))) 
      return 0;
   get_item(fp,item,tempre,sum);
   get_resp(fp,re,(short)GR_HEADER,(short)0); /* Get header of #0 */
   mclose(fp);
   return (re[0].uid==uid) && !strcmp(re[0].login,login);
}

/******************************************************************************/
/* CREATE A NEW ITEM                                                          */
/******************************************************************************/
int
do_enter(this,sub,text,idx,sum,part,stt,art,mid,uid,login,fullname)
   sumentry_t  *this; /*   New item summary */
   char        *sub;  /*   New item subject */
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
{
   short item,line;
   FILE *fp;
   char buff[MAX_LINE_LENGTH];
   long mod;

   /* item = ++(stt->i_last);  * next item number */
   item = ++(stt->c_confitems); /* next item number */
   if (!art && !(flags & O_QUIET)) 
      printf("Saving as %s %d...",topic(0), item);
if (debug & DB_ITEM)
printf("%s flags=%d sub=!%s!\n", topic(0), this->flags, sub);
   sprintf(buff,"%s/_%d",conflist[idx].location,item);
   mod = O_W|O_EXCL;
   if (stt->c_security & CT_BASIC) mod |= O_PRIVATE;
   if ((fp=mopen(buff,mod))==NULL) return 0;
   fprintf(fp,"!<ps02>\n,H%s%s\n,R%04X\n,U%d,%s\n,A%s\n,D%08X\n,T\n",
      sub,spaces(atoi(get_conf_param("padding",PADDING))-strlen(sub)),
      RF_NORMAL,uid,login,fullname,this->first);

   if (art) {
      fprintf(fp,",N%06ld\n",art);
      fprintf(fp,",M%s\n",mid);
   } else {
      for (line=0; line<xsizeof(text); line++) {
         if (text[line][0]==',') fprintf(fp,",,%s\n",text[line]);
         else                    fprintf(fp,"%s\n",text[line]);
      }
   }
   if (fprintf(fp,",E%s\n", spaces(atoi(get_conf_param("padding",PADDING)))) >=0) {
      time(&(part[item-1].last));
      part[item-1].nr = this->nr;
      dirty_part(item-1);
   } else
      error("writing ",topic(0));

   /* If sum doesn't exist, we must make sure the data has
    * been written before the sum file is created.
    */
   fflush(fp);
   memcpy(&sum[item-1],this,sizeof(sumentry_t));
   save_sum(sum,(short)(item-1),idx,stt);
   dirty_sum(item-1);
   mclose(fp);
   store_subj(idx, item-1, sub);
   store_auth(idx, item-1, login);
#ifdef SUBJFILE
   update_subj(idx, item-1); /* update subjects file */
#endif
   if (!art && !(flags & O_QUIET)) 
      wputs("saved.\n");
   stt->i_numitems++;
   return 1;
}

/******************************************************************************/
/* ENTER A NEW ITEM                                                           */
/******************************************************************************/
int              /* RETURNS: (nothing)     */
enter(argc,argv) /* ARGUMENTS:             */
int    argc;     /*    Number of arguments */
char **argv;     /*    Argument list       */
{
   char buff[MAX_LINE_LENGTH],sub[MAX_LINE_LENGTH],**file;
   char cfbufr[MAX_LINE_LENGTH];
   sumentry_t this;

#if 0
   if (st_glob.c_status & CS_NORESPONSE) {
      printf("You only have readonly access.\n");
      return 1;
   }
#endif

   if (argc>1) {
      printf("Bad %s range near \"%s\"\n",topic(0), argv[1]);
      return 1;
   }

   refresh_sum(0,confidx,sum,part,&st_glob);

   if (!check_acl(ENTER_RIGHT, confidx)) {
      printf("You are not allowed to enter new %ss.\n", topic(0));
      return 1;
   }
#if 0
   /* If users aren't allowed to enter and they're not a FW, then stop */
   if (!(st_glob.c_status & CS_FW) && (st_glob.c_security & CT_NOENTER)) {
      sprintf(buff,"Only %ss can enter new %ss.  Sorry!\n", fairwitness(0), 
       topic(0));
      wputs(buff);
      return 1;
   }
#endif

   if (!st_glob.i_numitems && !(st_glob.c_status & CS_FW)) {
      sprintf(buff, "Only the %s can enter the first %s.  Sorry!\n", 
       fairwitness(0), topic(0));
      wputs(buff);
      return 1;
   }
   if (st_glob.i_last == MAX_ITEMS) { /* numbered 1-last */
      sprintf(buff, "Too many %ss.\n", topic(0));
      wputs(buff);
      return 1;
   }

#ifdef NEWS
   if (st_glob.c_security & CT_NEWS) {
      char rnh[MAX_LINE_LENGTH];

      make_rnhead(re,0);
      sprintf(rnh,"%s/.rnhead",home);
      sprintf(buff,"Pnews -h %s",rnh);
      unix_cmd(buff);
      rm(rnh,SL_USER);
      return 1;
   } 
#endif

   sprintf(cfbufr,"%s/cf.buffer",work);
   if (text_loop(1, "text")) {
      if (!(file = grab_file(work,"cf.buffer",0))) 
         wputs("can't open cfbuffer\n");
      else {
         /* Get the subject */
         do {
            if (!(flags & O_QUIET))
               printf("Enter a one line %s or ':' to edit\n? ", subject(0));
            ngets(sub, st_glob.inp); /* st_glob.inp */

            if (!strcmp(sub,":")) {
               xfree_array(file);

               if (!edit(cfbufr,NULL,0) 
                || !(file = grab_file(work,"cf.buffer",0))) {
                  wputs("can't open cfbuffer\n");
                  return 1;
               }
               sub[0]=0;
               continue;
            }
            sprintf(buff, "Ok to abandon %s entry? ", topic(0));
            if (!strlen(sub) && get_yes(buff, DEFAULT_ON)) {
               rm(cfbufr,SL_USER);
               sprintf(buff, "No text -- %s entry aborted!\n", topic(0));
               wputs(buff);
               xfree_array(file);
               return 1;
            }
         } while (!strlen(sub));

         /* Expand seps in subject IF first character is % */
         if (sub[0]=='%') {
            char *str, *f;
            str = sub+1;
            f = get_sep(&str);
            strcpy(sub, f);
         }

         /* Replace any control characters with spaces */
         {
            register char *p;
            for (p=sub; *p; p++)
               if (iscntrl(*p))
                  *p = ' ';
         }

         /* Verify item entry */
         sprintf(buff, "Ok to enter this %s? ", topic(0));
         if (!get_yes(buff, DEFAULT_OFF)) {
            rm(cfbufr,SL_USER);
            sprintf(buff, "No text -- %s entry aborted!\n", topic(0));
            wputs(buff);
            xfree_array(file);
            return 1;
         }

         this.nr = 1;
         this.flags = IF_ACTIVE;
         this.last  = time(&( this.first ));
   
         /* make sure nothing changed since we started */
         refresh_sum(0,confidx,sum,part,&st_glob);
         /* dont need to free anything */

         if (st_glob.c_security & CT_EMAIL)  {
            char cfbufr2[MAX_LINE_LENGTH];

            sprintf(cfbufr2, "%s.tmp", cfbufr);
            move_file(cfbufr, cfbufr2, SL_USER);

            store_subj(confidx, st_glob.i_numitems, sub);
            st_glob.i_current = st_glob.i_numitems+1;
            make_emhead(re,0);

            sprintf(buff, "cat %s >> %s", cfbufr2, cfbufr);
            unix_cmd(buff);
            rm(cfbufr2, SL_USER);

            make_emtail();
            sprintf(buff, "%s -t < %s", get_conf_param("sendmail",SENDMAIL),
             cfbufr);
            unix_cmd(buff);
         } else {
            do_enter(&this,sub,file,confidx,sum,part,&st_glob,0,NULL,
             uid,login, fullname_in_conference(&st_glob));
         }

         xfree_array(file);

         refresh_stats(sum,part,&st_glob);
         st_glob.i_current = st_glob.i_last;
         custom_log("enter", M_RFP);
      }
   }
   rm(cfbufr,SL_USER);
   return 1;
}

/******************************************************************************/
/* DISPLAY CURRENT ITEM HEADER                                                */
/* Only requires st_glob.i_current to be set, nothing else                    */
/******************************************************************************/
void
show_header()
{
   FILE *fp;
   char  buff[MAX_LINE_LENGTH];
   register int scrib=0;

   open_pipe();
   sprintf(buff,"%s/_%d",conflist[confidx].location,st_glob.i_current);
   if ((fp=mopen(buff,O_R)) != NULL) {
      st_glob.r_current = 0; /* current resp = header */
      get_item(fp,st_glob.i_current,re,sum); 
      get_resp(fp,re,(short)GR_HEADER,(short)0); /* Get header of #0 */
/* The problem here is that itemsep uses r_current as an index to
   the information in re, so we can't show # new responses
      st_glob.r_current = sum[st_glob.i_current-1].nr
       - abs(part[st_glob.i_current-1].nr);
 */

      /* Get info about the actual item text if possible */
      if (!(re[st_glob.r_current].flags & (RF_EXPIRED|RF_SCRIBBLED)))
         get_resp(fp,&(re[st_glob.r_current]),(short)GR_ALL,st_glob.r_current);

      if ((re[st_glob.r_current].flags & RF_SCRIBBLED)
       && re[st_glob.r_current].numchars>7) {
         fseek(fp,re[st_glob.r_current].textoff,0);
         ngets(buff,fp);
         re[st_glob.r_current].text = (char**)buff;
         scrib=1;
      }

      if (flags & O_LABEL)
         sepinit(IS_CENSORED|IS_UID|IS_DATE);
      itemsep(expand((st_glob.opt_flags & OF_SHORT)?"ishort":"isep", DM_VAR),0);

      /* If we've allocated the text, free it */
      if (re[st_glob.r_current].text && !scrib)
         xfree_array(re[st_glob.r_current].text);

      st_glob.r_current = 0; /* current resp = header */
      mclose(fp);
   }
}

void
show_nsep(fp)
FILE *fp;
{
   short tmp,i=st_glob.r_first;
   tmp = st_glob.r_current;

   if (st_glob.since) {
      while (i<sum[st_glob.i_current-1].nr && st_glob.since > re[i].date) {
         i++;
         get_resp(fp,&(re[i]),(short)GR_HEADER,i);
         if (i>=sum[st_glob.i_current-1].nr) break;
      }
   } else if (!st_glob.r_first)
      i = abs(part[st_glob.i_current-1].nr);
   if (!i) i++;

   st_glob.r_current = sum[st_glob.i_current-1].nr-i; /* calc # new */
   itemsep(expand("nsep", DM_VAR),0);
   st_glob.r_current = tmp;
}

/******************************************************************************/
/* SHOW CURRENT RESPONSE                                                      */
/******************************************************************************/
void
show_resp(fp)
   FILE *fp;
{
   char buff[MAX_LINE_LENGTH];
   int pr=0;
      
   /* Get resp to initialize stats like %s (itemsep) */
   get_resp(fp,&(re[st_glob.r_current]),(short)GR_HEADER,st_glob.r_current);
   if (!(re[st_glob.r_current].flags & (RF_EXPIRED|RF_SCRIBBLED))) {
      get_resp(fp,&(re[st_glob.r_current]),(short)GR_ALL,st_glob.r_current);
      if (re[st_glob.r_current].flags & RF_CENSORED) 
         pr = ((st_glob.opt_flags & OF_NOFORGET) || !(flags & O_FORGET));
      else 
         pr = 1;
   }

   if ((re[st_glob.r_current].flags & RF_SCRIBBLED )
    && re[st_glob.r_current].numchars>7) {
      fseek(fp,re[st_glob.r_current].textoff,0);
      ngets(buff,fp);
      re[st_glob.r_current].text = (char**)buff;
   }

   /* Print response header */
   if (st_glob.r_current) {
      if (flags & O_LABEL)
         sepinit(IS_PARENT|IS_CENSORED|IS_UID|IS_DATE);
      itemsep(expand("rsep", DM_VAR),0);
   }

   /* Print response text */
   /* read short could do response headers only to browse resps */
   /* if (pr && !(st_glob.opt_flags & OF_SHORT)) */
   if (pr) {
      if (!st_glob.r_current) wfputc('\n',st_glob.outp);
      for (st_glob.l_current=0;
           st_glob.l_current<xsizeof(re[st_glob.r_current].text)
        && !(status & S_INT); 
           st_glob.l_current++) {
         itemsep(expand("txtsep", DM_VAR),0);
      }
   }

   /*
    * Unless response was scribbled (in which case text holds the scribbler),
    * free up the response text.
    */
   if (!(re[st_glob.r_current].flags & RF_SCRIBBLED))
      xfree_array(re[st_glob.r_current].text);
      
   /* Count it as seen */
   if (st_glob.r_lastseen<=st_glob.r_current)
      st_glob.r_lastseen=st_glob.r_current+1;
}

/******************************************************************************/
/* SHOW RANGE OF RESPONSES                                                    */
/******************************************************************************/
void
show_range()
{
   FILE *fp;
   char  buff[MAX_LINE_LENGTH];

   if (st_glob.r_first > st_glob.r_last) return; /* invalid range */
   refresh_sum(st_glob.i_current,confidx,sum,part,&st_glob);
   /* in case new reponses came */
   st_glob.r_max=sum[ st_glob.i_current-1 ].nr-1; 
   st_glob.r_current = sum[st_glob.i_current-1].nr
    - abs(part[st_glob.i_current-1].nr);

   open_pipe();
   sprintf(buff,"%s/_%d",conflist[confidx].location,st_glob.i_current);
   if ((fp=mopen(buff,O_R)) != NULL) {

/*    if (!st_glob.r_current && sum[st_glob.i_current-1].nr>1) */
/*    if ( st_glob.r_first   && sum[st_glob.i_current-1].nr>1)    */

      /* For each response */
      for (st_glob.r_current = st_glob.r_first;
           st_glob.r_current<= st_glob.r_last 
            && st_glob.r_current<=st_glob.r_max
            && !(status & S_INT);
           st_glob.r_current++) {
         if (st_glob.r_first==0 && st_glob.r_current==1)
            show_nsep(fp); /* nsep between resp 0 and 1 */
         show_resp(fp);
      }
      status &= ~S_INT; /* Int terminates range */

      mclose(fp);
      st_glob.r_current--;
      itemsep(expand("zsep", DM_VAR),0);
   }
}

/******************************************************************************/
/* READ THE CURRENT ITEM                                                      */
/******************************************************************************/
void
read_item()
{
   FILE *fp;
   short i_lastseen;    /* Macro for current item index */
   char  buff[MAX_LINE_LENGTH];
   short oldnr,   topt_flags;
   long  oldlast;
   time_t tsince;
   partentry_t oldpart;

   sepinit(IS_ITEM);
   if (flags & O_LABEL)
      sepinit(IS_ALL);
   i_lastseen         = st_glob.i_current-1;
   memcpy(&oldpart, &part[i_lastseen], sizeof(oldpart));
   oldnr = st_glob.r_lastseen = abs(part[i_lastseen].nr);
   st_glob.r_current=0;
   oldlast = part[i_lastseen].last;

   /* Open file */
   sprintf(buff,"%s/_%d",conflist[confidx].location,st_glob.i_current);
   if (!(fp=mopen(buff,O_R))) return;
   if (!ngets(buff,fp) || strcmp(buff,"!<ps02>")) {
      wputs("Invalid _N file format\n");
      mclose(fp);
      return;
   }
   if (!ngets(buff,fp) || strncmp(buff,",H",2)) {
      wputs("Invalid _N file format\n");
      mclose(fp);
      return;
   }

   /* Get all the response offsets       */
   /* censor/scribble require this setup */
   get_item(fp,st_glob.i_current,re,sum); 
   /* moved up here from above response set loop */

   get_resp(fp,re,(short)GR_HEADER,(short)0); /* Get header of #0 */
   if (st_glob.opt_flags & (OF_NEWRESP|OF_NORESPONSE))
      st_glob.r_first = abs(part[i_lastseen].nr);
   else if (st_glob.since) {
      st_glob.r_first=0;
      while (st_glob.since > re[st_glob.r_first].date) {
         st_glob.r_first++;
         get_resp(fp,&(re[st_glob.r_first]),(short)GR_HEADER,st_glob.r_first);
         if (st_glob.r_first>=sum[i_lastseen].nr) break;
      }
   } 
/* else st_glob.r_first=0; LLL */

   /* Display item header */
   open_pipe();
   show_header();
 
   st_glob.r_last = MAX_RESPONSES;
   st_glob.r_max=sum[i_lastseen].nr-1;

   /* For each response set */
   if (!(st_glob.opt_flags & OF_NORESPONSE)
       && st_glob.r_first<=st_glob.r_max) {
      if (st_glob.r_first>0)
         show_nsep(fp); /* nsep between header and responses */
      show_range();
   } else
      st_glob.r_current = st_glob.r_first; /* for -# command */
   if (!(st_glob.opt_flags & OF_PASS))
      status &= ~S_INT;

   /* Save range info */
   topt_flags = st_glob.opt_flags;
   tsince     = st_glob.since;
   strcpy(buff, st_glob.string);
   ext_fp     = fp;
   ext_first  = st_glob.r_first;
   ext_last   = st_glob.r_last;

   /* RFP loop until pass or EOF (S_STOP) */
   while (!(status & S_STOP) && (st_glob.r_last >= 0))
   {

      /* Temporarily mark seen, for forget command */
      /* but DON'T mark this as dirty since it's temporary */
      part[i_lastseen].nr   = st_glob.r_lastseen;
      if ((sum[i_lastseen].flags & IF_FORGOTTEN) && part[i_lastseen].nr>0)
         part[i_lastseen].nr = -part[i_lastseen].nr; /* stay forgotten */
/* Don't do this, acc to Russ
      time(&(part[i_lastseen].last));
*/
   
      /* Main RFP cmd loop */
      mode = M_RFP;
      {
         short tmp_stat;

         tmp_stat = st_glob.c_status;
         if ((sum[i_lastseen].flags & IF_FROZEN) || (flags & O_READONLY))
            st_glob.c_status |= CS_NORESPONSE;
         /* while (mode==M_RFP && !(status & S_INT)) */
         /* test stop in case ^D hit */
         while (mode==M_RFP && !(status & S_STOP)) {
            if (st_glob.opt_flags & OF_PASS)
               command("pass",0);
            else if (!get_command("pass", 0)) { /* eof */
               /* status |= S_INT; WHY was this here? */
               if (!(status & S_STOP)) {
                  status |= S_STOP; /* instead */
                  if (!(flags & O_QUIET))
                     wputs("Stopping.\n");
               }

               mode = M_OK;
               st_glob.r_last = -1;

               if (st_glob.opt_flags & OF_REVERSE)
                  st_glob.i_current = st_glob.i_first;
               else
                 st_glob.i_current = st_glob.i_last;
            }
         }
         st_glob.c_status = tmp_stat;
      }
   }
/* KKK what if new items come in? */

   /* Save range info */
   st_glob.opt_flags = topt_flags;
   st_glob.since = tsince;
   strcpy(st_glob.string, buff);

   /* Save to lastseen */
   /* 4/18 added NORESP check so that timestamp is only updated
    * if they actually saw the responses.  Thus, "browse new" doesn't
    * make things un-new for set sensitive.
    */
   if (!(st_glob.opt_flags & OF_NORESPONSE) &&
       (st_glob.r_last > -2)) { /* unless preserved or forgotten */
      part[i_lastseen].nr   = st_glob.r_lastseen;
      if ((sum[i_lastseen].flags & IF_FORGOTTEN) && part[i_lastseen].nr>0)
         part[i_lastseen].nr = -part[i_lastseen].nr; /* stay forgotten */
      if (abs(part[i_lastseen].nr)==sum[i_lastseen].nr)
         time(&(part[i_lastseen].last));
      if (flags & O_AUTOSAVE) {
			char **config;

			if ((config = get_config(confidx)) != NULL)
			   write_part(config[CF_PARTFILE]);
      }
   } else if (st_glob.r_last == -2) { /* unless preserved or forgotten */
      part[i_lastseen].nr   = oldnr;
      part[i_lastseen].last = oldlast;
   }
   if (memcmp(&oldpart, &part[i_lastseen], sizeof(oldpart)))
      dirty_part(i_lastseen);

   /* Must be kept open past RFP mode so 'since' command can access it */
   mclose(fp);
}

/* 
 * Same as usual item read progression EXCEPT it ignores the numerical
 * count.  This is just so the WWW can select an item to view and then
 * select the next or previous one.
 */
int           /* Returns next item number, or -1 for none */
nextitem(inc)
   int inc;
{
   int i;

   for (i = st_glob.i_current+inc;
        i >= st_glob.i_first && i <= st_glob.i_last;
        i += inc) {
      if (cover(i, confidx, st_glob.opt_flags, A_COVER, sum, part, &st_glob))
         return i;
   }
   return -1;
}

/******************************************************************************/
/* READ A SET OF ITEMS IN THE CONFERENCE                                      */
/******************************************************************************/
int                /* RETURNS: (nothing)     */
do_read(argc,argv) /* ARGUMENTS:             */
int    argc;       /*    Number of arguments */
char **argv;       /*    Argument list       */
{
   short      i_s,i_i, shown=0, rfirst=0,br;
   char       act[MAX_ITEMS];
   char       buff[MAX_LINE_LENGTH];

   br = (match(argv[0],"b_rowse") || match(argv[0],"s_can"));
   rangeinit(&st_glob,act);
   refresh_sum(0,confidx,sum,part,&st_glob);
   st_glob.r_first = 0;
   st_glob.opt_flags = (br)? OF_SHORT|OF_NORESPONSE|OF_PASS : 0; 

   /* Check arguments */
   if (argc>1)
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);

   if (!(st_glob.opt_flags & OF_RANGE)) {
      rangetoken("all",&st_glob.opt_flags,act,sum,&st_glob);
      if (!(st_glob.opt_flags & (OF_UNSEEN|OF_FORGOTTEN|OF_RETIRED|OF_NEWRESP|OF_BRANDNEW))
       && !st_glob.since && !br) {
         rangetoken("new",&st_glob.opt_flags,act,sum,&st_glob);
      }
   }
   if (st_glob.opt_flags & OF_REVERSE) {
      i_s = st_glob.i_last;
      i_i = -1;
   } else {
      i_s = st_glob.i_first;
      i_i = 1;
   }
   rfirst = st_glob.r_first;

   if (match(argv[0],"pr_int"))
      confsep(expand("printmsg", DM_VAR),confidx,&st_glob,part,0);

   /* Process items */
   sepinit(IS_START); 
   
#if 0
   /* Set i_next to first covered item */
   st_glob.i_next = i_s;
   while (st_glob.i_next <= st_glob.i_last
        && st_glob.i_next >= st_glob.i_first
        && !cover(st_glob.i_next,confidx,st_glob.opt_flags,act[st_glob.i_next-1],sum, part,&st_glob))
      st_glob.i_next += i_i;

   for (st_glob.i_current = st_glob.i_next; 
        st_glob.i_current >= st_glob.i_first 
         && st_glob.i_current <= st_glob.i_last 
         && !(status & (S_INT|S_STOP)); 
        st_glob.i_current = st_glob.i_next) {

      /* Set i_next to next covered item */
      st_glob.i_next = st_glob.i_current + i_i;
      while (st_glob.i_next <= st_glob.i_last
           && st_glob.i_next >= st_glob.i_first
           && !cover(st_glob.i_next,confidx,st_glob.opt_flags,act[st_glob.i_next-1],sum, part,&st_glob))
         st_glob.i_next += i_i;
       
      st_glob.r_first = rfirst;
      read_item();
      shown++;
      if (match(argv[0],"pr_int"))
         wputs("\n");
   }
#else
   for (st_glob.i_current = i_s;
            st_glob.i_current >= st_glob.i_first
         && st_glob.i_current <= st_glob.i_last
         && !(status & S_INT);
        st_glob.i_current += i_i) {
      if (cover(st_glob.i_current, confidx,st_glob.opt_flags,
       act[st_glob.i_current-1],sum, part,&st_glob)) {
         st_glob.i_next = nextitem(1);
         st_glob.i_prev = nextitem(-1);
         st_glob.r_first = rfirst;
         read_item();
         shown++;
         if (match(argv[0],"pr_int"))
            wputs("^L\n");
      }
   }
#endif
   if (!shown && (st_glob.opt_flags & (OF_BRANDNEW|OF_NEWRESP))) {
      sprintf(buff, "No new %ss matched.\n", topic(0));
      wputs(buff);
   }
   
   /* Check for new mail only */
   refresh_stats(sum,part,&st_glob);
   check_mail(0);

   return 1;
}

/******************************************************************************/
/* FORGET A SET OF ITEMS IN THE CONFERENCE                                    */
/******************************************************************************/
int               /* RETURNS: (nothing)     */
forget(argc,argv) /* ARGUMENTS:             */
int    argc;      /*    Number of arguments */
char **argv;      /*    Argument list       */
{
   char       act[MAX_ITEMS];
   short      j;

   rangeinit(&st_glob,act);
   refresh_sum(0,confidx,sum,part,&st_glob);

   if (argc<2) {
      printf("Error, no %s specified! (try HELP RANGE)\n", topic(0));
   } else { /* Process args */
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   }

   /* Process items */
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      if (cover(j,confidx,st_glob.opt_flags,act[j-1],sum,part,&st_glob)) {
         if (!part[j-1].nr) {
            part[j-1].nr = 1; /* Pretend we've read the initial text */
            dirty_part(j-1);
         }
         if (!(sum[j-1].flags & IF_FORGOTTEN)) {
            if (!(flags & O_QUIET)) 
               printf("Forgetting %s %d\n",topic(0), j);
            st_glob.r_lastseen = part[j-1].nr    = -part[j-1].nr;
            sum[j-1].flags |= IF_FORGOTTEN;
            time(&( part[j-1].last )); /* current time */
            dirty_part(j-1);
            dirty_sum(j-1);
         }
      }
   }
   st_glob.r_last = -1; /* go on to next item if at RFP prompt */
   return 1;
}

/******************************************************************************/
/* LOCATE A WORD OR GROUP OF WORDS IN A CONFERENCE                            */
/******************************************************************************/
int                /* RETURNS: (nothing)     */
do_find(argc,argv) /* ARGUMENTS:             */
int    argc;       /*    Number of arguments */
char **argv;       /*    Argument list       */
{
   char       act[MAX_ITEMS],buff[MAX_LINE_LENGTH],pr,
              astr[MAX_LINE_LENGTH],
              str[MAX_LINE_LENGTH];
   short      j,icur;
   FILE      *fp;
   int        count=0;

   icur = st_glob.i_current;
   rangeinit(&st_glob,act);
   refresh_sum(0,confidx,sum,part,&st_glob);

   if (argc>1) /* Process argc */
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   if (!(st_glob.opt_flags & OF_RANGE))
      rangetoken("all",&st_glob.opt_flags,act,sum,&st_glob);

   if (!st_glob.string[0] && !st_glob.author[0]) {
      if (!(flags & O_QUIET)) 
         wputs("Find: look for what?\n");
      return 1;
   }
   strcpy(str, lower_case(st_glob.string)); /* so items match */
   strcpy(astr,lower_case(st_glob.author)); /* so items match */
   st_glob.string[0]='\0';
   st_glob.author[0]='\0';

   /* Process items */
   open_pipe();
   sepinit(IS_START);
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      st_glob.i_current=j;
      if (cover(j, confidx,st_glob.opt_flags, act[j-1], sum, part,
       &st_glob)) {
         sprintf(buff,"%s/_%d",conflist[confidx].location,j);
         if (!(fp=mopen(buff,O_R))) continue;
         get_item(fp,j,re,sum);
         sepinit(IS_ITEM);
         if (flags & O_LABEL)
            sepinit(IS_ALL);

         /* For each response */
         for (st_glob.r_current = 0;
              st_glob.r_current < sum[j-1].nr
               && !(status & S_INT);
              st_glob.r_current++) {
      
            get_resp(fp,&(re[st_glob.r_current]),(short)GR_HEADER,st_glob.r_current);
      
            /* Scan response text */
            if (re[st_glob.r_current].flags & (RF_EXPIRED|RF_SCRIBBLED)) 
               pr = 0;
            else if (re[st_glob.r_current].flags & RF_CENSORED) 
               pr = ((st_glob.opt_flags & OF_NOFORGET) || !(flags & O_FORGET));
            else 
               pr = 1;

            if (pr) {
               get_resp(fp,&(re[st_glob.r_current]),(short)GR_ALL,st_glob.r_current);
               sepinit(IS_RESP);

               if (!astr[0] 
                || !strcmp(re[st_glob.r_current].login,astr)) {

                  if (str[0]) {
                     for (st_glob.l_current=0; 
                      !(status & S_INT) 
                       && st_glob.l_current<xsizeof(re[st_glob.r_current].text);
                      st_glob.l_current++) {
                        if (strstr(lower_case(
                         re[st_glob.r_current].text[st_glob.l_current]), str)){
                           itemsep(expand("fsep", DM_VAR),0);
                           count++;
                        }
                     }
                  } else {
                     st_glob.l_current=0; 
                     itemsep(expand("fsep", DM_VAR),0);
                     count++;
                  }
               }
               xfree_array(re[st_glob.r_current].text);
            }
         }
         mclose(fp);
      }
   }

   /* Reload initial re[] contents */
   st_glob.i_current = icur;
   if (icur>=st_glob.i_first && icur<=st_glob.i_last) {
      sprintf(buff,"%s/_%d",conflist[confidx].location,icur);
      if ((fp=mopen(buff,O_R)) != NULL) {
         get_item(fp,j,re,sum);
         mclose(fp);
      }
   }
 
   if (!count)
      wputs("No matches found.\n");
   return 1;
}

/******************************************************************************/
/* FREEZE/THAW/RETIRE/UNRETIRE A SET OF ITEMS IN THE CONFERENCE               */
/******************************************************************************/
int               /* RETURNS: (nothing)     */
freeze(argc,argv) /* ARGUMENTS:             */
int    argc;      /*    Number of arguments */
char **argv;      /*    Argument list       */
{
   char       act[MAX_ITEMS],buff[MAX_LINE_LENGTH];
   short      j;
   struct stat stt;

   rangeinit(&st_glob,act);

   if (argc<2) {
      printf("Error, no %s specified! (try HELP RANGE)\n", topic(0));
   } else { /* Process args */
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   }

   /* Process items */
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      if (!act[j-1] || !sum[j-1].flags) continue;
      sprintf(buff,"%s/_%d",conflist[confidx].location,j);
      st_glob.i_current = j;

      /* Check for permission */
      if (!(st_glob.c_status & CS_FW) && !is_enterer(j)) {
         printf("You can't do that!\n");
         continue;
      }

      if (!match(get_conf_param("freezelinked",FREEZE_LINKED),"true")) {
         if ((sum[j-1].flags & IF_LINKED) && (re[0].uid!=uid || strcmp(re[0].login,login))) {
            printf("%s %d is linked!\n",topic(1), j);
            continue;
         }
      }

      /* Do the change */
      if (stat(buff,&stt)) {
         printf("Error accessing %s\n",buff);
         continue;
      }

      switch(tolower(argv[0][0])) {
      case 'f': /* Freeze   r--r--r-- */
                sum[j-1].flags |=  IF_FROZEN;
                if (chmod(buff,stt.st_mode & ~S_IWUSR)) 
                   error("freezing ",buff);
                else {
                   custom_log("freeze", M_RFP);
                   /* sprintf(buff,"froze %s %d", topic(0), j); */
                }
                break;
      case 't': /* Thaw     rw-r--r-- */
                sum[j-1].flags &= ~IF_FROZEN;
                if (chmod(buff,stt.st_mode |  S_IWUSR)) 
                   error("thawing ",buff);
                else {
                   custom_log("thaw", M_RFP);
                   /* sprintf(buff,"thawed %s %d", topic(0), j); */
                }
                break;
      case 'r': /* Retire   r-xr--r-- F,R */
                sum[j-1].flags |=  IF_RETIRED;
                if (chmod(buff,stt.st_mode |  S_IXUSR)) 
                   error("retiring ",buff);
                else {
                   custom_log("retire", M_RFP);
                   /* sprintf(buff,"retired %s %d", topic(0), j); */
                }
                break;
      case 'u': /* Unretire rw-r--r-- */
                sum[j-1].flags &= ~IF_RETIRED;
                if (chmod(buff,stt.st_mode & ~S_IXUSR)) 
                   error("unretiring ",buff);
                else {
                   custom_log("unretire", M_RFP);
                   /* sprintf(buff,"unretired %s %d", topic(0), j); */
                }
                break;
      }
      save_sum(sum,(short)(j-1),confidx,&st_glob);
      dirty_sum(j-1);
      /* log(confidx, buff); */
   }
   return 1;
}

/******************************************************************************/
/* LINK AN ITEM INTO THE CONFERENCE                                           */
/******************************************************************************/
int                 /* RETURNS: (nothing)     */
linkfrom(argc,argv) /* ARGUMENTS:             */
int    argc;        /*    Number of arguments */
char **argv;        /*    Argument list       */
{
   short       idx,i,j;
/*
   unsigned int sec;
*/
   char      **config;
   sumentry_t  fr_sum[MAX_ITEMS];
   char        act[MAX_ITEMS];
   char        fr_buff[MAX_LINE_LENGTH],
               to_buff[MAX_LINE_LENGTH],
               buff[MAX_LINE_LENGTH];
   status_t    fr_st;
   partentry_t part2[MAX_ITEMS];

   st_glob.opt_flags = 0;
   refresh_sum(0,confidx,sum,part,&st_glob);
   if (!(st_glob.c_status & CS_FW)) {
      printf("Sorry, you can't do that!\n");
      return 1;
   }

   if (argc<2) {
      printf("No %s specified!\n", conference(0));
      return 1;
   }

   idx = get_idx(argv[1],conflist,maxconf);
   if (idx<0) {
      printf("Cannot access %s %s.\n",conference(0),argv[1]);
      return 1;
   }

   /* Read in config file */
   if (!(config=get_config(idx)))
      return 1;

   /* Pass security checks (from join() in conf.c) */
#if 0
   sec = security_type(config, idx);
   if (!checkpoint(idx,sec,0)) { /* Can't linkfrom a READONLY cf */
      printf("Failed security check on type %d: link aborted.\n",sec);
      return 1;
   }
#endif
   if (!check_acl(JOIN_RIGHT, idx) || !check_acl(RESPOND_RIGHT, idx)) {
      printf("You are not allowed to link %ss from the %s %s.\n",
       compress(conflist[idx].name), topic(0), conference(0));
      return 1;
   }
 
   /* Get item range */
   if (argc<3) {
      printf("Error, no %s specified! (try HELP RANGE)\n", topic(0));
      return 1;
   }

#ifdef NEWS
   fr_st.c_article = 0;
#endif
   read_part(config[CF_PARTFILE],part2,&fr_st,idx); /* Read in partfile */
   fr_st.c_security = security_type(config, idx);
   rangeinit(&fr_st,act);

   /* get subjs for range */
   get_status(&fr_st,fr_sum,part2,idx);

   range(argc,argv,&st_glob.opt_flags,act,fr_sum,&fr_st,1);

   /* Process items */
   for (j=fr_st.i_first; j<=fr_st.i_last && !(status & S_INT); j++) {
      if (!cover(j,idx,st_glob.opt_flags,act[j-1],fr_sum,part2,&fr_st))
         continue;
      
      sprintf(fr_buff,"%s/_%d",conflist[idx].location,j);
      i = ++(st_glob.c_confitems); /* next item number */
      printf("Linking as %s %d...",topic(0), i);
      sprintf(to_buff,"%s/_%d",conflist[confidx].location,i);
      if (link(fr_buff,to_buff) && symlink(fr_buff,to_buff))
         error("linking from ",fr_buff);
      else {
         fr_sum[j-1].flags |= IF_LINKED;
         memcpy(&(sum[i-1]),&(fr_sum[j-1]),sizeof(sumentry_t));
         /* to_sum[i-1].flags |= IF_LINKED; */
         save_sum(fr_sum,(short)(j-1),idx,&fr_st);
         save_sum(sum,(short)(i-1),confidx,&st_glob);
         dirty_sum(i-1);

         /* Log action to both conferences */
         sprintf(buff, "linked %s %d to %s %d", topic(0), j,
          compress(conflist[confidx].name), i);
         log(idx, buff);
         sprintf(buff, "linked %s %d from %s %d", topic(0), i,
          compress(conflist[idx].name), j);
         log(confidx, buff);

         printf("done.\n");
      }
   }
   return 1;
}

/******************************************************************************/
/* KILL A SET OF ITEMS IN THE CONFERENCE                                      */
/******************************************************************************/
int                /* RETURNS: (nothing)     */
do_kill(argc,argv) /* ARGUMENTS:             */
int    argc;       /*    Number of arguments */
char **argv;       /*    Argument list       */
{
   char       act[MAX_ITEMS],buff[MAX_LINE_LENGTH],
              buff2[MAX_LINE_LENGTH];
   short      j;
   FILE      *fp;

   rangeinit(&st_glob,act);
   refresh_sum(0,confidx,sum,part,&st_glob);

   if (argc<2) {
      printf("Error, no %s specified! (try HELP RANGE)\n", topic(0));
   } else { /* Process args */
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   }

   /* Process items */
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      if (!act[j-1] || !sum[j-1].flags) continue;
      sprintf(buff,"%s/_%d",conflist[confidx].location,j);

      /* Check for permission */
      if (!(st_glob.c_status & CS_FW)) {
         if (!(fp=mopen(buff,O_R))) continue;
         get_item(fp,j,re,sum);
         mclose(fp);
         if (re[0].uid!=uid || strcmp(re[0].login, login) || sum[j-1].nr>1) {
            wputs("You can't do that!\n");
            continue;
         }
      }

      /* Do the remove */
      sprintf(buff2,"Ok to kill %s %d? ",topic(0), j);
      if (get_yes(buff2, DEFAULT_OFF)) {
         printf("(Killing %s %d)\n",topic(0), j);
         rm(buff,SL_OWNER);
         sum[j-1].flags=0;
         save_sum(sum,(short)(j-1),confidx,&st_glob);
         dirty_sum(j-1);
#if 0
         sprintf(buff2, "killed %s %d", topic(0), j);
         log(confidx, buff2);
#endif
         custom_log("kill", M_RFP);
      }
   }
   st_glob.r_last = -1; /* go on to next item if at RFP prompt */
   return 1;
}

/******************************************************************************/
/* REMEMBER (UNFORGET) A SET OF ITEMS IN THE CONFERENCE                       */
/******************************************************************************/
int                 /* RETURNS: (nothing)     */
remember(argc,argv) /* ARGUMENTS:             */
   int    argc;     /*    Number of arguments */
   char **argv;     /*    Argument list       */
{
   char       act[MAX_ITEMS];
   short      j;

   /* Initialize range */
   rangeinit(&st_glob,act);

   /* Process arguments */
   if (argc<2) rangetoken("all",&st_glob.opt_flags,act,sum,&st_glob);
   else        range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);

   /* Process items in specified range */
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      if (!act[j-1] || !sum[j-1].flags) continue;
      part[j-1].nr   = abs(part[j-1].nr);
      sum[j-1].flags &= ~IF_FORGOTTEN;
      dirty_part(j-1);
      dirty_sum(j-1);
   }
   return 1;
}

/******************************************************************************/
/* MARK A SET OF ITEMS AS BEING SEEN                                          */
/******************************************************************************/
int                /* RETURNS: (nothing)     */
fixseen(argc,argv) /* ARGUMENTS:             */
   int    argc;    /*    Number of arguments */
   char **argv;    /*    Argument list       */
{
   char       act[MAX_ITEMS];
   short      j;

   rangeinit(&st_glob,act);
   refresh_sum(0,confidx,sum,part,&st_glob);

   if (argc<2) {
      rangetoken("all",&st_glob.opt_flags,act,sum,&st_glob);
   } else { /* Process args */
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   }

   /* Process items */
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      if (cover(j,confidx,st_glob.opt_flags,act[j-1],sum,part,&st_glob)) {
         part[j-1].nr   = sum[j-1].nr;
         time(&( part[j-1].last ));     /* or sum.last? */
         dirty_part(j-1);
      }
   }
   return 1;
}

/******************************************************************************/
/* MARK A SET OF RESPONSES AS BEING SEEN                                      */
/******************************************************************************/
int              /* OUT: 1 (don't exit)     */
fixto(argc,argv)
   int    argc;  /* IN: Number of arguments */
   char **argv;  /* IN: Argument list       */
{
   char       act[MAX_ITEMS];
   short      j=0, r;
   time_t     stamp;
   char       buff[MAX_LINE_LENGTH];
   FILE      *fp;

   stamp = since(argc, argv, &j);
   argc -= j;
   argv += j;

   rangeinit(&st_glob,act);
   refresh_sum(0,confidx,sum,part,&st_glob);

   if (argc<2) {
      rangetoken("all",&st_glob.opt_flags,act,sum,&st_glob);
   } else { /* Process args */
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   }

   /* Process items */
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      if (!cover(j,confidx,st_glob.opt_flags,act[j-1],sum,part,&st_glob)
       || !sum[j-1].nr || part[j-1].nr < 0)
         continue;

      /* Load response times */
      sprintf(buff,"%s/_%d",conflist[confidx].location, j);
      if ((fp=mopen(buff,O_R)) != NULL) {
         get_item(fp, j, re, sum);

         /* Find first response # which is dated > timestamp */
         for (r=0; r<sum[j-1].nr; r++) {
            get_resp(fp, &re[r], (short)GR_HEADER, r);
            if (re[r].date > stamp)
               break;
         }

         /* Store new info */
         part[j-1].nr   = r;
         part[j-1].last = stamp;
         dirty_part(j-1);

         mclose(fp);
      }
   }
   return 1;
}
