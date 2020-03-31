/* $Id: system.c,v 1.17 1998/02/10 11:36:18 kaylene Exp $ */

/* SYSTEM.C - Dave Thaler 3/1/93
 * This file does secure replacements for popen and system calls 
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "yapp.h"
#include "struct.h"
#include "lib.h"
#include "xalloc.h"
#include "macro.h"
#include "globals.h"
#include "files.h"
#include "driver.h"
#include "system.h"
#include "main.h" /* for wputs */
#include "edbuf.h" /* for text_loop */

#ifdef HAVE_FSTAT
extern FILE *conffp;
#endif

void
process_pipe_input(fd)
   int fd;
{
   char *p, *eb;
   int n, sz;
   fd_set rfds;
   struct timeval tm;

   /* read output into evalbuf */
   sz = strlen(evalbuf);
   eb = evalbuf + sz;
   FD_ZERO(&rfds);
   FD_SET(fd, &rfds);
   tm.tv_sec = 0;
   tm.tv_usec = 0;

   /* do { */
      /* must do a non-blocking read in case there was no output */
      if (select(fd+1, &rfds, NULL, NULL, &tm) > 0) {
         n = read(fd, eb, sizeof(evalbuf)-1-sz);
         if (n>0)
            eb[n]='\0';
if (debug & DB_PIPE) { fprintf(stderr, "read in %d chars from pipe\n", n); fflush(stderr); }
      }
   /* } while (n>=0); * repeat until process ends and we get an EOF */

   /* Convert all newlines to spaces */
   for (p=evalbuf; *p; p++) {
      if (*p=='\n' || *p=='\r')
         *p=' ';
   }

/*printf("Got !%s! in evalbuf\n", evalbuf);*/
}

static int sp_cpid;

int 
smclose(fp)
   FILE *fp;
{
   int statusp, i, wpid;
   int   pid = get_pid(fp);

   i=mclose(fp);
   while ((wpid = waitpid(pid, &statusp, 0)) != pid && wpid != -1);

/*printf("<P>Wait completed with status %d.<P>\n", statusp); fflush(stdout);*/
   return i;
}

/******************************************************************************/
/* SECURE mopen() - OPEN A USER FILE                                          */
/******************************************************************************/
FILE *         /* RETURNS: Open file pointer, or NULL on error */
smopenw(file,flg) /* ARGUMENTS: */
   char *file;    /*    Filename to open */
   long  flg;     /*    Flag: 0=append only, 1=create new (only) */
{
  int fd[2];
  FILE *fp;
  int  pid;

  if (pipe(fd)<0) return NULL;
  if ((fp=fdopen(fd[1],"w"))==NULL) return NULL;
  
  /* The following two lines are not necessary, and even slow the program
   * down, but make seen be more accurate in some cases, since less info is
   * buffered by the pager.
   */
/* fcntl(fd[1], F_SETFL, O_SYNC);
   setsockopt(fd[1],SOL_SOCKET, SO_SNDBUF, 4, 1);
 */

  fflush(stdout);
  if (status & S_PAGER) 
     fflush(st_glob.outp);

  pid=fork();
  if (pid) { /* parent */
     if (pid<0) return NULL; /* error: couldn't fork */
     close(fd[0]);
     status |= S_PIPE;
     madd(fd[1],file,O_PIPE, pid);
     return fp;
  } else { /* child */
     FILE *ufp;
     char buff[1024];
     int len;

     setuid(getuid());
     setgid(getgid());
     signal(SIGINT,SIG_DFL);
     signal(SIGPIPE,SIG_DFL);
     close(0);
     dup(fd[0]);
     close(fd[1]);

     if ((ufp = mopen(file, flg))!=NULL) {

#if 0
        /* Close stdout, stderr in case they are also pipes 
         * This is necessary to allow multiple output pipes,
         * as in:  
         *    unix a.out > /tmp/out >& /tmp/err
         * or else the error child will also keep the output child
         * as stdout, and we won't be able to close stdout from
         * the parent.
         */
        close(1);
        close(2);
#endif

        /* Now copy all standard input to ufp */
        while ((len = read(0, buff, sizeof(buff))) > 0) {
           fwrite(buff, len, 1, ufp);
/*printf("<P>%s", buff); fflush(stdout);*/
        }

/*printf("<P>Closing buffer<P>\n"); fflush(stdout);*/
        mclose(ufp);
     }
     exit(1);
  }
  return NULL;
}
FILE *         /* RETURNS: Open file pointer, or NULL on error */
smopenr(file,flg) /* ARGUMENTS: */
   char *file;    /*    Filename to open */
   long  flg;     /*    Flag: 0=append only, 1=create new (only) */
{
  int fd[2];
  FILE *fp;
  int  pid;

  if (pipe(fd)<0) return NULL;
  if ((fp=fdopen(fd[0],"r"))==NULL) return NULL;
  
  /* The following two lines are not necessary, and even slow the program
   * down, but make seen be more accurate in some cases, since less info is
   * buffered by the pager.
   */
/* fcntl(fd[1], F_SETFL, O_SYNC);
   setsockopt(fd[1],SOL_SOCKET, SO_SNDBUF, 4, 1);
 */

  fflush(stdout);
  if (status & S_PAGER) 
     fflush(st_glob.outp);

  pid=fork();
  if (pid) { /* parent */
     if (pid<0) return NULL; /* error: couldn't fork */
     close(fd[1]);
     status |= S_PIPE;
     madd(fd[0],file,O_PIPE, pid);
     return fp;
  } else { /* child */
     FILE *ufp;
     char buff[1024];
     int len;

     setuid(getuid());
     setgid(getgid());
     signal(SIGINT,SIG_DFL);
     signal(SIGPIPE,SIG_DFL);
     close(1);
     if (status & S_PAGER) 
        fclose(st_glob.outp);
     dup(fd[1]);
     close(fd[0]);

     /* Close stdin, stderr in case they are also pipes */
     /* 10/28/95: these were 'if 0'ed out, but I'm not sure why.
      * Solaris requires this to be done or else the following test
      * fails: "source read2" gives duplicate output, where read2 is:
      *    echo normal
      *    eval < rh
      *    echo duplicated
      * and rh is:
      *    anything
      */
     /* 1/13/97 These were inside the if block below, but if the file doesn't
      * exist, then we have the same problem:
      *    #!/usr/local/bin/bbs -qx
      *    debug ioredir
      *    set source
      *    join yapp
      *    echo HELLO
      * and the HELLO is printed twice.
      */
     close(0);
     close(2);

/*printf("opening %s for %x\n", file, flg);*/
     if ((ufp = mopen(file, flg))!=NULL) {

        /* Now copy ufp to standard output */
        while ((len = fread(buff, 1, sizeof(buff), ufp)) > 0) {
           write(1, buff, len);
        }

        mclose(ufp);
     }
     exit(1);
  }
  return NULL;
}

/******************************************************************************/
/* SECURE sdpopen() - OPEN A TWO-WAY PIPE TO A PROCESS                        */
/******************************************************************************/
int                        /* RETURNS: 1 on success, 0 on failure */
sdpopen(finp, foutp, cmd)  /* ARGUMENTS:                          */
   FILE **finp;            /*    OUT: file pointer for input      */
   FILE **foutp;           /*    OUT: file pointer for output     */
   char  *cmd;             /*    IN : command to execute          */
{
   int fd_tocmd[2],s;
   int fd_fromcmd[2];
   char **argv;
   FILE *fin, *fout;
 
   if (foutp && pipe(fd_tocmd)<0)
      return 0;
 
   if (finp && pipe(fd_fromcmd)<0) {
      close(fd_tocmd[0]);
      close(fd_tocmd[1]);
      return 0;
   }
  
  /* The following two lines are not necessary, and even slow the program
   * down, but make seen be more accurate in some cases, since less info is
   * buffered by the pager.
   */
/* fcntl(fd[1], F_SETFL, O_SYNC);
   setsockopt(fd[1],SOL_SOCKET, SO_SNDBUF, 4, 1);
 */

  fflush(stdout);
  fflush(stderr);
  sp_cpid=fork();
  if (sp_cpid) { /* parent */
     if (sp_cpid<0)
        return 0; /* error: couldn't fork */
     status |= S_PIPE;

if (debug & DB_PIPE) {
   char buff[256];

   fprintf(stderr,"(stderr) Opened pipe to '%s': ",cmd);
   if (finp) fprintf(stderr,"in fd %d ", fd_fromcmd[0]);
   if (foutp) fprintf(stderr,"out fd %d ", fd_tocmd[1]); 
   fprintf(stderr, "\n"); fflush(stderr);

   fprintf(stdout,"(stdout) Opened pipe to '%s': ",cmd);
   if (finp) fprintf(stdout,"in fd %d ", fd_fromcmd[0]);
   if (foutp) fprintf(stdout,"out fd %d ", fd_tocmd[1]); 
   fprintf(stdout, "\n"); fflush(stdout);
}

     if (foutp) {
        close(fd_tocmd[0]);
        madd(fd_tocmd[1],  cmd,O_PIPE, sp_cpid);
        if ((fout=fdopen(fd_tocmd[1],  "w"))==NULL) 
           return 0;
        (*foutp) = fout;
     }
     if (finp) {
        close(fd_fromcmd[1]);
        madd(fd_fromcmd[0],cmd,O_PIPE, sp_cpid);
        if ((fin=fdopen(fd_fromcmd[0],"r"))==NULL) 
           return 0;
        (*finp) = fin;
     }
     return 1;
  } else { /* child */
     setuid(getuid());
     setgid(getgid());
     signal(SIGINT,SIG_DFL);
     signal(SIGPIPE,SIG_DFL);
     if (foutp) {
        close(0);
        dup(fd_tocmd[0]);
        close(fd_tocmd[1]);
     }

     if (finp) {
        close(1);
        dup(fd_fromcmd[1]);
        close(fd_fromcmd[1]);
     }
#ifdef HAVE_FSTAT
     if (confidx>=0)
        mclose(conffp);
#endif

     if (strpbrk(cmd,"<>*?|![]{}~`$&';\\\"") == NULL) {
        argv = explode(cmd," ", 1);
        s    = xsizeof(argv);
        argv = xrealloc_array(argv, s+1);
        argv[s]=0;
        execvp(argv[0],argv);
        printf("oops: Can't execute \"%s\"!\n",cmd);
     } else {
        char *shpath,*sh;

        shpath = expand("shell",DM_VAR);
        sh = strrchr(shpath,'/');
        if (!sh) sh = shpath;
        else sh++;
        execl(shpath,sh,"-c",cmd,(char *)NULL);
     }
     exit(1);
  }
  return 0;
}

/******************************************************************************/
/* SECURE spopen() - OPEN A PIPE TO A PROCESS                                 */
/******************************************************************************/
FILE *      /* RETURNS: file pointer of pipe */
spopen(cmd) /* ARGUMENTS: */
char *cmd;  /*    Command to pipe to */
{
  int fd[2],s;
  char **argv;
  FILE *fp;

  if (status & (S_PAGER|S_SOCKET)) {
     wputs("Error, pipe already open\n");
  }

  if (!sdpopen(NULL, &fp, cmd))
     return NULL;

   return fp;
}

/******************************************************************************/
/* DUMP A STRING TO A SECURE PIPE                                             */
/******************************************************************************/
int            /* RETURNS: # bytes written */
spout(fd,buff) /* ARGUMENTS: */
int fd;        /*    File descriptor of pipe */
char *buff;    /*    String to dump */
{
   return write(fd,buff,strlen(buff));
}

/******************************************************************************/
/* CLOSE A SECURE PIPE                                                        */
/******************************************************************************/
int         /* RETURNS: error code */
sdpclose(fin,fout) /* ARGUMENTS: */
   FILE *fin;
   FILE *fout;
{
   int i;
   int statusp, wpid;

   if (!(status & (S_PAGER|S_PIPE))) {
      printf("Error, pipe not open\n");
   }

   if (fout) {
if (debug & DB_PIPE) {
   fprintf(stderr,"(stderr) Closing pipe on fd %d\n",fileno(fout)); fflush(stderr);
   fprintf(stdout,"(stdout) Closing pipe on fd %d\n",fileno(fout)); fflush(stdout);
}
      i=mclose(fout);
   }

   if (status & S_PIPE)
      while ((wpid = waitpid(sp_cpid, &statusp, 0)) != sp_cpid && wpid != -1);

   if (fin) {
      if (status & S_EXECUTE)
         process_pipe_input(fileno(fin));
      i=mclose(fin);
   }

   status &= ~(S_PIPE|S_PAGER|S_INT);
   return i;
}

/******************************************************************************/
/* CLOSE A SECURE PIPE                                                        */
/******************************************************************************/
int         /* RETURNS: error code */
spclose(pp) /* ARGUMENTS: */
FILE *pp;   /*    File pointer to close */
{
   return sdpclose(NULL,pp);
#if 0
   int i;
   int statusp, wpid;

   if (!(status & (S_PAGER|S_PIPE))) {
      printf("Error, pipe not open\n");
   }

if (debug & DB_PIPE) {
   fprintf(stderr,"(stderr) Closing pipe on fd %d\n",fileno(pp)); fflush(stderr);
   fprintf(stdout,"(stdout) Closing pipe on fd %d\n",fileno(pp)); fflush(stdout);
}

   i=mclose(pp);
   if (status & S_PIPE) {
      while ((wpid = waitpid(sp_cpid, &statusp, 0)) != sp_cpid && wpid != -1);
   }
   status &= ~(S_PIPE|S_PAGER|S_INT);
   return i;
#endif
}

int exit_status=0; /* exit status of last unix command executed */

/******************************************************************************/
/* SECURE system() - EXECUTE A UNIX COMMAND                                   */
/******************************************************************************/
int           /* RETURNS: exit status of command */
unix_cmd(cmd) /* ARGUMENTS: */
char *cmd;    /*    Command to execute */
{
   char **argv;
   short s;
   register int cpid,wpid;
   int statusp;
   int fd[2];
#if 0
#ifdef STUPID_REOPEN
   extern FILE *real_stdin, *new_stdin(FILE *x);
   FILE *saved_inp = NULL;
#else
   extern int real_stdin;
#endif
#endif
   int saved_stdin_stack_top;

/*printf("UNIX1 ftell=%d\n", ftell(st_glob.inp));*/

   fflush(stdout);
   if (status & S_PAGER) 
      fflush(st_glob.outp);

   /* Prepare to capture output for a `unix ...` replacement */
   if (status & S_EXECUTE) {
      if (pipe(fd)<0) 
         return -1;
   }

#ifdef STUPID_REOPEN
      /*
       * If this is a script, and we didn't specify a specific
       * input redirection, then restore the REAL stdin
       */
      if ((status & S_BATCH) && !(status & S_NOSTDIN)) {
         saved_stdin_stack_top = stdin_stack_top;
         if (debug & DB_IOREDIR)
            printf("Restoring the real stdin\n");
         push_stdin(stdin_stack[0], STD_TTY); /* saved_inp = new_stdin(real_stdin); */
      }
#endif

   cpid=fork();
   if (cpid) { /* parent */
      if (cpid<0) return -1; /* error: couldn't fork */
      signal(SIGINT,SIG_IGN);
      signal(SIGPIPE,SIG_IGN);
      while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);
      signal(SIGINT,handle_int);
      signal(SIGPIPE,handle_pipe);
      if (status & S_EXECUTE) {
         process_pipe_input(fd[0]);
#if 0
         char *p, *eb;
         int n, sz;
         fd_set rfds;
         struct timeval tm;

         /* read output into evalbuf */
         sz = strlen(evalbuf);
         eb = evalbuf + sz;
         FD_ZERO(&rfds);
         FD_SET(fd[0], &rfds);
         tm.tv_sec = 0;
         tm.tv_usec = 0;
         /* must do a non-blocking read in case there was no output */
         if (select(fd[0]+1, &rfds, NULL, NULL, &tm) > 0) {
            n = read(fd[0], eb, sizeof(evalbuf)-1-sz);
            if (n>0)
               eb[n]='\0';
         }

         /* Convert all newlines to spaces */
         for (p=evalbuf; *p; p++) {
            if (*p=='\n' || *p=='\r')
               *p=' ';
         }

/*printf("Got !%s! in evalbuf\n", evalbuf);*/
#endif
      }
   } else { /* child */
      setuid(getuid());
      setgid(getgid());
      if (status & S_EXECUTE) {
         close(1);          /* close stdout */
         dup(fd[1]);        /* reopen pipe-out as stdin */
         close(fd[0]);      /* close pipe-in */
      }

#ifndef STUPID_REOPEN
      /*
       * If this is a script, and we didn't specify a specific
       * input redirection, then restore the REAL stdin
       */
      if ((status & S_BATCH) && !(status & S_NOSTDIN)) {
         close(0);
         dup(saved_stdin[0].fd); /* dup(real_stdin); */
      }
#endif

      signal(SIGINT,SIG_DFL);
      signal(SIGPIPE,SIG_DFL);
#ifdef HAVE_FSTAT
      if (confidx>=0)
        mclose(conffp);
#endif
      if (strpbrk(cmd,"<>*?|![]{}~`$&';\\\"") == NULL) {
         argv = explode(cmd," ", 1);
         s    = xsizeof(argv);
         argv = xrealloc_array(argv, s+1);
         argv[s]=0;
         execvp(argv[0],argv);
         printf("oops: Can't execute \"%s\"!\n",cmd);
      } else {
         char *shpath,*sh;

         shpath = expand("shell",DM_VAR);
         sh = strrchr(shpath,'/');
         if (!sh) sh = shpath;
         else sh++;
         execl(shpath,sh,"-c",cmd,(char *)NULL);
      }
      exit(1);
   }

#ifdef STUPID_REOPEN
/*
   if (saved_inp)
      real_stdin = new_stdin(saved_inp);
*/
   while (stdin_stack_top > saved_stdin_stack_top)
      pop_stdin();
#endif

/*printf("UNIX2 ftell=%d\n", ftell(st_glob.inp));*/
   exit_status = WEXITSTATUS(statusp);
   return statusp;
}

/******************************************************************************/
/* REMOVE A FILE                                                              */
/******************************************************************************/
int                     /* RETURNS: error code */
rm(file,sec)        /* ARGUMENTS: */
char *file;
int   sec;              /*    As owner(0) or user(1)? */
{
   register int cpid,wpid;
   int statusp;

   fflush(stdout);
   if (status & S_PAGER) 
      fflush(st_glob.outp);

   if (!sec) {
      statusp=unlink(file);
   } else {
      cpid=fork();
      if (cpid) { /* parent */
         if (cpid<0) return -1; /* error: couldn't fork */
         while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);
      } else { /* child */
         signal(SIGINT,SIG_DFL);
         signal(SIGPIPE,SIG_DFL);
         close(0);

         setuid(getuid());
         setgid(getgid());
         exit(unlink(file));
      }
   }

/* This Error message removed 7/24/95 at request of janc, so
 * that his `gate` editor will not give this error
 *
 * if (statusp) error("removing ",file);
 */

   return statusp;
}

/******************************************************************************/
/* SECURELY COPY ONE FILE TO ANOTHER                                          */
/******************************************************************************/
int                     /* RETURNS: error code */
copy_file(src,dest,sec) /* ARGUMENTS: */
   char *src;              /*    Source file */
   char *dest;             /*    Destination file */
   int   sec;              /*    As owner(0) or user(1)? */
{
   FILE *fsrc,*fdest;
   int c;
   register int cpid,wpid;
   int statusp;
   long mod;

   fflush(stdout);
   if (status & S_PAGER) 
      fflush(st_glob.outp);

   cpid=fork();
   if (cpid) { /* parent */
      if (cpid<0) return -1; /* error: couldn't fork */
      while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);
   } else { /* child */
      signal(SIGINT,SIG_DFL);
      signal(SIGPIPE,SIG_DFL);
      close(0);

#ifdef HAVE_FSTAT
      if (confidx>=0)
         mclose(conffp);
#endif

      mod = O_W;
      if (!sec && (st_glob.c_security & CT_BASIC)) mod |= O_PRIVATE;

      if (sec) { /* cfadm to user */
         if ((fsrc=mopen(src,O_R))==NULL) exit(1);
         setuid(getuid());
         setgid(getgid());
         if ((fdest=mopen(dest,mod))==NULL) { 
            mclose(fsrc); 
            exit(1); 
         }
      } else {   /* user to cfadm */
         if ((fdest=mopen(dest,mod))==NULL) exit(1);
         setuid(getuid());
         setgid(getgid());
         if ((fsrc=mopen(src,O_R))==NULL) { 
            mclose(fdest); 
            exit(1); 
         }
      }

      while ((c=fgetc(fsrc))!=EOF)
         fputc(c,fdest);
      mclose(fdest);
      mclose(fsrc);
      exit(0);
   }
   return statusp;
}

/******************************************************************************/
/* SECURELY MOVE ONE FILE TO ANOTHER                                          */
/******************************************************************************/
int                     /* RETURNS: error code */
move_file(src,dest,sec) /* ARGUMENTS: */
   char *src;           /*    Source file */
   char *dest;          /*    Destination file */
   int   sec;           /*    As owner(0) or user(1)? */
{
   register int cpid,wpid;
   int statusp;

   int ret;

   if (sec==SL_OWNER) {
      if ((ret = rename(src, dest))) {
         error("renaming ", src);
         exit(1);
      }
      return ret;
   }

   fflush(stdout);
   if (status & S_PAGER) 
      fflush(st_glob.outp);

   cpid=fork();
   if (cpid) { /* parent */
      if (cpid<0) return -1; /* error: couldn't fork */
      while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);
   } else { /* child */
      signal(SIGINT,SIG_DFL);
      signal(SIGPIPE,SIG_DFL);
      close(0);

#ifdef HAVE_FSTAT
      if (confidx>=0)
         mclose(conffp);
#endif

      setuid(getuid());
      setgid(getgid());

      exit( rename(src, dest) );
   }
   if (statusp)
      error("renaming ", src);
   return statusp;
}

/******************************************************************************/
/* INVOKE EDITOR ON A FILE                                                    */
/******************************************************************************/
int                        /* RETURNS: (nothing)                          */
priv_edit(dir,file,fl)     /* ARGUMENTS:                                  */
   char *dir;              /*    Directory containing file                */
   char *file;             /*    Filename to edit                         */
   int   fl;               /*    Flags: 1=visual, 2=force                 */
{
   char buff3[MAX_LINE_LENGTH]; /* original file */
   char bufr[MAX_LINE_LENGTH];
   int  ret;

   if (file)
      sprintf(buff3,"%s/%s",dir,file);
   else
      strcpy(buff3,dir);

   /* Copy file to cf.buffer owned by user */
   sprintf(bufr,"%s/cf.buffer",work);
   copy_file(buff3,bufr,SL_USER); /* Assume readable by user? XXX */
 
   ret = edit(bufr,(char*)0, (fl & 1));
   if (!ret)
      printf("Aborting...\n");
   else {
      if ((fl&2) || get_yes("Ok to install this? ", DEFAULT_OFF))
         copy_file(bufr,buff3,SL_OWNER);
      rm(bufr,SL_USER);
   }
   return ret;
}

int                   /* RETURNS: (nothing)                          */
edit(dir,file,visual) /* ARGUMENTS:                                  */
   char *dir;         /*    Directory containing file                */
   char *file;        /*    Filename to edit                         */
   int   visual;      /*    Flag: visual editor?                     */
{
   char buff[MAX_LINE_LENGTH];
   char buff3[MAX_LINE_LENGTH]; /* original file */
   struct stat st;
   char *ed;

   if (file)
      sprintf(buff3,"%s/%s",dir,file);
   else
      strcpy(buff3,dir);
   
   ed = expand((visual)? "visual":"editor", DM_VAR);

   if (!strcmp(ed, "builtin")) {
      flags &= ~O_EDALWAYS;

      /* Copy file to user's cf.buffer */
      sprintf(buff,"%s/cf.buffer", work);
      if (strcmp(buff, buff3)) {
         copy_file(buff3, buff, SL_USER);
      }

      if (text_loop(0,"text")) {

         /* Now install cf.buffer contents in original filename */
         if (strcmp(buff, buff3)) {
            copy_file(buff, buff3, SL_USER);
            rm(buff, SL_USER);
         }
      }
   } else {
      int len = strlen(ed) + strlen(buff3) + 2;
      char *ptr = (char *)xalloc(0, len);
      sprintf(ptr,"%s %s", ed, buff3);
      unix_cmd(ptr);
      xfree_string(ptr);
   }
   return !stat(buff3,&st);
}

#ifdef HAVE_DBM_OPEN
#ifdef HAVE_NDBM_H
#include <ndbm.h>
#include <fcntl.h>
#endif

int /* RETURNS: 1 on success, 0 on failure */
ssave_dbm(userfile, keystr, valstr)
   char *userfile;
   char *keystr;
   char *valstr;
{
   datum dkey, dval;
   DBM  *db;

   db = dbm_open(userfile, O_RDWR|O_CREAT, 0644);
   if (!db)
      return 0;
   dkey.dptr  = keystr;
   dkey.dsize = strlen(keystr)+1;
   dval.dptr  = valstr;
   dval.dsize = strlen(valstr)+1;
   dbm_store(db, dkey, dval, DBM_REPLACE);
   dbm_close(db);
   return 1;
}

int /* RETURNS: 1 on success, 0 on failure */
save_dbm(userfile, keystr, valstr, suid)
   char *userfile;
   char *keystr;
   char *valstr;
   int   suid;
{

   if (suid==SL_OWNER) {
      return ssave_dbm(userfile, keystr, valstr);

   } else { /* suid==SL_USER */
      register int cpid,wpid;
      int statusp;

      fflush(stdout);
      if (status & S_PAGER) 
         fflush(st_glob.outp);
      cpid=fork();
      if (cpid) { /* parent */
         if (cpid<0) return -1; /* error: couldn't fork */
         while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);
      } else { /* child */                                         
         signal(SIGINT,SIG_DFL);
         signal(SIGPIPE,SIG_DFL);
         close(0);  

         setuid(getuid());    
         setgid(getgid());
         exit(!ssave_dbm(userfile, keystr, valstr));
      }
      return !statusp;
   }
}

void
dump_dbm(userfile)
   char *userfile;
{
   datum dkey, dval;
   DBM *db;

   db = dbm_open(userfile, O_RDONLY, 0644);
   if (!db)
      perror(userfile);

   for (dkey = dbm_firstkey(db); dkey.dptr != NULL; dkey = dbm_nextkey(db)) {
      dbm_fetch(db, dkey);
      dval = dbm_fetch(db, dkey);
      printf("%s: %s\n", dkey.dptr, dval.dptr);
   }

   dbm_close(db);
}

static char *
sget_dbm(userfile, keystr)
   char *userfile;
   char *keystr;
{
   datum dkey, dval;
   DBM *db;

   db = dbm_open(userfile, O_RDWR, 0644);
   if (!db)
      return "";
   dkey.dptr  = keystr;
   dkey.dsize = strlen(keystr)+1;
   dval = dbm_fetch(db, dkey);
   dbm_close(db);
   return (dval.dptr)? dval.dptr : "";
}

/* This function must use a pipe to send back the string */
char *
get_dbm(userfile, keystr, suid)
   char *userfile;
   char *keystr;
   int   suid; /*    As owner(0) or user(1)? */
{
   if (suid==SL_OWNER) {
      return sget_dbm(userfile, keystr);

   } else { /* suid==SL_USER */
      register int cpid,wpid;
      int statusp;
      int fd[2];
      static char buff[MAX_LINE_LENGTH];

      /* Open a pipe to use to pass string back through */
      if (pipe(fd)<0) 
         return NULL;

      /* Flush output to avoid duplication */
      fflush(stdout);
      if (status & S_PAGER) 
         fflush(st_glob.outp);

      cpid=fork();
      if (cpid) { /* parent */
         if (cpid<0) return ""; /* error: couldn't fork */
         close(fd[1]);
         while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);

         /* Retrieve the string now? or before the wait? */
         read(fd[0], buff, MAX_LINE_LENGTH);
         close(fd[0]);
         return buff;

      } else { /* child */                                         
         char *str;

         signal(SIGINT,SIG_DFL);
         signal(SIGPIPE,SIG_DFL);
         close(0);  
         close(fd[0]);

         setuid(getuid());    
         setgid(getgid());

         str = sget_dbm(userfile, keystr);
         write(fd[1], str, strlen(str)+1);
         close(fd[1]);
         exit(0);
      }
   }
}
#endif
