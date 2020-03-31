/* $Id: joq.c,v 1.14 1997/08/28 00:03:09 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for fork, etc */
#endif
#include "yapp.h"
#include "struct.h"
#include "lib.h"
#include "globals.h"
#include "joq.h"
#include "item.h"
#include "sum.h"
#include "xalloc.h"
#include "files.h"
#include "stats.h"
#include "conf.h" /* for leave() */
#include "misc.h" /* for misc_cmd_dispatch() */
#include "help.h" /* for help() */
#include "user.h" /* for partfile_perm() */
#include "system.h" /* for SL_OWNER */

/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE JOQ COMMAND FUNCTION                       */
/******************************************************************************/
char                        /* RETURNS: 0 on abort, 1 else */
joq_cmd_dispatch(argc,argv) /* ARGUMENTS:                  */
int    argc;                /*    Number of arguments      */
char **argv;                /*    Argument list            */
{
   char **ulst,file[MAX_LINE_LENGTH],buff[MAX_LINE_LENGTH];
   short  j;
   char **config;

   if (match(argv[0],"r_egister") 
    || match(argv[0],"j_oin")
    || match(argv[0],"p_articipate"))   { 
      if (confidx>=0) leave(0,(char**)0);
      write_part((char *)NULL);
      st_new.c_status   |= CS_JUSTJOINED; 
      mode=M_OK; 

      /* Unless ulist file is referenced in an acl, add login to ulist */
      if ((config = get_config(joinidx)) != NULL) {

         if (is_auto_ulist(joinidx)) {
            if (!is_inlistfile(joinidx, "ulist")) {
               sprintf(file,"%s/%s",conflist[joinidx].location,"ulist");
               sprintf(buff,"%s\n",login);
               write_file(file,buff);
             
               custom_log("newjoin", M_OK);
            }

#if 0
            sprintf(file,"%s/%s",conflist[joinidx].location,"ulist");
            sprintf(buff,"%s\n",login);
            if (!(ulst=grab_file(conflist[joinidx].location,"ulist",
             GF_SILENT|GF_WORD|GF_IGNCMT)))
               write_file(file,buff);
            else {
               for (j=xsizeof(ulst)-1; 
                    j>=0 && strcmp(ulst[j],login) && uid!=atoi(ulst[j]); 
                    j--);
               if (j<0)

                  write_file(file,buff);
               xfree_array(ulst);
            }
#endif
         }
      }

   } else if (match(argv[0],"o_bserver")) { 
      sumentry_t sum2[MAX_ITEMS];
      short i;

      if (confidx>=0) leave(0,(char**)0);
      st_new.c_status |= (CS_OBSERVER|CS_JUSTJOINED); 
      mode=M_OK; 

      /* Initialize part[] */
      for (i=0; i<MAX_ITEMS; i++) {
         part[i].nr=part[i].last=0;
         dirty_part(i);
      }
      get_status(&st_new,sum2,part,joinidx);
      st_new.sumtime = 0;
      for (i=st_new.i_first+1; i<st_new.i_last; i++) 
         time(&(part[i-1].last));
   } else if (match(argv[0],"h_elp"))     help(argc,argv); 
   else if (match(argv[0],"q_uit"))     { status |= S_QUIT; mode=M_OK; }
   else return misc_cmd_dispatch(argc,argv);
   return 1;
}

static void
write_part2(buff, stt, sum3)
   char       *buff;
   status_t   *stt;
   sumentry_t *sum3;
{
   FILE        *fp;
   register int i;
   char buff2[MAX_LINE_LENGTH];

   if (debug & DB_PART) {
      printf("after split: Partfile=%s\n",buff);
      fflush(stdout);
   }

   if (debug & DB_PART) 
      printf("file %s uid %d euid %d\n", buff, getuid(), geteuid());

   /* KKK*** in the future, allocate string array of #items+2,
      save lines in there, call dump_file, and free the array */
   sprintf(buff2, "%s.new", buff);
   errno=0;
   if ((fp=mopen(buff2,O_W))==NULL) /* "w" */
      exit(1);
   if (debug & DB_PART) 
      printf("open succeeded\n");
   fprintf(fp,"!<pr03>\n%s\n",stt->fullname);

   if (debug & DB_PART) 
      printf("first %d last %d\n",stt->i_first, stt->i_last);
   for (i=stt->i_first; i<=stt->i_last; i++) {
      if (debug & DB_PART) 
         printf("sum3[%d]=%d ",i-1,sum3[i-1].nr);
      if (sum3[i-1].nr || part[i-1].last) {
         fprintf(fp,"%d %d %X\n",i,part[i-1].nr,part[i-1].last);
         if (debug & DB_PART) 
            printf(": %d %d %X",i,part[i-1].nr,part[i-1].last);
         fflush(fp);
      }
      if (debug & DB_PART) 
         printf("\n");
   }
   mclose(fp);

   /* Now atomically replace the old participation file */
   if (!errno) 
      rename(buff2, buff);
   else {
      error("writing ", buff);
      unlink(buff2);
   }
}

/******************************************************************************/
/* WRITE OUT A USER PARTICIPATION FILE FOR THE CURRENT CONFERENCE             */
/******************************************************************************/
void                 /* RETURNS: (nothing) */
write_part(partfile) /* ARGUMENTS:         */
char *partfile;      /*    Filename        */
{
   char       buff[MAX_LINE_LENGTH],*file;
   short      i,cpid,wpid;
   status_t   *stt;
   sumentry_t sum2[MAX_ITEMS],*sum3;

   if (st_glob.c_status & CS_OBSERVER) 
      return; 
 
   if (partfile) {
      file=partfile;
      sum3 = sum;
      stt  = &st_glob;
   } else {
      char **config;

      if (!(config = get_config(joinidx)))
         return;
      file = config[CF_PARTFILE];
      sum3 = sum2;
      stt  = &st_new;

      /* Initialize part[] */
      for (i=0; i<MAX_ITEMS; i++) {
         part[i].nr=part[i].last=0;
         dirty_part(i);
      }
      get_status(stt,sum2,part,joinidx);
      if (flags & O_UNSEEN) {
         for (i=st_new.i_first+1; i<st_new.i_last; i++)
            time(&(part[i-1].last));
      }
      stt->sumtime = 0;
   }

   /* Create WORK/.name.cf */
   sprintf(buff,"%s/%s",partdir,file);
   if (debug & DB_PART) {
      printf("before split: Partfile=%s\n",buff);
      fflush(stdout);
   }

   if (partfile_perm()==SL_OWNER) {
      write_part2(buff, stt, sum3);
      return;
   }

   /* FORK */
   fflush(stdout);
   if (status & S_PAGER)
      fflush(st_glob.outp);

   cpid=fork();
   if (cpid) { /* parent */
      if (cpid<0) return; /* error: couldn't fork */
      while ((wpid = wait((int *)0)) != cpid && wpid != -1);
   } else { /* child */
      signal(SIGINT,SIG_DFL);
      signal(SIGPIPE,SIG_DFL);
      close(0); /* make sure we don't touch stdin */

      setuid(getuid());
      setgid(getgid());

      write_part2(buff, stt, sum3);
      exit(0);
   } /* ENDFORK */

   if (debug & DB_PART) 
      printf("write_part: fullname=%s\n",st_glob.fullname);
}

/******************************************************************************/
/* READ IN A USER PARTICIPATION FILE FOR SOME CONFERENCE                      */
/******************************************************************************/
char                             /* RETURNS: (nothing)  */
read_part(partfile,part,stt,idx) /* ARGUMENTS:          */
   char        *partfile;        /*    Filename         */
   partentry_t  part[MAX_ITEMS]; /*    Array to fill in */
   status_t    *stt;
   short        idx;
{
   char **partf,buff[MAX_LINE_LENGTH];
   short sz,i,a,b;
   long d;
   struct stat st;
   sumentry_t sum2[MAX_ITEMS];

   for (i=0; i<MAX_ITEMS; i++) {
      part[i].nr=part[i].last=0;
      dirty_part(i);
   }
   strcpy(stt->fullname,st_glob.fullname);
   if (!(partf=grab_file(partdir,partfile,GF_SILENT))) {

      /* Newly joined, Initialize part[] */
      for (i=0; i<MAX_ITEMS; i++) 
         part[i].nr = part[i].last = 0;
      get_status(stt,sum2,part,idx);
      stt->sumtime = 0;
      for (i=stt->i_first+1; i<stt->i_last; i++) 
         time(&(part[i-1].last));

      return 0;
   }
   sz=xsizeof(partf);
   if (!sz || strcmp(partf[0],"!<pr03>"))
       printf("Invalid participation file format.\n");
   else if (sz>1)
       strcpy(stt->fullname,partf[1]);
   for (i=2; i<xsizeof(partf); i++) {
      sscanf(partf[i],"%hd %hd %lx",&a,&b,&d);
      if (a>=1 && a<=MAX_ITEMS) {
         part[a-1].nr   = b;
         part[a-1].last = d;
      }
   }
   xfree_array(partf);
   
   sprintf(buff,"%s/%s",partdir,partfile);
   if (!stat(buff,&st) && st.st_size>0)
      stt->parttime = st.st_mtime;
 
   if (debug & DB_PART) 
      printf("read_part: fullname=%s\n",st_glob.fullname);
   return 1;
}
