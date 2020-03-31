/* Routines for dealing with editing responses and subjects */
/* $Id: edit.c,v 1.7 1998/06/17 17:54:35 kaylene Exp $ */

/*
 * Caveat: note that another process may be in the middle of reading or
 * writing to the item file, and the sum file contains offsets into the
 * item file which will all change if the header or the response text
 * changes.
 *
 * One option would be, for editing a response, to scribble the old
 * response, and copy the text to the entry buffer and let you enter
 * a new one (which is completely safe).
 *
 * Another option would be to go to a DBM format item file which would
 * obviate the need for keeping offsets.
 *
 * Another option would be to lock the sum file, wait a second 
 * or two, then do the change and regenerate the sum file.  This is not
 * safe because it doesn't 100% guarantee someone else won't be messed up 
 * (say they're going to do a scribble and you change the subject to be 
 * longer or shorter...).  If only the sum file is locked, changes are
 * pretty good though, that Yapp will be done reading the item file
 * within a second or two.
 *
 * Editing the last response in the file should be safe, however, since no
 * offsets in the sum file change.
 *
 * So, for editing responses, modify the text only if it's the last response
 * and do the scribble/append otherwise.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h> /* for memset */
#ifdef HAVE_STDLIB
# include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "system.h" /* for SL_OWNER */
#include "files.h"  /* for mopem */
#include "xalloc.h" /* for xfree_string */
#include "sum.h"    /* for save_sum */
#include "stats.h"  /* for store_subj */
#include "macro.h"  /* for subject */
#include "lib.h"    /* for ngets */
#include "sep.h"    /* for get_sep */
#include "range.h"  /* for rangeinit */
#include "item.h"   /* for is_enterer */
#include "license.h" /* for get_conf_param */
#include "arch.h"   /* for GR_ALL */
#ifndef SEEK_END
#define SEEK_END 2
#endif

/* Malloc a duplicate string without leading or trailing spaces */
char *
despace(str)
   char *str;
{
   char *ptr, *p;

   while (isspace(*str)) str++; /* skip leading spaces */
   ptr = xstrdup(str);
   for (p=ptr+strlen(ptr)-1; p>=ptr && isspace(*p); p--); /* skip trailing */
   p[1]='\0'; /* delete trailing spaces */
   return ptr;
}

char *
spaces(n)
   int n;  /* # of spaces in range [0,512] */
{
   static char spc[MAX_LINE_LENGTH], init=0;
   if (!init) {
      memset(spc, ' ', sizeof(spc)-1);
      spc[ sizeof(spc)-1 ]='\0';
      init=1;
   }
   return (n>=0)? spc+sizeof(spc)-1-n : spc+sizeof(spc)-1;
}

int
dump_file(dir,filename,text,mod)
   char  *dir;       /*    IN: Directory to put file in    */
   char  *filename;  /*    IN: Filename to write text into */
   char **text;      /*    IN: Text to write out           */
   int    mod;       /*    IN: File open mode              */
{
   char buff[MAX_LINE_LENGTH];
   FILE *fp;
   int   i, n;

   if (filename)
      sprintf(buff,"%s/%s",dir,filename);
   else
      strcpy(buff,dir);
   if ((fp=mopen(buff, mod))==NULL)
      return 0;

   n = xsizeof(text);
   for (i=0; i<n; i++)
      fprintf(fp, "%s\n", text[i]);
   mclose(fp);
   return 1;
}

/*
 * Change subject of st_glob.i_current item in confidx conference 
 * We ONLY allow this if the offsets don't change (i.e., new subject fits 
 * into space holding old subject).  Newly created items now hold 78 bytes
 * of space for the subject, regardless of the string length.
 */
int 
retitle(argc,argv)
   int argc;
   char **argv;
{
   char act[MAX_ITEMS],
        **header;
   char sub[MAX_LINE_LENGTH];
   char itemfile[MAX_LINE_LENGTH];
   int j;

   rangeinit(&st_glob, act);

   if (argc<2) { 
      printf("Error, no %s specified! (try HELP RANGE)\n", topic(0));
   } else { /* Process args */
      range(argc,argv,&st_glob.opt_flags,act,sum,&st_glob,0);
   }

   /* Process items */
   for (j=st_glob.i_first; j<=st_glob.i_last && !(status & S_INT); j++) {
      if (!act[j-1] || !sum[j-1].flags) continue;

      /* Check for permission */
      if (!(st_glob.c_status & CS_FW) && !is_enterer(j)) {
         printf("You can't do that!\n");
         continue;
      }

      if (!(flags & O_QUIET)) {
         printf("Old subject was:\n> %s\n", get_subj(confidx, j-1, sum));
         printf("Enter new %s or return to keep old\n? ", subject(0));
      }
   
      ngets(sub, st_glob.inp); /* st_glob.inp */
      if (!sub[0])
         return 1; /* keep old */

      /* Expand seps in subject IF first character is % */ 
      if (sub[0]=='%') {
         char *str, *f;
         str = sub+1;
         f = get_sep(&str);
         strcpy(sub, f);
      }

      /* Determine length of old subject */
      sprintf(itemfile,"%s/_%d", conflist[confidx].location, j);
      header = grab_file(itemfile, NULL, GF_HEADER);
      if (xsizeof(header)>1) {
         int len = strlen(header[1])-2;
         FILE *fp;

         /* Do some error checking, should never happen */
         if (len > MAX_LINE_LENGTH) {
            error("subject too long in ", itemfile);
            xfree_array(header);
            return 1;
         }

         /* Truncate if necessary */
         if (strlen(sub)>len) {
            sub[len] = '\0';
            if (!(flags & O_QUIET))
               printf("Truncated subject to: %s", sub);
         }

         /* Store in memory */
         store_subj(confidx, (short)(j-1), sub);

         /* Store into item file */
         if ((fp=mopen(itemfile,O_RPLUS))!=NULL) {
            if (fseek(fp, 10L, 0))
               error("fseeking in ", itemfile);
            else{
               if(len-strlen(sub) < MAX_LINE_LENGTH){
                  fprintf(fp, "%s%s\n", sub, spaces(len-strlen(sub)));
               }else{
                  int spaces_added; 
                  int spaces_to_add = len-strlen(sub);

                  fprintf(fp, "%s", sub); /* Add new text */
                  for (spaces_added=0; 
                   spaces_to_add - spaces_added >= MAX_LINE_LENGTH; 
                   spaces_added += (MAX_LINE_LENGTH-1)){
                     fprintf(fp, "%s", spaces(MAX_LINE_LENGTH-1));
                  }

                  /* Add any remaining spaces */
                  fprintf(fp, "%s\n", spaces(spaces_to_add - spaces_added));
               }
            }
            mclose(fp);
         }

#ifdef SUBJFILE
         XXX /* Fill this in if we ever use SUBJFILE */
#endif
 
         custom_log("retitle", M_RFP);
      }
      xfree_array(header);
   }
   return 1;
}

/******************************************************************************
 * EDIT A RESPONSE IN THE CURRENT ITEM                                        *
 *                                                                            *
 * If the new text fits into the old space, we overwrite it (padding it if    *
 * necessary).  If not, we just scribble the old response, and add a new      *
 * one at the end of the item.                                                *
 ******************************************************************************/
int               /* RETURNS: (nothing)          */
modify(argc,argv) /* ARGUMENTS:                  */
   int    argc;   /*    Number of arguments      */
   char **argv;   /*    Argument list            */
{
   char buff[MAX_LINE_LENGTH],over[MAX_LINE_LENGTH];
   char cenbuff[MAX_LINE_LENGTH];
   int i,j, n, oldlen, newlen;
   FILE *fp;
   char **text=NULL; 
   register int cpid,wpid;
   int statusp, ok;
extern FILE *ext_fp;

   if (st_glob.c_security & CT_EMAIL) {
      wputs("Can't modify in an email conference!\n");
      return 1;
   }
#ifdef NEWS
   if (st_glob.c_security & CT_NEWS) {
      wputs("Can't modify in a news conference!\n");
      return 1;
   }
#endif

   /* Validate arguments */
   if (argc<2 || sscanf(argv[1],"%d",&i)<1) {
      wputs("You must specify a response number.\n");
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

   /* Check for permission to modify (only the author can modify) */
   if (!re[i].date) get_resp(ext_fp,&(re[i]),GR_HEADER,i);
/*
 * Only match logins if coming from or entered from web.  Slightly
 * dangerous, but it's better than not letting the author edit 
 */
   if (uid==get_nobody_uid() || re[i].uid==get_nobody_uid())
      ok = !strcmp(login, re[i].login);
   else
      ok = (uid == re[i].uid);
   if (!ok)
#if 0
   if ((uid!=re[i].uid 
    || (uid==get_nobody_uid() && strcmp(login,re[i].login)))) 
#endif
   {
      printf("You don't have permission to affect response %d.\n", i);
      return 1;
   }
   if (sum[st_glob.i_current-1].flags & IF_FROZEN) {
      sprintf(buff,"Cannot modify frozen %ss!\n", topic(0));
      wputs(buff);
      return 1;
   }

   if (re[i].flags & RF_SCRIBBLED) {
      wputs("Cannot modify a scribbled response!\n");
      return 1;
   }
   if (re[i].offset < 0) {
      wputs("Offset error.\n"); /* should never happen */
      return 1;
   }

   /* Get old text */
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
      dump_file(work, "cf.buffer", re[i].text, O_W);

      exit(0);
   }

   /* Get new text */
   if (!text_loop(0,"text")
    || !(text = grab_file(work, "cf.buffer", GF_NOHEADER))) {
      xfree_array( re[i].text );
      return 1;
   }

   /* Write old text to censorlog */
   sprintf(cenbuff,"%s/censored",bbsdir);
   if ((fp=mopen(cenbuff,O_A|O_PRIVATE)) != NULL) {
      fprintf(fp,",C %s %s %d resp %d rflg %d %s,%d %s date %s\n",
       conflist[confidx].location, topic(0), st_glob.i_current, i, 
        RF_CENSORED|RF_SCRIBBLED, login, uid, get_date(time((time_t *)0),0), 
        fullname_in_conference(&st_glob));
      fprintf(fp,",R%04X\n,U%d,%s\n,A%s\n,D%08X\n,T\n",
       re[i].flags,re[i].uid,re[i].login,re[i].fullname,re[i].date);
      for (j=0; j<xsizeof(re[i].text); j++) 
         fprintf(fp,"%s\n",re[i].text[j]);
      fprintf(fp,",E\n");
      mclose(fp);
   }

   /* Escape the new text if needed, and see if it fits into the old space */
   oldlen = (re[i].endoff-3) - re[i].textoff;
   n = xsizeof(text);
   for (newlen = 0, j=0; j<n; j++) {
      if (text[j][0]==',') {
         char *ptr = (char *)xalloc(0, strlen(text[j])+3);
         ptr[0]=ptr[1]=',';
         strcpy(ptr+2, text[j]);
         xfree_string(text[j]);
         text[j] = ptr;
      }
      newlen += strlen(text[j])+1; /* count 1 for the newline */
   }

   /* Overwrite the old text with either scribbling or with the new text */
   if ((fp=mopen(buff,O_RPLUS))!=NULL) {
      int is_at_end=0;

      /* If it's the last response, the space can be extended */
      fseek(fp, 0L, SEEK_END);
      if (ftell(fp)==re[i].endoff) {
         oldlen = newlen+atoi(get_conf_param("padding",PADDING)); 
         is_at_end=1;
      } 
      
      /* Append if the new text is longer */
      if (newlen > oldlen) {
         int len, k;

         fseek(fp,re[i].offset,0);
         fprintf(fp,",R%04d\n", RF_CENSORED|RF_SCRIBBLED); 
         fseek(fp,re[i].textoff,0);
         sprintf(over,"%s %s %s ",login,get_date(time((time_t *)0),0),
          fullname_in_conference(&st_glob));
         len = strlen(over);
         for (j=oldlen; j>76; j-=76) {
            for (k=0; k<75; k++)
               fputc(over[k%len],fp);
            fputc('\n',fp);
         }
         for (k=0; k<j-1; k++)
            fputc(over[k%len],fp);
         fprintf(fp, "\n,E\n");
      } else {
         /* Replace if the new text fits in the old space */
         int k;

         /* Write new timestamp */
         for (k=14; k>7; k--) {
            int c;
            fseek(fp,re[i].textoff-k,0);
            c = getc(fp);
            if (c==',')
               break;
         }
         if (k==7) {
            char errmsg[100];
            sprintf(errmsg, "%s #%d.%d", compress(conflist[confidx].name),
             st_glob.i_current, i);
            error("lost date in ", errmsg);
         } else {
            fseek(fp,re[i].textoff-k,0);
            fprintf(fp, ",D%08X\n", time((time_t *)0));
         }

         /* Write new text and pad with spaces after the ,E */
         fseek(fp,re[i].textoff,0);
         for (j=0; j<n-1; j++)
            fprintf(fp, "%s\n", text[j]);
         if( oldlen-newlen < MAX_LINE_LENGTH){
            fprintf(fp, "%s\n,E%s\n", text[j], spaces(oldlen-newlen));
         }else {
            int spaces_added; 
            int spaces_to_add = oldlen-newlen;

            fprintf(fp, "%s\n,E", text[j]);
            for (spaces_added=0; 
             spaces_to_add - spaces_added >= MAX_LINE_LENGTH; 
             spaces_added += (MAX_LINE_LENGTH-1)){
               fprintf(fp, "%s", spaces(MAX_LINE_LENGTH-1));
            }

            /* Add any remaining spaces */
            fprintf(fp, "%s\n", spaces(spaces_to_add - spaces_added));
         }

         if (is_at_end)
            ftruncate(fileno(fp), ftell(fp));
      }
      mclose(fp);

      /* Added 4/18, since sum file wasn't being updated, causing set
       * sensitive to fail.
       */
      sum[ st_glob.i_current-1 ].last  = time((time_t *)0);
      save_sum(sum, (short)(st_glob.i_current-1), confidx, &st_glob);
      dirty_sum(st_glob.i_current-1);

   }

   if (newlen > oldlen) {
      /*
       * At this point, the old text is scribbled, and the new text is
       * in text, so just do a normal add response.
       */
      add_response(&(sum[st_glob.i_current-1]), text, confidx, sum, part,
       &st_glob, 0, NULL, uid, login, re[i].fullname, re[i].parent);
   }

   /* free_sum(sum); unneeded, always SF_FAST */

   xfree_array(text);
   xfree_array(re[i].text);

   custom_log("edit", M_RFP);
  
   /* Remove the cf.buffer file now that we are done*/
   sprintf(buff,"%s/%s",work,"cf.buffer");
   rm(buff,SL_USER);

   return 1;
}
