/* $Id: main.c,v 1.6 1996/12/19 03:54:17 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h> /* for strcat */
#include "yapp.h"
#include "driver.h" /* for endbbs */

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    main
Called by:   (user)
Arguments:   command line arguments
Returns:     (nothing)
Calls:       init to set up global variables
             join to start up first conference
             command to process user commands
Description: This function parses the command line arguments,
             and acts as the main driver.
*******************************************************************************/
void                        /* RETURNS: (nothing)                  */
main(argc, argv)            /* ARGUMENTS:                          */
int argc;                   /*    Number of command line arguments */
char **argv;                /*    Array of command line arguments  */
{
     if (!strncmp(argv[0]+strlen(argv[0])-9, "yappdebug", 9)) {
        printf("Content-type: text/plain\n\nSTART OF OUTPUT:\n");
        fflush(stdout);
     }
     init(argc,argv); /* set up globals */
   
     while (get_command(NULL, 0));
     endbbs(0);
}

/******************************************************************************/
/* The following output routines simply call the standard output routines.    */
/* In the Motif version, the w-output routines send output to the windows     */
/******************************************************************************/
void
wputs(s)
char *s;
{
   fputs(s,stdout); /* NO newline like puts() does */
}

extern char evalbuf[MAX_LINE_LENGTH];
/* WARNING: the caller is responsible for doing an fflush on the stream
 *          when finished with calls to wfputs and wfputc 
 */
void
wfputs(s,stream)
   char *s;
   FILE *stream;
{
   if (stream)
      fputs(s,stream);
   else {
      strncat(evalbuf,s,MAX_LINE_LENGTH-strlen(evalbuf)-1);
      evalbuf[MAX_LINE_LENGTH - 1] = '\0';
   }
}

void
wputchar(c)
char c;
{  
   putchar(c);
}

/* WARNING: the caller is responsible for doing an fflush on the stream
 *          when finished with calls to wfputs and wfputc 
 */
void
wfputc(c,fp)
   char c;
   FILE *fp;
{
   char s[2];

   if (fp)
      fputc(c,fp);
   else {
      s[0]=c; s[1]='\0';
      strncat(evalbuf,s,MAX_LINE_LENGTH-strlen(evalbuf)-1);
      evalbuf[MAX_LINE_LENGTH - 1] = '\0';
   }
}

void
wgets(a,b)
   char *a;
   char *b;
{
}
