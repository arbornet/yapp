/* $Id: files.c,v 1.7 1996/06/21 03:37:19 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <sys/types.h> 
#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif
#include <sys/stat.h> 
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* to get O_CREAT, etc */
#endif
#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for close, etc */
#endif
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "files.h"
#include "xalloc.h"
#include "system.h" /* to get SL_OWNER */
#include "lib.h" /* for error() */

#ifndef LOCK_EX
#define LOCK_EX 2
#define LOCK_NB 4
#endif

/* Information about each open file */
typedef struct fd_tt {
   char         *filename;
   short         flg;
   int           fd;
   int           pid;  /* child process id, if secure */
   struct fd_tt *next;
} fd_t;
static fd_t *first_fd=0;

#ifndef HAVE_FLOCK
#ifdef HAVE_LOCKF
/******************************************************************************/
/* FLOCK FOR SYSTEMS WITH LOCKF() ONLY                                        */
/******************************************************************************/
int 
flock(fd, operation)
int fd, operation;
{
   return lockf(fd,(operation & LOCK_NB)? F_TLOCK : F_LOCK,0L);
}
#define HAVE_FLOCK
#endif
#endif

/******************************************************************************/
/* DUMP ALL FILES CURRENTLY OPEN                                              */
/******************************************************************************/
void    /* RETURNS: (nothing) */
mdump() /* ARGUMENTS: (none)  */
{
   fd_t *this;

   for (this=first_fd; this; this = this->next) {
      printf("mdump: fd=%d filename='%s' flg=%x\n",this->fd,
       this->filename, this->flg);
   }
}

/******************************************************************************/
/* ADD AN OPEN FD TO THE DATABASE (FROM SPOPEN/MOPEN)                         */
/******************************************************************************/
void
madd(fd,file,flg,pid)
int   fd;
char *file;
short flg;
int   pid;
{
   fd_t       *this;

   /* Save info for debugging */
   this           = (fd_t *)xalloc(0,sizeof(fd_t));
   this->fd       = fd;
   this->filename = xstrdup(file);
   this->flg      = flg;
   this->pid      = pid;
   this->next     = first_fd;
   first_fd       = this;
}

/******************************************************************************/
/* OPEN A FILE AND LOCK IT FOR EXCLUSIVE ACCESS                               */
/******************************************************************************/
FILE *         /* RETURNS: Open file pointer, or NULL on error */
mopen(file,flg) /* ARGUMENTS: */
char *file;    /*    Filename to open */
long  flg;     /*    Flag: 0=append only, 1=create new (only) */
{
   struct stat st;
   short       err=0;
   int         fd,
               perm;
   char        modestr[3];
   FILE       *fp;
   char        buff[MAX_LINE_LENGTH];
#ifndef HAVE_FLOCK
   short       timeout=0;
#endif

   if (debug & DB_FILES)
      printf("mopen: flags=%x\n",flg);

   /* Process flags: insure it exists or doesn't exist if required */
   if (flg & (O_EXCL|O_NOCREATE))  
      err=stat(file,&st);
   if ( err && (flg & O_NOCREATE)) {
      if (!(flg & O_SILENT)) 
         error("opening ",file);
      return NULL; /* append: doesn't exist  */
   } else if (!err && (flg & O_CREAT) && (flg & O_EXCL))    {
      if (!(flg & O_SILENT)) 
         error("creating ",file);
      return NULL; /* create: already exists */
   }
   perm=umask(0);

#ifndef HAVE_FLOCK
   /* For auxilary file locking */
   if (flg & O_LOCK) {
      sprintf(buff,"%s.lock",file);
      while ((fd=open(buff,O_WRONLY|O_CREAT|O_EXCL,0400))<0
       && errno==EEXIST && timeout<10 && !(status & S_INT)) {
			if (flg & O_NOBLOCK)
				return NULL; /* can't lock */
         timeout++;
         sleep(1);
      }
      if (fd>=0) close(fd);

   /* Currently overrides lock after timeout
    *
    * if (timeout>=10 || (status & S_INT)) {
    *    error("locking ",buff);
    *    umask(perm);
    *    return NULL;
    * }
    */
      if (timeout>=10) printf("Warning: overriding lock on %s\n",file);
   }
#endif

   /* Open file */
   fd=open(file,flg & O_PASSTHRU,(flg & O_PRIVATE)? 0600 : 0644);
   if (fd < 0) {
      if (!(flg & O_SILENT)) 
         error("opening ",file);
#ifndef HAVE_FLOCK
      if (flg & O_LOCK)
         rm(buff,SL_OWNER);      /* unlock */
#endif
      umask(perm);
      return NULL;
   }

#ifdef HAVE_FLOCK
   /* Lock it */
   if (flg & O_LOCK) {
      if (flock(fd,(flg & O_NOBLOCK)? LOCK_EX|LOCK_NB : LOCK_EX)) {
/*
 * ignore this error, since it may be /dev/null or something weird like
 * that. 8/4/95
 * error("Lock failed on ", file);
 */
/* allow it to continue without lock, can't flock an NFS file in BSD1.1 it seems
         umask(perm);
         return NULL;
*/
/* 8/28/95 need it to fail for license, fail only on NOBLOCK */
         if (flg & O_NOBLOCK) {
            umask(perm);
            close(fd);
/*error("Lock failed on ", file);*/
            return NULL;
         }
      }
   }
#endif

   /* Open/lock succeeded */
   umask(perm);
   if (flg & O_APPEND) 
      lseek(fd,0L,2);

   /* Determine mode string */
   if      ((flg & O_WPLUS)==O_WPLUS) strcpy(modestr,"w+");
   else if ((flg & O_W    )==O_W    ) strcpy(modestr,"w"); 
   else if ((flg & O_APLUS)==O_APLUS) strcpy(modestr,"a+");
   else if ((flg & O_A    )==O_A    ) strcpy(modestr,"a");
   else if ((flg & O_RPLUS)==O_RPLUS) strcpy(modestr,"r+");  /* should be next to last */
   else if ((flg & O_R    )==O_R    ) strcpy(modestr,"r");   /* MUST be last */
   else printf("KKK Invalid mopen mode\n"); 

   /* Save info for debugging */
   madd(fd,file,flg,0);

   /* Reopen fd as file pointer of equivalent mode */
   if ((fp = fdopen(fd,modestr))==NULL) {
      sprintf(buff,"%s for %s after mode %x\n",file,modestr,flg);
      if (!(flg & O_SILENT)) 
         error("reopening ",buff);
   }
   return fp;
}

int
get_pid(fp)
   FILE *fp;
{
   fd_t *this,*prev=0;
#ifndef HAVE_FLOCK
   char buff[MAX_LINE_LENGTH];
#endif

   if (!fp) {
      error("invalid file pointer passed to get_pid", NULL);
      return 0;
   }
   for (this=first_fd; this && this->fd != fileno(fp); prev=this, this = this->next);
   if (!this) {
      error("file pointer not found by get_pid", NULL);
      return 0; /* not found */
   }
   return this->pid;
}

/******************************************************************************/
/* CLOSE AND UNLOCK A FILE                                                    */
/******************************************************************************/
int         /* RETURNS: non-zero on error */
mclose(fp)  /* ARGUMENTS:               */
FILE *fp;   /*    File pointer to close */
{
   fd_t *this,*prev=0;
   int   ret;
#ifndef HAVE_FLOCK
   char buff[MAX_LINE_LENGTH];
#endif

   fflush(fp);
   for (this=first_fd; this && this->fd != fileno(fp); prev=this, this = this->next);
   if (!this) {
      (void)printf("Tried to close unopened file\n");
      return 1; /* not found */
   }
   if (!fp) {
      (void)printf("Tried to close null file\n");
      return 1; 
   }

   ret=fclose(fp); /* flock automatically closes */

#ifndef HAVE_FLOCK
   /* For auxilary file locking */
   if (this->flg & O_LOCK) {
      sprintf(buff,"%s.lock",this->filename);
      rm(buff,SL_OWNER);
   }
#endif

   /* Remove from debugging database */
   if (!prev) first_fd   = this->next;
   else       prev->next = this->next;
   xfree_string(this->filename);
   xfree_string((char *)this);

   return ret;
}

/******************************************************************************/
/* VERIFY THAT ALL FILES HAVE BEEN CLOSED                                     */
/******************************************************************************/
void
mcheck()
{
   if (!first_fd) {
      if (debug & DB_FILES)
         puts("mcheck: Everything closed.\n");
   } else {
      printf("mcheck: Error, failed to close the following:\n");
      mdump();
   }
}
