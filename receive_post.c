/* $Id: receive_post.c,v 1.6 1997/06/21 01:46:21 thaler Exp $ */
/*
 * Receive the POST infomation.
 * Output the ticket to standard out
 * Save the body in the cf.buffer in the user's directory 
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#else
char *getenv();
#endif

/* #define LOG "/tmp/postlog" */
#undef LOG
#define MAX_ENTRIES 10000

typedef struct {
    char *name;
    char *val;
} entry;

char *makeword(char *line, char stop);
char *fmakeword(FILE *f, char stop, int *len);
char x2c(char *what);
void unescape_url(char *url);
void plustospace(char *str);

void 
ftextout(fp,buff)
FILE *fp;       /* File pointer for output */
char *buff;     /* string to output */
{
   char *q = buff;
   while (*q) {
      /* Remove the Control M's from the post */
      if (*q=='\r') {
         q++;
         continue;
      }
      fputc(*q,fp);
      *q++;
   }
   fputc('\n',fp);

}


void
usage()
{
   fprintf(stderr, "Yapp %s (c)1996 Armidale Software\n usage: receive_post [-p pseudofile] [[-s] subjfile]\n", VERSION);
   fprintf(stderr, " -p pseudofile  Save pseudonym (if any) to indicated file\n");
   fprintf(stderr, " -s subjfile    Save subject (if any) to indicated file\n");
   exit(1);
}


int
main(argc,argv) 
   int    argc;
   char **argv;
{
#ifdef LOG
   FILE *fp;
#endif
   entry entries[MAX_ENTRIES];
   register int x, m=-1, i;
   int cl;
   char *env = getenv("REQUEST_METHOD");
   char *subjfile   = NULL;
   char *pseudofile = NULL;
   char *options="hvs:p:";    /* Command-line options */
   extern char *optarg;
   extern int optind,opterr;

   while ((i = getopt(argc, argv, options)) != -1) {
      switch(i) {
         case 's': subjfile  =optarg; break;
         case 'p': pseudofile=optarg; break;
         case 'v':
         case 'h':
         default:  usage();
      }
   }
   argc -= optind;
   argv += optind;
   if (argc==1 && !subjfile) {
      subjfile = argv[0];
      argc--;
      argv++;
   }
   if (argc>0)
      usage();

#ifdef LOG
   fp = fopen("/tmp/postlog", "a");
   if (!fp) exit(1);
   fprintf(fp, "receive_post began\n"); fflush(fp);
   fprintf(fp, "method=%s\n", env); fflush(fp);
#endif
    
    if(!env || strcmp(env,"POST")) {
        printf("This script should be referenced with a METHOD of POST.\n");
        printf("If you don't understand this, see this ");
        printf("<A HREF=\"http://www.ncsa.uiuc.edu/SDG/Software/Mosaic/Docs/fill-out-forms/overview.html\">forms overview</A>.%c",10);
        exit(1);
    }

#ifdef LOG
    fprintf(fp, "got past POST check\n"); fflush(fp);
#endif

    env = getenv("CONTENT_TYPE");
    if(!env || strcmp(env,"application/x-www-form-urlencoded")) {
        printf("This script can only be used to decode form results. \n");
        exit(1);
    }

#ifdef LOG
    fprintf(fp, "got past content type\n"); fflush(fp);
#endif

    env = getenv("CONTENT_LENGTH");
    if (!env)
       cl = 0;
    else
       cl = atoi(env);

#ifdef LOG
    fprintf(fp, "length=%d\n", cl); fflush(fp);
#endif

    for(x=0;cl && (!feof(stdin));x++) {
        m=x;
        entries[x].val = fmakeword(stdin,'&',&cl);
        plustospace(entries[x].val);
        unescape_url(entries[x].val);
        entries[x].name = makeword(entries[x].val,'=');
    }

#ifdef LOG
    fprintf(fp, "lines=%d\n", m); fflush(fp);
#endif

    for(x=0; x <= m; x++) {
        if (!strcmp(entries[x].name, "ticket")
         || !strcmp(entries[x].name, "tkt")) {
           printf("%s", entries[x].val);
#ifdef LOG
           fprintf(fp, "ticket=!%s!\n", entries[x].val); fflush(fp);
#endif
        } else if (!strcmp(entries[x].name, "text")) {
#ifdef LOG
           fprintf(fp, "text=!%s!\n", entries[x].val); fflush(fp);
#endif
           ftextout(stderr,entries[x].val); /* to strip bad characters */
           /* fprintf(stderr, "%s\n", entries[x].val); */
#ifdef LOG
           fprintf(fp, "textok\n", entries[x].val); fflush(fp);
#endif
        } else if (!strcmp(entries[x].name, "subj") && subjfile) {
           FILE *sfp;
#ifdef LOG
           fprintf(fp, "subj=!%s!\n", entries[x].val); fflush(fp);
#endif
           if ((sfp = fopen(subjfile, "w"))!=NULL) {
              fprintf(sfp, "%s\n", entries[x].val);
              fclose(sfp);
#ifdef LOG
              fprintf(fp, "wrote subject to %s\n", subjfile); fflush(fp);
#endif
           } else {
              printf("Can't open %s\n", subjfile);
#ifdef LOG
              fprintf(fp, "can't open %s\n", subjfile); fflush(fp);
#endif
              exit(1);
           }
        } else if (!strcmp(entries[x].name, "pseudo") && pseudofile) {
           FILE *sfp;
           if ((sfp = fopen(pseudofile, "w"))!=NULL) {
              fprintf(sfp, "%s\n", entries[x].val);
              fclose(sfp);
           } else {
              printf("Can't open %s\n", pseudofile);
              exit(1);
           }
        }
#ifdef LOG
        fprintf(fp, "completed %d/%d\n", x,m); fflush(fp);
#endif
    }
#ifdef LOG
    fprintf(fp, "exiting normally\n"); fclose(fp);
#endif
    exit(0);
}
