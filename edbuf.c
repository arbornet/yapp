/* $Id: edbuf.c,v 1.8 1996/12/19 03:59:09 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for getpid */
#endif
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "macro.h"
#include "driver.h"
#include "lib.h"
#include "item.h"
#include "edbuf.h"
#include "help.h"
#include "files.h"
#include "system.h"
#include "main.h" /* for wputs */
#include "misc.h" /* for misc_cmd_dispatch */

static FILE *file;
static char post;
static char oldmode;
static int  resp;

/* Regarding mode: NOW USES oldmode
 * Set mode back to RFP for respond command
 * Enter command will set mode to OK itself
 */

/******************************************************************************/
/* MAIN TEXT ENTRY LOOP                                                       */
/******************************************************************************/
char              /* RETURNS: 1 on success, 0 on failure */
cfbufr_open(flg)  /* ARGUMENTS: */
short flg;        /*    File open type (r,w, etc) */
{
   char buff[MAX_LINE_LENGTH];

   sprintf(buff,"%s/cf.buffer",work);
   if ((file=smopenw(buff,flg))==NULL) return 0;
   if (!(flags & O_QUIET))
      printf("Type \".\" to exit or \":help\".\n");
   return 1;
}

/******************************************************************************/
char                  /* RETURNS: 1 on post, 0 on abort */
text_loop(new, label) /* ARGUMENTS: (none) */
   int   new;    /* True if we should start from scratch, false if modifying */
   char *label;  /* What to ask for ("text", "response", etc) */
{
   char ok=1,inbuff[MAX_LINE_LENGTH],
        fromname[MAX_LINE_LENGTH],toname[MAX_LINE_LENGTH];
   struct stat st;

   if (flags & O_EDALWAYS)
      return edit(work,"cf.buffer",0);

   post=0;

   if (new) {
      sprintf(fromname,"%s/cf.buffer",work);
      if (!stat( fromname ,&st)) { /* cf.buffer exists */
         sprintf(toname,"%s/cbf.%d",work,getpid());
         if (copy_file(fromname,toname, SL_USER))
            error("renaming cf.buffer to ",toname);
      }
   } 

   if (!cfbufr_open((new)? O_WPLUS : O_APLUS)) return 0; /* "w+" */
   if (!(flags & O_QUIET)) {
      if (!new)
         text_print(0,NULL);
      printf("%s your %s%s\n",(new) ? "Enter"    : "(Continue", label,
                              (new) ? ":"        : " entry)" );
   }

   oldmode = mode;
   mode = M_TEXT;
   while (mode==M_TEXT && ok) {
      /* For optimization purposes, we do not allow seps in TEXT mode
       * prompt.  This could be changed back if confsep would dump
       * out most strings quickly without accessing the disk.
       */
      if (!(flags & O_QUIET))
         wputs(TEXT);
      /* print_prompt(mode); */

      ok = (ngets(inbuff, st_glob.inp)!=NULL); /* st_glob.inp */
      if (ok && (status & S_INT)) {
         char str[80];
         status &= ~S_INT; /* Clear interrupt */
         sprintf(str, "Abort %s? ", label);
         ok = !get_yes(str, DEFAULT_ON);
         if (!ok) post= -1;
      }
      /* printf("ok=%d inbuff=%s\n",ok,inbuff); */
      if (!ok) {
         printf("\n");
         mode = oldmode; /* Ctrl-D same as "." */
         post++; /* post on ^D or .  don't post on ^C */
      } else if (inbuff[0]==':') {
         if (inbuff[1]) ok = command(inbuff+1,0);
         else           ok = command("e",0);
      } else if ((flags & O_DOT) && !strcmp(inbuff,".")) {
         mode = oldmode; /* done */
         post = 1;
      } else { /* Add to file buffer */
         if (fprintf(file,"%s\n",inbuff)<0) ok=0;
         else fflush(file);
      }
   }
   smclose(file);
/*printf("File has been closed\n");*/
   return post;
}

/* Commands available while in text entry mode */
static dispatch_t text_cmd[]={
 { "q_uit",    text_abort, },
 { "c_ommand", text_done, },
 { "ok",       text_done, },
 { "p_rint",   text_print, },
 { "e_dit",    text_edit, },
 { "v_isual",  text_edit, },
 { "h_elp",    help, },
 { "?",        help, },
 { "cl_ear",   text_clear, },
 { "em_pty",   text_clear, },
 { "r_ead",    text_read, },
 { "w_rite",   text_write, },
 { 0, 0 },
};

/******************************************************************************/
/* DISPATCH CONTROL TO APPROPRIATE TEXT COMMAND FUNCTION                      */
/******************************************************************************/
char                         /* RETURNS: 1 on abort, 0 else */
text_cmd_dispatch(argc,argv) /* ARGUMENTS:                  */
int    argc;                 /*    Number of arguments      */
char **argv;                 /*    Argument list            */
{
   int i;

   for (i=0; text_cmd[i].name; i++)
      if (match(argv[0],text_cmd[i].name))
         return text_cmd[i].func(argc,argv);

   /* Command dispatch */
   if (match(argv[0],"d_one") /* same as . on a new line */
    ||      match(argv[0],"st_op") /* ? */
    ||      match(argv[0],"ex_it"))/* ?*/{
      mode = oldmode; post = 1; /* mark as done */ 
   } else {
      printf("Don't understand that!\n\n");
      text_abort(argc,argv);
   }
   return 1;
}

/******************************************************************************/
/* READ TEXT FROM A FILE INTO THE BUFFER                                      */
/******************************************************************************/
int                  /* RETURNS: (nothing)     */
text_read(argc,argv) /* ARGUMENTS:             */
int argc;            /*    Number of arguments */
char **argv;         /*    Argument list       */
{
   FILE *fp;
   char buff[MAX_LINE_LENGTH];

   /* PicoSpan puts spaces into the filename, we don't */
   if (argc!=2) {
      printf("Syntax: r filename\n");
      return 1;
   }

   /* This is done inside the secure portion
    * writing to the cf.buffer file, so it's already secure
    */
   if (!(flags & O_QUIET))
      printf("Reading %s\n",argv[1]);
   if ((fp=mopen(argv[1],O_R))==NULL) {
      return 1;
   }
   while (ngets(buff,fp))
      fprintf(file,"%s\n",buff);
   mclose(fp);
   return 1;
}

/******************************************************************************/
/* WRITE TEXT IN BUFFER OUT TO A FILE                                         */
/******************************************************************************/
int                  /* RETURNS: (nothing)     */
text_write(argc,argv) /* ARGUMENTS:             */
int argc;             /*    Number of arguments */
char **argv;          /*    Argument list       */
{
   FILE *fp;
   char buff[MAX_LINE_LENGTH];
   char filename[MAX_LINE_LENGTH];

   /* PicoSpan puts spaces into the filename, we don't */
   if (argc!=2) {
      printf("Syntax: w filename\n");
      return 1;
   }

   /* This is done inside the secure portion
    * writing to the cf.buffer file, so is already secure
    */
   printf("Writing %s\n",argv[1]);
   if ((fp=mopen(argv[1],O_W))==NULL) { /* use normal umask */
      return 1;
   }

/* 
   fseek(file,0L,0);
*/
   smclose(file);
   sprintf(filename,"%s/cf.buffer",work);
   if ((file=smopenr(filename, O_R))==NULL) return 0;

   while (ngets(buff,file))
      fprintf(fp,"%s\n",buff);
   mclose(fp);

   smclose(file);
   if ((file=smopenw(filename, O_APLUS))==NULL) return 0;

   return 1;
}

/******************************************************************************/
/* DUMP TEXT IN BUFFER AND START OVER                                         */
/******************************************************************************/
int                   /* RETURNS: (nothing)     */
text_clear(argc,argv) /* ARGUMENTS:             */
int argc;             /*    Number of arguments */
char **argv;          /*    Argument list       */
{
   mclose(file);
   if (!(flags & O_QUIET))
      printf("Enter your %s:\n",(oldmode==M_OK)? "text" : "response" );
   if (!cfbufr_open(O_WPLUS)) /* "w+" */
      mode = oldmode; /* abort */
   else 
      mode = M_TEXT;
   return 1;
}

/******************************************************************************/
/* REPRINT CURRENT CONTENTS OF BUFFER                                         */
/******************************************************************************/
int                   /* RETURNS: (nothing)     */
text_print(argc,argv) /* ARGUMENTS:             */
int argc;             /*    Number of arguments */
char **argv;          /*    Argument list       */
{
   char filename[MAX_LINE_LENGTH];
   char buff[MAX_LINE_LENGTH];
/* XXX */

   smclose(file);

   sprintf(filename,"%s/cf.buffer",work);
   if ((file=smopenr(filename, O_R))==NULL) return 0;

/*
   fseek(file,0L,0);
*/
   while (ngets(buff,file) && !(status & S_INT))
      printf(" %s\n",buff);
   smclose(file);

   if ((file=smopenw(filename, O_APLUS))==NULL) return 0;
   return 1;
}

/******************************************************************************/
/* INVOKE UNIX EDITOR ON THE BUFFER                                           */
/******************************************************************************/
int                  /* RETURNS: (nothing)     */
text_edit(argc,argv) /* ARGUMENTS:             */
int argc;            /*    Number of arguments */
char **argv;         /*    Argument list       */
{
   int visual = (argv[0][0]=='v');/* 'v_isual' check */
   if (mode==M_TEXT 
    && !strcmp(expand((visual)? "visual":"editor", DM_VAR), "builtin")) {
      if (!(flags & O_QUIET))
         printf("Already in builtin editor.\n");
   } else {
      mclose(file);
      edit(work,"cf.buffer",visual); 
      printf("(Continue your %s entry)\n",(resp)? "response" : "text" );
      cfbufr_open(O_APLUS); /* a+ */
   }
   return 1;
}

/******************************************************************************/
/* ABORT TEXT ENTRY MODE                                                      */
/******************************************************************************/
int                   /* RETURNS: (nothing)     */
text_abort(argc,argv) /* ARGUMENTS:             */
int argc;             /*    Number of arguments */
char **argv;          /*    Argument list       */
{
   if (get_yes("Ok to abandon text? ", DEFAULT_ON)) 
      mode = oldmode;
   return 1;
}

/******************************************************************************/
/* END TEXT ENTRY MODE AND POST IT                                            */
/******************************************************************************/
int                  /* RETURNS: (nothing)     */
text_done(argc,argv) /* ARGUMENTS:             */
int argc;            /*    Number of arguments */
char **argv;         /*    Argument list       */
{
   /* Main EDB cmd loop */
   mode = M_EDB;
   while (mode==M_EDB && get_command(NULL, 0));
   return 1;
}

/******************************************************************************/
/* FIGURE OUT WHAT TO DO WHEN ESCAPING OUT OF TEXT MODE                       */
/******************************************************************************/
char             
edb_cmd_dispatch(argc,argv) /* ARGUMENTS:             */
int argc;                   /*    Number of arguments */
char **argv;                /*    Argument list       */
{
   /* Command dispatch */
   if      (match(argv[0],"n_on")
    ||      match(argv[0],"nop_e")) {
      /* printf("Response aborted!  Returning to current %s.\n", topic(0));*/
      mode = oldmode;
   } else if (match(argv[0],"y_es")
    ||        match(argv[0],"ok"))  { post = 1; mode = oldmode; }
   else if (match(argv[0],"ed_it"))         text_edit(argc,argv);
   else if (match(argv[0],"ag_ain")
    ||      match(argv[0],"c_ontinue"))     {
      mode = M_TEXT;
      if (!(flags & O_QUIET))
         printf("(Continue your text entry)\nType \".\" to exit or \":help\".\n");
   } else if (match(argv[0],"pr_int"))      text_print(argc,argv);
   else if (match(argv[0],"em_pty")
    ||      match(argv[0],"cl_ear"))        text_clear(argc,argv);
   else return misc_cmd_dispatch(argc,argv);
   return 1;
}
