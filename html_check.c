/* $Id: html_check.c,v 1.6 1997/06/17 01:03:09 thaler Exp $ */
/* HTML sanity filter
 * Phase 1: map < > & " to escape sequences
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif

#define STACKSIZE 1000
int stack[STACKSIZE];
int top=0;

/* Illegal case-insensitive strings */
char *illegal[]={
   "HTML", "BODY", "HEAD", "TITLE", "ADDRESS", "ISINDEX", 
   NULL
};

char *matching[]={
   "H1", "H2", "H3", "H4", "H5", "H6", 
   "A",  "UL", "OL", "DL", "PRE", "BLOCKQUOTE",
   "DFN", "EM", "CITE", "CODE", "KBD", "SAMP",
   "STRONG", "VAR", "B", "I", "TT", "BLINK",
   "FORM", "SELECT", "TEXTAREA", "SUP", "SUB",
   "DIV", "FONT", "CENTER", "NOFRAMES", "FRAMESET",
   "TABLE", "TD", "TR",
   NULL
};

char *Umatching[100];
char *Uillegal[100];

void
push(i)
   int i;
{
   if (top==STACKSIZE) {
      printf("Stack overflow\n");
      exit(1);
   }
   stack[top++] = i;
}

int
pop()
{
   if (top==0) 
      return -1;
   top--;
   return stack[top];
}

/*
 * Load custom tags into malloc'ed space pointed to by elements of
 * Uillegal and Umatching arrays, and make sure those arrays
 * are NULL terminated
 */
int
load_illegal_tags(file)
   char *file;
{
   FILE *fp;
   char buff[80];
   int i;

   fp = fopen(file, "r");
   if (!fp) 
      return 0; /* use non-custom tags */

   /* Load illegal tags first */
   i = 0;
   while (fscanf(fp, "%s", buff)==1) {
      if (!strcmp(buff, "|"))
         break;
      Uillegal[i] = malloc(strlen(buff)+1); /* "strdup" */
      strcpy(Uillegal[i], buff);
      i++;
   }
   Uillegal[i] = NULL;

   fclose(fp);
   return 1;
}

int
load_matched_tags(file)
   char *file;
{
   FILE *fp;
   char buff[80];
   int i;

   fp = fopen(file, "r");
   if (!fp) 
      return 0; /* use non-custom tags */

   /* Now load matching tags */
   i = 0;
   while (fscanf(fp, "%s", buff)==1) {
      Umatching[i] = malloc(strlen(buff)+1); /* "strdup" */
      strcpy(Umatching[i], buff);
      i++;
   }
   Umatching[i] = NULL;
   
   fclose(fp);
   return 1;
}

void
usage()
{
   fprintf(stderr, "Yapp %s (c)1996 Armidale Software\n usage: html_check [-h] [-m file] [-i file]\n", VERSION);
   fprintf(stderr, " -h       Allow HTML header as legal\n");
   fprintf(stderr, " -i file  Use tags in file as those which are illegal\n");
   fprintf(stderr, " -m file  Use tags in file as those which must be matched\n");
   exit(1);
}

/* Also match quotes ("...") inside < > */

int
main(argc, argv)
   int argc;
   char **argv;
{
   FILE *fp;
   char buff[1024], *p;
   char *tag;
   int html=0, undo, quot=0;
   int i;
   int allow_header=0, illegal_tags = 0, matched_tags = 0;
   char *options="hvi:m:";    /* Command-line options */
   extern char *optarg;
   extern int optind,opterr;

   while ((i = getopt(argc, argv, options)) != -1) {
      switch(i) {
         case 'h': allow_header = 1; break;
         case 'm': matched_tags = load_matched_tags(optarg); break;
         case 'i': illegal_tags = load_illegal_tags(optarg); break;
         case 'v':
         default:  usage();
      }
   }
   argc -= optind;
   argv += optind;

   if (argc>0) {
      if ((fp = fopen(argv[0], "r"))==NULL) {
         fprintf(stderr, "Can't open %s\n", argv[0]);
         exit(1);
      }
   } else
      fp = stdin;

   while (fgets(buff, sizeof(buff), fp)!=NULL) {
      for (p=buff; *p; ) {

         /* Match < > */
         if (*p=='<') {
            if (html) {
               printf("Found \"<\" inside HTML tag in \"%s\".\n", buff);
               printf("Use \"&lt;\" instead of \"<\" if you want a less-than sign to appear.\n");
               exit(1);
            }
            html++;
            quot=0;

            p++;
            if (*p == '/') {
               undo = 1;
               p++;
            } else
               undo = 0;

            /* Compare to list of illegal tags */
            if (!allow_header) {
               i=0;
               tag=(illegal_tags)? Uillegal[i] : illegal[i];
               while (tag) {
                  if (!strncasecmp(p, tag, strlen(tag))
                   && !isalnum(p[ strlen(tag) ]) ) {
                     printf("Illegal HTML tag found: %s\n", tag);
                     exit(1);
                  }
                  i++;
                  tag=(illegal_tags)? Uillegal[i] : illegal[i];
               }
            }

            /* Compare to list of matching tags */
            i = 0;
            tag=(matched_tags)? Umatching[i] : matching[i];
            while (tag) {
               if (!strncasecmp(p, tag, strlen(tag))
                && !isalnum(p[ strlen(tag) ]) ) {

                  /* Disallow overlapping tags like B I /B /I 
                   * But allow H1 A /A H1 
                   */
                  if (undo) {
                     int j = pop();
                     if (j<0) {
                        printf("closing %s tag found without opening tag\n",
                         tag);
                        exit(1);
                     }
                     if (i != j) {
                        printf("tags %s and %s overlap in \"%s\"\n", tag, 
                         ((matched_tags)? Umatching[j] : matching[j]), buff);
                        exit(1);
                     }
                  } else {
                     push(i);
                  }
                  break;
               }
               i++;
               tag=(matched_tags)? Umatching[i] : matching[i];
            }

         } else if (*p=='>') {
            if (!html) {
               printf("\">\" appears outside an HTML tag in \"%s\".\n", buff);
               printf("Use \"&gt;\" instead of \">\" if you want a greater-than sign to appear.\n");
               exit(1);
            }
            html--;
            if (quot) {
               printf("Missing end quote in HTML tag in \"%s\".\n", buff);
               exit(1);
            }
            p++;
         } else if (*p=='"' && html) {
            quot=1-quot;
            p++;
         } else
            p++;
      }
   }

   /* Make sure not still in tag */
   if (html) {
      printf("Missing \">\" at end of HTML tag.\n");
      printf("Use \"&lt;\" instead of \"<\" if you want a less-than sign to appear.\n");
      exit(1);
   }

   /* Make sure stack is empty */
   if (top) {
      i = pop();
      printf("Missing ending %s tag\n",
       (matched_tags)? Umatching[i] : matching[i]);
      exit(1);
   }

   /* Ok, no problems found */
   exit(0);
}
