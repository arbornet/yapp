/* $Id: help.c,v 1.5 1996/02/16 03:23:10 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include "yapp.h"
#include "struct.h"
#include "lib.h"
#include "globals.h"
#include "conf.h"   /* for get_idx */
#include "xalloc.h" /* for xfree_array */

/* Index files of help text, in order by mode */
#define HELPINDEX  "Index"

void
show_help(count,argc,argv,file,hdr)
int   *count;   /*    Number of arguments */
int    argc;
char **argv;    /*    Argument list       */
char  *file;/*    Filename of list    */
char  *hdr;     /*    Help display header */
{
   assoc_t *helplist;
   int      helpsize,idx=0,j;
   char *dir,*fil;
   char **header,name[MAX_LINE_LENGTH];

   /* Set location */
   if (file[0]=='%') {
      dir = bbsdir;
      fil = file+1;
   } else {
      dir = helpdir;
      fil = file;
   }

   /* Is this a text file or a list? */
   if (!(header = grab_file(dir,fil,GF_HEADER))) return;
   if (strcmp(header[0],"!<hl01>")) {
      if (*count < argc) 
         printf("Sorry, only this message available.\n");
      else if (hdr)
         printf("****    %s    ****\n",hdr);
      if (!more(dir,fil))
         printf("Can't find help file %s/%s.\n",dir,fil);
      xfree_array(header);
      return;
   }
   xfree_array(header);

   /* Read in help list */
   if ((helplist=grab_list(dir,fil,0))==NULL)
      return;
   helpsize = xsizeof(helplist)/sizeof(assoc_t);

   /* Display requested file */
   if (*count>=argc) {

      /* No arguments, use default file */
      show_help(count,argc,argv,helplist[0].location,hdr);

   } else { 

      idx=get_idx(argv[*count],helplist,helpsize);
      (*count)++;
      if (idx<0) 
         printf("Sorry, no help available for \"%s\"\n",argv[(*count)-1]);

      /* %filename indicates file is in bbsdir not helpdir */
      else if (helplist[idx].location[0]=='%') {
         show_help(count,argc,argv,helplist[idx].location,hdr);

      /* normal help files are in helpdir & get a header displayed */
      } else {
         char buff[MAX_LINE_LENGTH];

         strcpy(name,compress(helplist[idx].name));
         for (j=strlen(name)-1; j>=0; j--)
            name[j]=toupper(name[j]);
         if (hdr)
            sprintf(buff,"%s %s",hdr,name);
         else
            strcpy(buff,name);
         show_help(count,argc,argv,helplist[idx].location,buff);
      }
   }

   /* Free the list */
   free_list(helplist);
}

/*****************************************************************************/
/* GET HELP ON SOME TOPIC                                                    */
/*****************************************************************************/
int             /* RETURNS: (nothing)     */
help(argc,argv) /* ARGUMENTS:             */
int    argc;    /*    Number of arguments */
char **argv;    /*    Argument list       */
{
   char **helpfile;
   int    count=1;

   if (!(helpfile = grab_file(helpdir,HELPINDEX,0))) return 1;
   do {
      show_help(&count,argc,argv,helpfile[mode],(char*)0);
   } while (count<argc);
   xfree_array(helpfile);
   return 1;
}
