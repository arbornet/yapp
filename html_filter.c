/* $Id: html_filter.c,v 1.5 1997/06/17 01:03:10 thaler Exp $ */
/* HTML sanity filter
 * Phase 1: map < > & " to escape sequences
 * with -c: map \n to newline
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

void
usage()
{
   fprintf(stderr, "Yapp %s (c)1996 Armidale Software\n usage: html_filter [-cn]\n", VERSION);
   fprintf(stderr, " -c   Map \\n to newline\n");
   fprintf(stderr, " -n   Don't output newlines\n");
   exit(1);
}

int
main(argc,argv)
   int    argc;
   char **argv;
{
   int c;
   int newline = 1,  /* pass newlines through */
       back    = 0, 
       convert = 0;  /* convert \n to newline */

   char *options="hvnc";    /* Command-line options */
   extern int optind;

   while ((c = getopt(argc, argv, options)) != -1) {
      switch(c) { 
         case 'n': newline=0;  break;
         case 'c': convert=1;  break;
         case 'v':  
         case 'h':  
         default:  usage();
      }
   }
   argc -= optind;
   argv += optind;

   while ((c=getchar())!=EOF) {
      if (convert) {
         if (back) {
            if (c=='n')
               c='\n';
            back = 0;
         } else if (c=='\\') {
            back = 1;
            continue;
         }
      }

      switch(c) {
      case '>': printf("&gt;"); break;
      case '<': printf("&lt;"); break;
      case '&': printf("&amp;"); break;
      case '"': printf("&quot;"); break;
      case '\n': if (newline)
                    putchar(c);
                break;
      default : putchar(c);
      }
   }
   exit(0);
}
