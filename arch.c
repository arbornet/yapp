/* $Id: arch.c,v 1.7 1997/08/28 00:04:10 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h> /* for atoi */
#include <string.h>
#include "yapp.h"
#include "struct.h"
#include "item.h"
#include "lib.h"
#include "globals.h"
#include "arch.h"
#include "xalloc.h"
#include "news.h"
#include "sum.h"

#ifdef NEWS
void
check_news(re)
response_t *re;
{
   /* News article? */
   if (xsizeof(re->text) && !strncmp(re->text[0],",N",2)) {

      /* Get article # */
      if (sscanf(re->text[0]+2,"%d",&(re->article))>0 
       && re->article) {
	  xfree_array(re->text);
	  get_article(re);
      }

      /* Get message id# */
      if (re->text && xsizeof(re->text)>1 && !strncmp(re->text[1],",M",2)) {
         if (re->mid) xfree_string(re->mid);
    	 re->mid=xstrdup(re->text[1]+2);
      }
   }
}
#endif
 
/******************************************************************************/
/* READ IN A SINGLE RESPONSE                                                  */
/* Starting at current file position, read in a response.  The ending file    */
/* position will be the start of the next response.  Also, allocates space    */
/* for text which needs to be freed.                                          */
/* Assumes (sum) that this is always done within the current conference       */
/******************************************************************************/
void                         /* RETURNS: (nothing)             */
get_resp(fp,re,fast,num) /* ARGUMENTS                      */
FILE       *fp;              /*    Current file position       */
response_t *re;              /*    Response to fill in         */
short       fast;            /*    Don't save the actual text? */
short       num;
{
   char       buff[MAX_LINE_LENGTH];
   char     **who;
   char       done=0;
   int        i;

   /* Get response */
   if (re->offset>=0 && re->numchars>0 && fast==GR_ALL) {
      if (fseek(fp,re->textoff,0)) {
         sprintf(buff,"%d",re->textoff);
         error("fseeking to ",buff);
      }
      re->text = grab_more(fp,",E",0,NULL);
      if (!(flags & O_SIGNATURE) && (st_glob.c_security & CT_EMAIL)) {
         for (i=0; i<xsizeof(re->text) && strcmp(re->text[i],"--"); i++);
         if (i<xsizeof(re->text))
            re->text = xrealloc_array(re->text, i);
      }

#ifdef NEWS
      check_news(re);
#endif
      return;
   } 
   if (re->offset>=0 && fseek(fp,re->offset,0)) {
      sprintf(buff,"%d",re->textoff);
      error("fseeking to ",buff);
   }
   if (re->offset<0) { /* Find start of response */
      short i,j;
      
      for (i=1; i<=num && re[-i].endoff<0; i++); /* find prev offset */
      for (j=i-1; j>0; j--) {
         get_resp(fp,&(re[-j]),GR_OFFSET,num-j);
      }
      if (num && fseek(fp,re[-1].endoff,0)) {
         sprintf(buff,"%d",re[-1].endoff);
         error("fseeking to ",buff);
      }
   }
   if (fast==GR_OFFSET) {
         re->offset  = ftell(fp); 
         while (ngets(buff,fp) && buff[1]!='T');
         re->textoff = ftell(fp); 
         while (ngets(buff,fp) && strncmp(buff,",E",2) && strncmp(buff,",R",2));
         if (!strncmp(buff,",R",2)) re->endoff = ftell(fp)-strlen(buff)-1;
         else re->endoff  = ftell(fp); 
         re->numchars = -1;
   } else {
 
#ifdef NEWS
      if (re->mid) 
			xfree_string(re->mid);
      re->mid = NULL;
		re->article = 0;
#endif
      re->parent = 0;

/*printf("Lines: ");*/
      while (!done && !(status & S_INT)) {
         if (!ngets(buff,fp)) break; /* UNK error */
/*putchar(buff[1]);*/
         switch(buff[1]) {
         case 'A': if (re->fullname) xfree_string(re->fullname);
                   re->fullname=xstrdup(buff+2); 
                   break;
#if (SIZEOF_LONG == 8)
         case 'D': sscanf(buff+2, "%x", &(re->date)); break;
#else
         case 'D': sscanf(buff+2, "%lx",&(re->date)); break;
#endif
         case 'E': done=1; re->endoff = ftell(fp); break;
      /* case 'H': strcpy(subj,buff+2); break; */
         case 'R': re->offset = ftell(fp)-strlen(buff)-1;
                   sscanf(buff+2,"%hx",&(re->flags)); break;
/*
         case 'M': if (re->mid) xfree_string(re->mid);
		   re->mid=xstrdup(buff+2);
		   break;
*/
         case 'P': sscanf(buff+2,"%hd",&(re->parent)); 
                   re->parent++;
                   break;
         case 'T': re->textoff = ftell(fp); 
                   re->numchars= 0;
                   if (fast==GR_ALL) {
                      int endlen;
                      re->text = grab_more(fp,",E",0, &endlen);
                      re->numchars= ftell(fp)-re->textoff-(endlen+1); /*-",E"*/
                   } else {
                      while (ngets(buff,fp) && strncmp(buff,",E",2) && strncmp(buff,",R",2));
                      re->text = NULL;                 /*-",E..." */
                      re->numchars= ftell(fp)-re->textoff-(strlen(buff)+1); 
                   } 
#ifdef NEWS
                   check_news(re);
#endif
                   done=1;
                   break;
         case 'U': {
                   register int i;
                   who=explode(buff+2,",", 0);
                   i = xsizeof(who);
                   re->uid = (i)? atoi(who[0]) : 0;
                   if (re->login) xfree_string(re->login);
                   re->login = xstrdup((i>1)? who[1] : "Unknown");
                   xfree_array(who); 
                   break;
            }
         }
      }
   }
   if (debug & DB_ARCH)
      printf("get_resp: returning response author %s date %lx flags %d textoff %ld\n", re->login,get_date(re->date,0),re->flags, re->textoff);
}
 
/******************************************************************************/
/* READ IN INFORMATION SUMMARIZING ALL THE RESPONSES IN AN ITEM               */
/* Note that this is currently only used within the current cf, but could     */
/* easily be used for a remote cf by passing in the right sum & re, confidx   */
/******************************************************************************/
void                             /* RETURNS: (nothing)                    */
get_item(fp,n,re,sum)            /* ARGUMENTS:                            */
   FILE      *fp;                /*    File pointer to read from          */
   short      n;                 /*    Which item # we're reading         */
   response_t re[MAX_RESPONSES]; /*    Buffer array to hold response info */
   sumentry_t sum[MAX_ITEMS];    /*    Buffer array holding info on items */
{
   short i;
   long offset=0;
 
   /* For each response */
   for (i=0; i<MAX_RESPONSES; i++) {
      re[i].offset = re[i].endoff = -1;
      re[i].date = 0;
   }

   /* Find EOF */
   fseek(fp, 0L, 2);  
   offset = ftell(fp);
   rewind(fp);

   /* Get all responses, and fix sum file NR value */
   for (i=0; !i || re[i-1].endoff < offset; i++) {
      get_resp(fp,&(re[i]),(short)GR_OFFSET,i);
      if (debug & DB_ARCH)
         printf("%2d Offset = %4o Textoff = %4o\n",i,re[i].offset, 
          re[i].textoff);
   }
/*printf("GR: %d =?= %d\n", sum[n-1].nr, i);*/
   if (sum[n-1].nr != i) {
      sum[n-1].nr = i;
      save_sum(sum,(short)(n-1),confidx,&st_glob);
      dirty_sum(n-1);
   }
}
