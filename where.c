/* $Id: where.c,v 1.4 1996/06/21 03:36:06 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include "yapp.h"
#include "struct.h"
#include "xalloc.h"
#include "files.h"
#ifndef HAVE_SYS_ERRLIST
extern char *sys_errlist[]; /* standard error messages, usu. in errno.h */
#endif
#define MAX_LIST_LENGTH 300

flag_t         debug =0;            /* user settable parameter flags */
flag_t         flags =0;            /* user settable parameter flags */
flag_t         status=0;            /* system status flags           */
int     uid;                       /* User's UID                    */
char    login[L_cuserid];          /* User's login                  */
char    bbsdir[MAX_LINE_LENGTH];   /* Directory for bbs files       */

assoc_t *conflist; /* System table of conferences   */
short   maxconf;

/******************************************************************************/
/* GENERATE STRING WITHOUT ANY _'s IN IT */
/******************************************************************************/
char *      /* RETURNS: New string */
compress(s) /* ARGUMENTS:          */
char *s;    /*    Original string  */
{
   static char buff[MAX_LINE_LENGTH];
   char *p,*q;

   for (p=buff,q=s; *q; q++)
      if (*q != '_')
         *p++ = *q;
   *p=0;
   return buff;
}

/******************************************************************************/
/* READ IN ASSOCIATIVE LIST                                                   */
/* ! and # begin comments and are not part of the list                        */
/* =filename chains to another file                                           */
/******************************************************************************/
assoc_t *               /* RETURNS: list on success, NULL on error */
grab_list(dir,filename) /* ARGUMENTS:                     */
   char   *dir;         /*    Directory containing file   */
   char   *filename;    /*    Filename to read from       */
{
   FILE    *fp;
   char     buff[MAX_LINE_LENGTH],
            name[MAX_LINE_LENGTH], /* Full pathname of filename */
           *loc;
   assoc_t *list;                /*    Array to fill in            */
   int      size;                /* Current size of array */

   /* Compose filename */
   if (filename && dir) sprintf(name,"%s/%s",dir,filename);
   else if (dir)        strcpy(name,dir);
   else if (filename)   strcpy(name,filename);
   else return 0;

   /* Open the file to read */
   if ((fp=mopen(name,O_R|O_SILENT))==NULL) {
      error("grabbing list ",name);
      return 0;
   }

   /* Get the first line (skipping any comments) - this is the default */
   do {
      loc=ngets(buff,fp);
   } while (loc && (buff[0]=='#' || buff[0]=='!'));

   /* If empty, return null array */
   if (!loc) {
      (void)fprintf(stderr,"Error: %s is empty.\n",name);
      return NULL;
   }

   /* Start the list, and save default in location 0 */
   list = (assoc_t *)xalloc(0, MAX_LINES*sizeof(assoc_t));
   list[0].name     = xstrdup("");
   list[0].location = xstrdup(buff);
   size = 1;
   if (debug & DB_LIB) 
      printf("Default: '%s'\n",buff);
   if (strchr(buff,':')) 
      (void)fprintf(stderr,"Warning: %s may be missing default.\n",name);
   
   /* Read until EOF */
   while (ngets(buff,fp)) {

      if (debug & DB_LIB) 
         printf("Buff: '%s'\n",buff);
      if (buff[0]=='#' || buff[0]=='\0') 
         continue; /* Skip comment and blank lines */

      /* Have a line, split into name and location */
      if (loc = strchr(buff,':')) {
         strncpy(name,buff,loc-buff);
         name[loc-buff]=0;
         loc++;

         if (size==(xsizeof(list)/sizeof(assoc_t)))
            list = (assoc_t *)xrealloc_string(list, (xsizeof(list)+MAX_LINES)*sizeof(assoc_t));

         list[size].name     = xstrdup(name);
         list[size].location = xstrdup(loc);
         if (debug & DB_LIB)
            printf("Name: '%s' Dir: '%s'\n",list[size].name, 
             list[size].location);
         size++;

      /* Chain to another file */
      } else if (buff[0]=='=' && strlen(buff)>1) {
         mclose(fp);

         if (buff[1]=='%') sprintf(name,"%s/%s",bbsdir,buff+2);
         else if (dir)     sprintf(name,"%s/%s",dir,buff+1);
         else              strcpy(name,buff+1);

         if ((fp=mopen(name,O_R|O_SILENT))==NULL) {
            error("grabbing list ",name);
            break;
         }
         ngets(buff,fp); /* read magic line */
         if (debug & DB_LIB) 
            printf("grab_list: magic %s\n",buff);

      } else {
         (void)fprintf(stderr,"Bad line read: %s\n",buff);
      }
   }
   mclose(fp);
   list = (assoc_t*)xrealloc_string(list, size*sizeof(assoc_t));
   return list;
}

void
free_list(list)
   assoc_t *list;
{
   int i, sz;
   
   sz = xsizeof(list)/sizeof(assoc_t);
   for (i=0; i<sz; i++) {
      xfree_string(list[i].name);
      xfree_string(list[i].location);
   }
   xfree_string(list);
}

void
where(argc, argv)
   int     argc;
   char  **argv;
{
   long    inode[MAX_LIST_LENGTH];
   int i,n, fl=0;
   struct stat st;
   char    buff[MAX_LINE_LENGTH],
           login[MAX_LINE_LENGTH];
   char    word[20][MAX_LINE_LENGTH];
   FILE   *fp,*pp;
   struct statfs buf;

   /* Check for -s flag */
   if (argc>1 && !strcmp(argv[1], "-s")) {
      fl=1;
      argc--;
      argv++;
   }

   /* Load in inode index */
   for (i=1; i<=maxconf; i++) {
      sprintf(buff,"%s/config", conflist[i].location);
      if (fl) {
         statfs(buff, &buf);
         if (buf.f_type==MOUNT_NFS)
            continue;
      }

      if (!stat(buff,&st))
         inode[i] = st.st_ino;
      else
         inode[i] = 0;
   }

   if ((fp = popen("/usr/bin/fstat","r"))==NULL) {
      printf("Can't open fstat\n");
      exit(1);
   }
   printf("USER     TT      PID CMD      CONFERENCE\n");
   while (fgets(buff,MAX_LINE_LENGTH,fp)) {
      if (sscanf(buff,"%s%s%s%s%s%s%s", 
        &(word[0]),
        &(word[1]),
        &(word[2]),
        &(word[3]),
        &(word[4]),
        &(word[5]),
        &(word[6])
      )<7 || !(n = atoi(word[5])))
         continue;

      for (i=1; i<=maxconf && inode[i]!=n; i++);
      if (i<=maxconf) {
         sprintf(buff,"ps -O ruser -p %s", word[2]);
         if ((pp = popen(buff,"r"))==NULL) {
            strcpy(word[12], word[0]);
            strcpy(word[13], "??"); /* actually this is available */
         } else {
            fgets(buff,MAX_LINE_LENGTH, pp);
            fgets(buff,MAX_LINE_LENGTH, pp); /* Real thing */
/*
 8431 thaler   s0  I+     0:00.97 nvi where.c
*/
            if (sscanf(buff,"%s%s%s%s%s%s",
             &(word[11]),
             &(word[12]),
             &(word[13]),
             &(word[14]),
             &(word[15]),
             &(word[16])
            )<6) {
               strcpy(word[12], word[0]);
               strcpy(word[13], "??"); /* actually this is available */
            }
            fclose(pp);
         }

         /* Check user list */
         if (argc>1) {
            for (i=1; i<argc && strcmp(word[12],argv[i]); i++);
            if (i==argc)
               continue;
         }
      
         printf("%-8s %s %8s %-8s %s\n", word[12], word[13], word[2], word[1], compress(conflist[i].name));
      }
   }
   fclose(fp);
}

int
main(argc, argv)
   int argc;
   char **argv;
{
   int     i;

   strcpy(bbsdir,BBSDIR);
   if (conflist=grab_list(bbsdir,"conflist")) {
      printf("Couldn't access conflist\n");
      exit(1);
   }
   maxconf = xsizeof(conflist)/sizeof(assoc_t);

   where(argc, argv);
      
   free_list(conflist);
}

void
wputs(s)
char *s;
{
   fputs(s,stdout); /* NO newline like puts() does */
}

void
wgets(a,b)
char *a,*b;
{
}

void
wputchar(c)
char c;
{
   putchar(c);
}

int
error(str1,str2)
char *str1,*str2;
{
   fprintf(stderr,"Got error %d (%s) in %s%s\n",errno,sys_errlist[errno],
    str1,str2);
}
