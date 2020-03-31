/* $Id: sum.c,v 1.9 1997/08/28 00:07:50 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <time.h>
#include <sys/types.h> /* for sys/stat.h under ultrix */
#include <sys/stat.h>
#include <string.h>
#include <memory.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>   /* for getenv() */
#endif
#include <errno.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "lib.h"
#include "sum.h"
#include "item.h"
#include "xalloc.h"
#include "files.h"
#include "macro.h"
#include "sep.h"
#include "news.h"
#include "stats.h"
#include "license.h" /* for get_conf_param() */
#include "range.h"   /* for is_brandnew(), is_newresp() */

#define ST_LONG 0x01
#define ST_SWAP 0x02

#define SHORT_MAGIC 0x00001537L
#define SHORT_BACK  0x37150000L
#define LONG_MAGIC  0x12345678L
#define LONG_BACK   0x78563412L
#define OLD_YAPP    0x58585858L

typedef struct {
   int32   flags,nr;
   time_t last,first;
} longsumentry_t;

static part_is_dirty=0;
static sum_is_dirty=0;

/* Compute partfilename hash to appear in sum file */
int32         /* OUT: hash value */
get_hash(str)
   char *str; /* IN : participation filename */
{
   int32 ret=0;
   char *p;

   for (p=str; *p; p++)
      ret = (ret*4) ^ (*p);
   return ret;
}

int32
get_sumtype(buff)
   char *buff;
{
   int32       temp, 
               sumtype;
   short       swap=atoi(get_conf_param("byteswap",BYTESWAP)); /* constant used for byte swap check */

   /* Determine byte order & file type */
   memcpy((char *)&temp, buff, sizeof(int32));
   switch(temp) {
   case SHORT_MAGIC : sumtype = ST_LONG;            break;
   case SHORT_BACK  : sumtype = ST_LONG  | ST_SWAP; break;
   case LONG_MAGIC  : sumtype = 0;                  break;
   case LONG_BACK   : sumtype = ST_SWAP;            break;
   case OLD_YAPP    : swap=(*((char*)&swap)); 
                      sumtype = swap * ST_SWAP;     break;
   default: printf("invalid sum type = %08X\n", temp);
            swap=(*((char*)&swap)); /* check which byte contains the 1 */
            sumtype = swap * ST_SWAP;               break;
   }

#ifdef SDEBUG
   printf("sum type = %08X (%d)\n", temp, sumtype);
   printf("%s %s\n", (sumtype & ST_LONG)? "long":"short", 
    (sumtype & ST_SWAP)? "swap":"normal");
#endif
   return sumtype;
}

/******************************************************************************/
/* SWAP BYTES IF LOW BIT IN HIGH BYTE (INTEL)                                 */
/* This is so machines with Intel processors and those with Motorola          */
/* processors can both use the same data files on the same filesystem         */
/******************************************************************************/
static void           /* RETURNS: (nothing)        */
byteswap(word,length) /* ARGUMENTS:                */
   char *word;        /*    Byte string to reverse */
   int length;        /*    Number of bytes        */
{
   register int i;

   if (length>1)
      for (i = 0; i <= (length - 2) / 2; i++)
         word[i] ^= (word[length - 1 - i] ^= (word[i] ^= word[length - 1 - i]));
}

#ifdef NEWS
void
save_article(art, idx)
   long art;
   short idx;
{
   FILE       *fp;
   char        path[MAX_LINE_LENGTH];
   struct stat st;
   long        mod;

   sprintf(path,"%s/article",conflist[idx].location);

   /* Create if doesn't exist, else update */
   if (stat(path,&st)) mod = O_W;
   else                mod = O_RPLUS;

   /* if (stt->c_security & CT_BASIC) mod |= O_PRIVATE;*/
   if ((fp=mopen(path, mod))==NULL) return;
   fprintf(fp,"%ld\n",art);
   mclose(fp);
}

void
load_article(art,idx)
   long *art;
   short idx;
{
   FILE       *fp;
   char        path[MAX_LINE_LENGTH];

   sprintf(path,"%s/article",conflist[idx].location);

   if ((fp=mopen(path, O_R))==NULL) {
      *art = 0;
      return;
   }
   fscanf(fp,"%d\n",art);
   mclose(fp);
}
#endif
 
/******************************************************************************/
/* SAVE SUM FILE FOR CONFERENCE                                               */
/******************************************************************************/
void                           
save_sum(newsum,where,idx,stt) 
   sumentry_t *newsum;         /* IN/OUT: Modified record              */
   short       where;          /* IN:     Index of modified record     */
   short       idx;            /* IN:     Conference to write file for */
   status_t   *stt;            /*      */
{
   FILE       *fp;
   short       i=1;
   char        path[MAX_LINE_LENGTH], buff[17], 
             **config;
   sumentry_t  entry;
   longsumentry_t  longentry;
   short       swap=atoi(get_conf_param("byteswap",BYTESWAP)); /* constant used for byte swap check */
   long        mod;
   struct stat st;
   int32        temp,
               sumtype;

   if (debug & DB_SUM)
      printf("SAVE_SUM %x %d\n", newsum, where); 

   swap=(*((char*)&swap)); /* check which byte contains the 1 */

   sprintf(path,"%s/sum",conflist[idx].location); 

   /* Create if doesn't exist, else update */
   if (stat(path,&st)) mod = O_W;
   else                mod = O_RPLUS;

   if (stt->c_security & CT_BASIC) mod |= O_PRIVATE;
   if ((fp=mopen(path,mod))==NULL) return;

   /* Determine file type */
   if (mod==O_W || (i=fread(buff,16,1,fp)) <16) /* new file */
      sumtype = ST_LONG;
   else
      sumtype = get_sumtype(buff+12);
   rewind(fp);

   if (!(config=get_config(idx)))
      return;

   /* Write header */
   if (sumtype & ST_LONG) {
      fwrite("!<sm02>\n",8,1,fp);
      temp = get_hash( config[CF_PARTFILE] );
      fwrite((char *)&temp,sizeof(int32),1,fp);
      temp = SHORT_MAGIC;
      fwrite((char *)&temp,sizeof(int32),1,fp);
      temp = LONG_MAGIC;
      fwrite((char *)&temp,sizeof(int32),1,fp);
   } else {
      short tshort;

      fwrite("!<pr03>\n",8,1,fp);
      tshort = get_hash( config[CF_PARTFILE] );
      fwrite((char *)&tshort,sizeof(short),1,fp);
      temp = SHORT_MAGIC;
      fwrite((char *)&tshort,sizeof(short),1,fp);
      temp = LONG_MAGIC;
      fwrite((char *)&temp,sizeof(int32),1,fp);
   }

   if (debug & DB_SUM)
      printf("Saving %d %ss\n", stt->c_confitems, topic(0));
   if (where+1 > stt->c_confitems)
      stt->c_confitems = where+1;
   for (i=1; i<=stt->c_confitems; i++) {
      short t;

   /* if (newsum[i-1].nr) printf("%d: %d\n",i,newsum[i-1].nr); */
      t=newsum[i-1].flags;
      newsum[i-1].flags &= IF_SAVEMASK;

      if (sumtype & ST_LONG) {
         longentry.nr    = newsum[i-1].nr;
         longentry.flags = newsum[i-1].flags;
         longentry.last  = newsum[i-1].last ;
         longentry.first = newsum[i-1].first;
         if (sumtype & ST_SWAP) {
            byteswap((char*)&(longentry.nr   ),sizeof(int32  ));
            byteswap((char*)&(longentry.flags),sizeof(int32  ));
            byteswap((char*)&(longentry.first),sizeof(time_t));
            byteswap((char*)&(longentry.last ),sizeof(time_t));
         }
         fwrite((char *)&longentry,sizeof(longsumentry_t),1,fp);
      } else {
         memcpy((char *)&entry,(char *)&(newsum[i-1]),sizeof(sumentry_t));
         if (sumtype & ST_SWAP) {
            byteswap((char*)&(entry.nr   ),sizeof(short ));
            byteswap((char*)&(entry.flags),sizeof(short ));
            byteswap((char*)&(entry.first),sizeof(time_t));
            byteswap((char*)&(entry.last ),sizeof(time_t));
         }
         fwrite((char *)&entry,sizeof(sumentry_t),1,fp);
      }

      newsum[i-1].flags=t;
   }

   mclose(fp);
}

void
refresh_sum(item,idx,sum,part,stt)
   short        item;              /* IN:     Item index               */
   short        idx;               /* IN:     Conference index         */
   sumentry_t  *sum;               /* IN/OUT: Item summary             */
   partentry_t *part;              /*         User participation info  */
   status_t    *stt;               /* IN/OUT: Status structure         */
{
   struct stat st;
   char   path[MAX_LINE_LENGTH];
   short  i,last,first;

   if (idx<0) 
      return;

#ifdef NEWS
   char **config;

   if (!(config = get_config(idx)))
      return;
   if (stt->c_security & CT_NEWS) {
      sprintf(path,"%s/%s",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]));
      if (stat(path,&st)) {
         stt->sumtime=0;
         st.st_mtime    =1;
      }
      if (st.st_mtime!=stt->sumtime) {
         load_sum(sum,part,stt,idx);
         stt->sumtime = st.st_mtime; 
      }
      refresh_stats(sum,part,stt); /* update stt */
      return;
   }
#endif

   /* Is global information current? */
   sprintf(path,"%s/sum",conflist[idx].location);
   if (stat(path,&st)) {
      stt->sumtime=0;
      st.st_mtime    =1;
   }
   if (st.st_mtime!=stt->sumtime) {

      /* Load global information */
      load_sum(sum,part,stt,idx);
      stt->sumtime = st.st_mtime;
   }

   /* Are links current? */
   last  = (item)? item : MAX_ITEMS;
   first = (item)? item : 1;
   for (i=first-1; i<last; i++)
      refresh_link(stt,sum,part,idx,i);

   /* Need to refresh stats anyway, in case part[] changed */
   if (sum_is_dirty || part_is_dirty)
      refresh_stats(sum,part,stt); /* update stt */
}

int                          /* OUT: 1 on valid, 0 else           */
item_sum(i,sum,part,idx,stt) 
   short        i;           /* IN: Item number                   */
   sumentry_t  *sum;         /*     Item summary array to fill in */
   partentry_t *part;        /*     Participation info            */
   short        idx;         /* IN: Conference index              */
   status_t    *stt;
{
   FILE       *fp;
   char        path[MAX_LINE_LENGTH];
   struct stat st;
   char        buff[MAX_LINE_LENGTH];
   char      **config;

   sum[i].flags=sum[i].nr=0;
   dirty_sum(i);

   if (!(config = get_config(idx)))
      return 0;

   sprintf(path,"%s/_%d",conflist[idx].location,i+1);
   if (stat(path,&st)) 
      return 0;
   else
      sum[i].flags |= IF_ACTIVE;

   if (st.st_nlink > 1) 
      sum[i].flags |= IF_LINKED;
   if (!(st.st_mode & S_IWUSR))
      sum[i].flags |= IF_FROZEN;
   if (part[i].nr < 0)
      sum[i].flags |= IF_FORGOTTEN;
   sum[i].last = st.st_mtime;
   if (!(st.st_mode & S_IRUSR) || !(fp = mopen(path,O_R))) {
      sum[i].flags |= IF_RETIRED;
      sum[i].nr     = 0;
      sum[i].first  = 0;
   } else {
      if (st.st_mode & S_IXUSR)
         sum[i].flags |= IF_RETIRED;
      sum[i].nr     = 0; /* count them */

      ngets(buff,fp); /* magic - ignore */
      ngets(buff,fp); /* H - ignore if FAST */
      store_subj(idx, i, buff+2);
      ngets(buff,fp); /* R - ignore */
      ngets(buff,fp); /* U - ignore */
      store_auth(idx, i, strchr(buff+2,',')+1);
      ngets(buff,fp); /* A - ignore */
      ngets(buff,fp); /* date */
      sscanf(buff+2,"%x",&(sum[i].first));
      while (ngets(buff,fp)) {
         if (!strcmp(buff,",T")) sum[i].nr++; 

#ifdef NEWS
         if (!strncmp(buff,",N",2)) {
            art = atoi(buff+2);

            /* Check to see if it has expired */
            sprintf(buff,"%s/%s/%d",get_conf_param("newsdir",NEWSDIR),dot2slash(config[CF_NEWSGROUP]),
             art);
printf("Checking %s ",buff);
            if ((nfp=mopen(buff,O_R|O_SILENT))==NULL) {
               sum[i].flags |= IF_EXPIRED;
printf("EXPIRED\n");
            } else {
               mclose(nfp);
printf("OK\n");
            }

            if (art > stt->c_article)
               stt->c_article = art;
            /* printf("Found article %d\n",art); */
         }
#endif
      }
      mclose(fp);
   }
   return 1;
}

/******************************************************************************/
/* Load SUM data for arbitrary conference (requires part be done previously)  */
/******************************************************************************/
void                       /* RETURNS: (nothing) */
load_sum(sum,part,stt,idx) /* ARGUMENTS: */
   sumentry_t  *sum;       /*     Item summary array to fill in */
   partentry_t *part;      /*     Participation info */
   status_t    *stt;       /*      */
   short        idx;       /* IN: Conference index */
{
   FILE       *fp;
   short       i=1,j;
   char        path[MAX_LINE_LENGTH];
   char      **config;
   struct stat st;
   char        buff[MAX_LINE_LENGTH];
   short       confitems=0;
   short       swap=atoi(get_conf_param("byteswap",BYTESWAP)); /* constant used for byte swap check */
   int32        sumtype=0;
   int32        temp;
   short       tshort;

   for (j=0; j<MAX_ITEMS; j++) {
      sum[j].nr = sum[j].flags = 0;
      dirty_sum(j);
   }
   
   swap=(*((char*)&swap));
   sprintf(path,"%s/sum",conflist[idx].location);

   /* For NFS mounted cf, open is 27 secs with lock(failing) and 4
    * without the lock.  Why should we lock it anyway?
    */

   /* If SUM doesn't exist */
   if ((fp=mopen(path,O_R|O_SILENT))==NULL) {
   /* if ((fp=mopen(path,O_RPLUS|O_LOCK|O_SILENT))==NULL) */
      DIR *fp;
      struct dirent *dp;
      char fmt[6];

      strcpy(path,conflist[idx].location);
      strcpy(fmt,"_%d");

      if ((fp = opendir(path))==NULL) {
         error("opening ",path);
         return;
      }
     
      /* Load in stats 1 piece at a time - the slow stuff */
      for (dp = readdir(fp); dp != NULL; dp = readdir(fp)) {
         long i2;
         if (sscanf(dp->d_name,fmt,&i2)==1) {
            i = i2-1;
            if (item_sum(i,sum,part,idx,stt)) {
               confitems++;
               if (i2>stt->c_confitems)
                  stt->c_confitems=i2;
            }
         }
      }
      closedir(fp);

      /* Load in stats 1 piece at a time - the slow stuff 
      for (i=0; i<MAX_ITEMS; i++) {
         confitems += item_sum(i,sum,part,idx,stt);
      }
      */

#ifdef NEWS
      /* Update ITEM files with new articles */
      /* printf("Article=%d\n",stt->c_article); */
      if (stt->c_security & CT_NEWS)
         refresh_news(sum,part,stt,idx);
#endif

      return;
   }

   /* Read in SUM file - the fast stuff */
   if (!stat(path,&st)) 
      stt->sumtime = st.st_mtime;

   if (!(i=fread(buff,16,1,fp)) 
    || (strncmp(buff,"!<sm02>\n",8) && strncmp(buff,"!<pr03>\n",8))) {
      mclose(fp);
      errno=0;
      error(path," failed magic check");
      /* printf("WARNING: %s failed magic check\n",path); */

      /* Load in stats 1 piece at a time - the slow stuff */
      for (i=0; i<MAX_ITEMS; i++) {
         if (item_sum(i,sum,part,idx,stt)) {
            confitems++;
            if (i+1>stt->c_confitems)
               stt->c_confitems=i+1;
         }
      }

      refresh_stats(sum,part,stt);
      save_sum(sum,(short)-1,idx,stt);
      return;
   }

/*
   fread((char *)&confitems, sizeof(confitems),1,fp); * skip first 16 bytes *
   fread((char *)sum,sizeof(sumentry_t),1,fp); *fseek fails for some reason *
*/

   /* Determine byte order & file type */
   sumtype = get_sumtype(buff+12);
   if (!(config = get_config(idx)))
      return;

   if (sumtype & ST_LONG) {
      fread((char *)&temp, sizeof(int32), 1, fp); /* skip 4 more bytes */
      memcpy((char *)&temp, buff+8, sizeof(int32));
      if (sumtype & ST_SWAP)
         byteswap((char*)&temp,sizeof(int32  ));
      if (temp != get_hash( config[CF_PARTFILE] )) {
         errno = 0;
         error("bad participation filename hash for ", config[CF_PARTFILE]);
      }
   } else {
      memcpy((char *)&tshort, buff+8, sizeof(short));
      if (sumtype & ST_SWAP)
         byteswap((char*)&temp,sizeof(short));
      if (tshort != (short)get_hash( config[CF_PARTFILE] )) {
         errno = 0;
         error("bad participation filename hash for ", config[CF_PARTFILE]);
      }
   }

#ifdef NEWS
   if (stt->c_security & CT_NEWS)
      load_article(&stt->c_article, idx);
#endif

   if (sumtype & ST_LONG) {
      longsumentry_t longsum[MAX_ITEMS];
      int i;

      confitems=fread((char *)longsum, sizeof(longsumentry_t), MAX_ITEMS, fp);

      for (i=0; i<confitems; i++) {
         if (!longsum[i].nr) 
            continue; /* skip if deleted */

         if (sumtype & ST_SWAP) {
            byteswap((char*)&(longsum[i].nr   ),sizeof(int32  ));
            byteswap((char*)&(longsum[i].flags),sizeof(int32  ));
            byteswap((char*)&(longsum[i].first),sizeof(time_t));
            byteswap((char*)&(longsum[i].last ),sizeof(time_t));
         }

         sum[i].nr    = longsum[i].nr;
         sum[i].flags = longsum[i].flags;
         sum[i].first = longsum[i].first;
         sum[i].last  = longsum[i].last;
      }

   } else {

      confitems=fread((char *)sum, sizeof(sumentry_t), MAX_ITEMS, fp);

      for (i=0; i<confitems; i++) {
         if (!sum[i].nr) 
            continue; /* skip if deleted */

         /* Check for byte swapping and such */
         if (sumtype & ST_SWAP) {
            byteswap((char*)&(sum[i].nr   ),sizeof(short ));
            byteswap((char*)&(sum[i].flags),sizeof(short ));
            byteswap((char*)&(sum[i].first),sizeof(time_t));
            byteswap((char*)&(sum[i].last ),sizeof(time_t));
         }
      }
   }

   mclose(fp);
   if (debug & DB_SUM)
      printf("confitems=%d\n",confitems);
   stt->c_confitems = confitems;

   for (i=0; i<confitems; i++) {
      if (!sum[i].nr) 
         continue; /* skip if deleted */

      if (sum[i].nr < 0 || sum[i].nr > MAX_RESPONSES) {
         printf("Invalid format of sum file\n");
         break;
      }

      if (part[i].nr < 0)
         sum[i].flags |= IF_FORGOTTEN;

      /* verify it's still linked, didnt used to check sumtime */
      refresh_link(stt,sum,part,idx,i);
   }

   for (; i<MAX_ITEMS; i++) {
      sum[i].flags=sum[i].nr=0;
   }

#ifdef NEWS
   /* Update ITEM files with new articles */
   if (stt->c_security & CT_NEWS)
      refresh_news(sum,part,stt,idx);
#endif
}

void
dirty_part(i)
   int i;
{
   part_is_dirty=1;
}

void
dirty_sum(i)
   int i;
{
   sum_is_dirty=1;
}

void
refresh_stats(sum,part,st)
   sumentry_t  *sum;       /* IN:     Sum array to fill in (optional)    */
   partentry_t *part;      /* IN:     Previously read participation data */
   status_t    *st;        /* IN/OUT: pointer to status structure        */
{
   register int i, last, first, n;

   st->i_brandnew = st->i_newresp = st->i_unseen = 0;
   st->r_totalnewresp = 0;
   st->i_last  = 0;

   /* Find first valid item */
   i=0;
   while (i<MAX_ITEMS
    && (!sum[i].nr || !sum[i].flags || (sum[i].flags & IF_EXPIRED)))
      i++;
   first = i+1;

   /* Find last valid item */
   i=MAX_ITEMS-1;
   while (i>=first
    && (!sum[i].nr || !sum[i].flags || (sum[i].flags & IF_EXPIRED)))
      i--;
   last = i+1;

   for (n=0,i=first-1; i<last; i++) {
      if (sum[i].nr) {
         if (!sum[i].flags) continue;
         if (!(sum[i].flags & IF_EXPIRED))
            n++;
         if ((sum[i].flags & (IF_RETIRED|IF_FORGOTTEN|IF_EXPIRED))
          && (flags & O_FORGET))
            continue;
         if (!part[i].nr && sum[i].nr) {
            st->i_unseen++; /* unseen */
            if (is_brandnew(&part[i], &sum[i])) 
               st->i_brandnew++;
         } else if (is_newresp(&part[i], &sum[i])) 
            st->i_newresp++;
         st->r_totalnewresp += sum[i].nr - abs(part[i].nr);
      }
   }

   if (!n) {
      first = 1;
      last  = 0;
   }

   st->i_first = first;
   st->i_last  = last;
   st->i_numitems = n;

   part_is_dirty = 0;
   sum_is_dirty = 0;
}

void
refresh_link(stt,sum,part,idx,i)
status_t    *stt;                /*     pointer to status structure */
sumentry_t  *sum;                /* IN/OUT: Sum array to fill in (optional) */
partentry_t *part;               /*     Previously read participation data  */
short        idx;                /* IN: Index of conference to process     */
short        i;                  /* IN: Item index                         */
{
   char path[MAX_LINE_LENGTH];
   struct stat st;

   if (sum[i].flags & IF_LINKED) { /* verify it's still linked */
      sprintf(path,"%s/_%d",conflist[idx].location,i+1);
      if (stat(path,&st) || st.st_nlink < 2) {
         sum[i].flags &= ~IF_LINKED;
         dirty_sum(i);
      }
      if (st.st_mtime > stt->sumtime) { /* new activity */
         item_sum(i,sum,part,idx,stt);
      }
   }
}

/******************************************************************************/
/* UPDATE THE GLOBAL STATUS STRUCTURE, OPTIONALLY GET ITEM SUBJECTS           */
/******************************************************************************/
/* note: does load_sum and not free_sum if argument is there */
void                      /* RETURNS: (nothing) */
get_status(st,s,part,idx) /* ARGUMENTS:                            */
   status_t    *st;       /*     pointer to status structure */
   sumentry_t  *s;        /*     Sum array to fill in (optional)    */
   partentry_t *part;     /*     Previously read participation data */
   short        idx;      /* IN: Index of conference to process     */
{
   sumentry_t  s1[MAX_ITEMS];
   sumentry_t *sum;
   short i;

   sum  = (s)? s : s1;
   
   if (st != (&st_glob))
      st->sumtime=0; 
   refresh_sum(0,idx,sum,part,st);

   /* Are links current? */
   for (i=0; i<MAX_ITEMS; i++)
      refresh_link(st,sum,part,idx,i);

   refresh_stats(sum,part,st);
}

void
check_mail(f) 
   int f;
{
   static int prev=0;
   int f2=f;
   char mbox[MAX_LINE_LENGTH],*mail;
   struct stat st;

   /* Mail is currently only checked in conf.c, when should new mail
    * be reported?  At Ok:? or only when join or display new? 
    * If conf.c is the only place, perhaps it should be moved there.
    * Note: the above was fixed when seps were added
    */
   mail = expand("mailbox",DM_VAR);
   if (!mail) mail = getenv("MAIL");
   if (!mail) {
      sprintf(mbox,"%s/%s", get_conf_param("maildir", MAILDIR), login);
      mail = mbox;
   }
   if (!stat(mail,&st) && st.st_size>0) {
      status |= S_MAIL;
      if (st_glob.mailsize && st.st_size > st_glob.mailsize)
         status |=  S_MOREMAIL;
      st_glob.mailsize = st.st_size;
   } else {
      status &= ~S_MAIL;
      st_glob.mailsize = 0;
   }

   if (status & S_MAIL) {
      if (!prev) f2=1;
      if (status & S_MOREMAIL) { sepinit(IS_START|IS_ITEM); f2=1; }
      if (f2)
         confsep(expand("mailmsg", DM_VAR),confidx,&st_glob,part,0);
      status &= ~S_MOREMAIL;
   }
   prev = (status & S_MAIL);
}
