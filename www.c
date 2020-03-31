/* $Id: www.c,v 1.9 1996/12/19 03:52:08 thaler Exp $ */

/* WWW specific commands */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for read */
#endif
#include <ctype.h>  /* for isalnum() */
#include "yapp.h"
#include "struct.h"
#include "macro.h"
#include "lib.h"
#include "xalloc.h"
#include "globals.h" /* for status */
#include "driver.h"  /* for open_pipe() */
#include "main.h"    /* for wfputc() */

static char 
x2c(what)
   char *what;
{
    register char digit;

    digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A')+10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A')+10 : (what[1] - '0'));
    return(digit);
}

static void 
plustospace(str)
   char *str;
{
   register int x;

   for(x=0;str[x];x++) 
      if(str[x] == '+') 
         str[x] = ' ';
} 

/*
 * To be compliant with RFC-1738, any characters other than alphanumerics
 * and "$-_.+!*'()," must be encoded in URLs.
 */
int
url_encode(argc,argv)
   int    argc;
   char **argv;
{
   FILE *fp;
   char  to[4];
   char *from = argv[1];

   /* If `url...` but not `url...|cmd`  (REDIRECT bit takes precedence) */
   if ((status & S_EXECUTE) && !(status & S_REDIRECT)) {
      fp = NULL;
   } else {
      if (status & S_REDIRECT) {
         fp = stdout;
      } else {
         open_pipe();
         fp = st_glob.outp;
         if (!fp) {
            fp = stdout;
         }
      }
   } 

   if (argc<2) {
      printf("syntax: url_encode string\n");
      return 1;
   }

   while (*from) {
      if (isalnum(*from) || strchr("$-_.+!*'(),", *from))
         wfputc(*from++, fp);
      else {
         sprintf(to, "%%%02X", *from++);
         wfputs(to, fp);
      }
   }
   if (fp)
      fflush(fp);  /* flush after done with wfput stuff */
   return 1;
}

static void 
unescape_url(url)
   char *url;
{
    register int x,y;

    for(x=0,y=0;url[y];++x,++y) {
        if((url[x] = url[y]) == '%') {
            url[x] = x2c(&url[y+1]);
            y+=2;
        }
    }
    url[x] = '\0';
}

/*
 * With the -c option, converts newlines to 2 characters ("\n") 
 * Also, if any variable already exists, the value is appended
 * to allow posting lists (from checkbox,select,etc).
 */
int
www_parse_post(argc,argv)
   int    argc;
   char **argv;
{
   register int v, m=-1, cumul=0;
   int cl;
   char *env = expand("requestmethod", DM_VAR), *env2;
   char *buff;
   char **vars, *tmp;
   char *valbuff, *q, *p;
   int convert = 0;

   if (argc>1 && strchr(argv[1], 'c'))
      convert = 1;

   env2 = getenv("CONTENT_TYPE");
   if (!env || strcmp(env, "POST")
    || !env2 || strcmp(env2,"application/x-www-form-urlencoded")) {
       def_macro("error", DM_VAR, 
        "This command can only be used to decode form results.");
       return 1;
   }
   
   env = getenv("CONTENT_LENGTH");
   cl = (env)? atoi(env) : 0;

   buff = (char *)xalloc(0, cl+1);

   /* Read in the whole thing */
   for (cumul=0; cumul<cl; cumul+=m) {
      m = read(saved_stdin[0].fd, buff+cumul, cl-cumul);
      if (m<0) {
         error("reading full ", "content length");
         break;
      }
   }
/* printf("Read %d/%d bytes\n", cumul, cl); fflush(stdout); */

   if (cumul>0) {
      buff[cumul]='\0';
      vars = explode(buff, "&", 0);
      for (v=0; v<xsizeof(vars); v++) {
         plustospace(vars[v]);
         unescape_url(vars[v]);
         tmp = strchr(vars[v], '=');
         if (!tmp) continue;
         *tmp++ = '\0';
         if (tmp) {
            int prevlen;
            char *prev;

            prev = expand(vars[v], DM_VAR);
            prevlen = (prev)? strlen(prev) : 0;

            valbuff = (char *)xalloc(0, prevlen + 2*strlen(tmp) + 2);
            p=valbuff+prevlen; q=tmp;
            if (prevlen) {
               strcpy(valbuff, prev);
               *p++ = ' ';
            }
            while (*q) {
               if (convert) {
                  /*
                   * Chimera sends \n but not \r's
                   * Netscape sends both 
                   */
                  if (*q=='\n') {
                     *p++ = '\\'; /* convert newline to \n */
                     *q = 'n';
                  } else if (*q=='\r') {
                     q++;
                     continue;
                  }
               } else if (*q == '%')
                  *p++ = '%'; /* escape %'s */
               *p++ = *q++;
            }
            *p = '\0';
            def_macro(vars[v], DM_VAR, valbuff);
            xfree_string(valbuff);
         }
      }
      xfree_array(vars);
   }
   xfree_string(buff);
   return 1;
}

void  
urlset() 
{
   char *str, **vars, **fields;
   int v;

   str = expand("querystring", DM_VAR);
   if (!str || !str[0]) return;

   plustospace(str);
   unescape_url(str);
   vars = explode(str, "&", 0);
   for (v=0; v<xsizeof(vars); v++) {
      fields = explode(vars[v], "=", 0);
      if (xsizeof(fields)==2 && fields[0][0])
         def_macro(fields[0], DM_VAR, fields[1]);
      xfree_array(fields);
   }
   xfree_array(vars);
}
