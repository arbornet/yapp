/* $Id: lib.c,v 1.18 1998/02/13 10:56:17 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <stdio.h>
#include <string.h>
#include <sys/types.h> 
#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif
#include <sys/stat.h> 
#include <time.h> 
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* to get O_CREAT, etc */
#endif
#include <ctype.h>
#include <signal.h> /* to get sigvec stuff */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef HAVE_SYS_ERRLIST
extern char *sys_errlist[];  /*standard error messages, usu. in errno.h */
#endif
#include "yapp.h"
#include "struct.h"
#include "xalloc.h"
#include "globals.h"
#include "lib.h"
#include "macro.h"
#include "system.h"
#include "main.h"
#include "files.h"
#include "driver.h" /* for open_pipe */

#define MAX_LINES 5000
#define HEADER_LINES 6

/******************************************************************************/
/* FIND INDEX OF ELT IN ARR AT OR AFTER START, -1 IF NOT FOUND                */
/******************************************************************************/
short                  /* RETURNS: index of elt or -1  */
searcha(elt,arr,start) /* ARGUMENTS:                   */
char  *elt;            /*    String to search for      */
char **arr;            /*    String array to search    */
short  start;          /*    Index to start looking at */
{
   short i,l;
   l=xsizeof(arr);
   for (i=start; i<l; i++)
      if (!strcmp(arr[i],elt)) return i;
   return -1;
}

/******************************************************************************/
/* CHECK FOR A PARTIAL STRING MATCH (required_optional)                       */
/******************************************************************************/
char           /* RETURNS: 1 if match, 0 else        */
match(ent,und) /* ARGUMENTS:                         */
char *ent,     /*    String entered by user          */
     *und;     /*    String with underlines to match */
{
   char *q;
   short n;

   q=strchr(und,'_');          /* "_optional" */
   n=(q)? q-und : strlen(und); /* number of required chars */
   if (!n) return !strlen(ent);     /* dont match "" with anything but "" */
   if (strncasecmp(ent,und,n))                    return 0; /* match before _ */
   if (q && strncasecmp(ent+n,q+1,strlen(ent)-n)) return 0; /* match after  _ */
   if (strlen(ent) > strlen(und)-(q!=0)) return 0; /* no extra chars */
   return 1;
}


/* return static string of fields */
void
implode(buff,arr,sep,start)
char *buff;
char **arr;
char *sep;
short start;
{
   short i,s;

   buff[0]='\0';
   s = xsizeof(arr);
   if (s>start) strcpy(buff,arr[start]);
   for (i=start+1; i<s; i++) {
      strcat(buff,sep);
      strcat(buff,arr[i]);
   }
}

/*
 * This differs from the standard strtok() in that multiple seps are
 * not seen as one.
 */
char *             /* OUT: string in variable-length static buffer */
mystrtok(str, sep)
   char *str;      /* IN: original string to parse, not modified */
   char *sep;      /* IN: characters which will terminate string */
{
   char *start;
   static char *ptr;
   static char *strtok_buff = NULL;

   if (!sep) {
      if (strtok_buff) xfree_string(strtok_buff);
      strtok_buff=NULL;
      return NULL;
   }

   if (str) {
      if (strtok_buff) xfree_string(strtok_buff);
      ptr = strtok_buff = xstrdup(str);
   }

   if (!*ptr) {
      if (strtok_buff) 
         xfree_string(strtok_buff);
      strtok_buff = NULL;
      return NULL;
   }
   start = ptr;

   /* Advance until end of buffer or delimiter found */
   while (*ptr && !strchr(sep, *ptr))
      ptr++;
   if (*ptr)
      *ptr++ = '\0';
  
   return start;
}

/******************************************************************************/
/* SPLIT A STRING INTO AN ARRAY OF FIELDS                                     */
/******************************************************************************/
char **                     /* RETURNS: (nothing)                  */
explode(str,sep,fl)         /* ARGUMENTS:                          */
   char *str;               /*    Filename to read into memory     */
   char *sep;               /*    String of field separators       */
   int fl;                  /*    Ignore extra multiple seps?      */
{
   char **mem,*ln,*nstr;
   int lines;

   mem=xalloc(MAX_LINES,sizeof(char *));
   nstr=xstrdup(str);

   if (sep) {
      ln=(fl)? strtok(nstr,sep) : mystrtok(nstr,sep);
      for (lines=0; ln && lines<MAX_LINES; lines++) {
         while (*ln == ' ') ln++; /* skip leading spaces */
         mem[lines]=xstrdup(ln);
         while (strlen(mem[lines]) && mem[lines][ strlen(mem[lines])-1 ] == ' ')
            mem[lines][ strlen(mem[lines])-1 ] = 0; /* trash trailing spaces */
         ln =(fl)? strtok(NIL,sep) : mystrtok(NIL,sep);
      }
   } else {
      mem[0]=xstrdup(str);
      lines=1;
   }

   mem=xrealloc_array(mem,lines);
   xfree_string(nstr);
   return mem;
}

/******************************************************************************/
/* APPEND A LINE TO A FILE                                                    */
/******************************************************************************
Function:    char write_file(char *filename, char *string)
Called by:  
Arguments:   filename to put stuff in, string to put there
Returns:     1 on success, 0 if error
Calls:
Description: Appends a block of text to a file with a single operation.
             Locks & unlocks file to be safe.
*******************************************************************************/
char                        /* RETURNS: 1 on success, 0 if error   */
write_file(filename,str)    /* ARGUMENTS:                          */
char *filename;             /*    Filename to append to            */
char *str;                  /*    Block of text to write           */
{
   FILE *fp;
   long mod=O_A;

   if (st_glob.c_security & CT_BASIC) mod |= O_PRIVATE;
   if ((fp=mopen(filename,mod))==NULL) return 0;
   fwrite(str,strlen(str),1,fp);
   mclose(fp);
   return 1;
}
            
/******************************************************************************/
/* DUMP A FILE TO THE OUTPUT                                                  */
/******************************************************************************
Function:    char cat(char *dir, char *filename)
Called by:   
Arguments:   filename to display
Returns:     1 on success, 0 on failure
Calls:
Description: Copies a file to the screen (not grab_file)
*******************************************************************************/
char                        /* RETURNS: (nothing)                  */
cat(dir,filename)           /* ARGUMENTS:                          */
char *dir;                  /*    Directory containing file        */
char *filename;             /*    Filename to display              */
{
   FILE *fp;
   int c;
   char buff[MAX_LINE_LENGTH];

   if (filename)
      sprintf(buff,"%s/%s",dir,filename);
   else
       strcpy(buff,dir);
   if (debug & DB_LIB) 
      printf("cat: %s\n",buff);
   if ((fp=mopen(buff,O_R|O_SILENT))==NULL) return 0;
   while ((c=fgetc(fp))!=EOF && !(status & S_INT)) wputchar(c);
   mclose(fp);
   return 1;
}
extern char *pipebuf;
/******************************************************************************/
/* DUMP A FILE TO THE OUTPUT THROUGH A PAGER                                  */
/******************************************************************************/
char                        /* RETURNS: (nothing)                  */
more(dir,filename)          /* ARGUMENTS:                          */
char *dir;                  /*    Directory containing file        */
char *filename;             /*    Filename to display              */
{
   FILE *fp;
   char buff[MAX_LINE_LENGTH],*p;
   int c;

   /* Need to check if pager exists */
   if (!pipebuf) {
      p = expand("pager", DM_VAR);
      if (p) {
         pipebuf = xstrdup(p);
      }
   }
   if (!(flags & O_BUFFER) || !pipebuf)
      return cat(dir,filename);

   if (filename)
      sprintf(buff,"%s/%s",dir,filename);
   else
       strcpy(buff,dir);
   if (debug & DB_LIB) 
      printf("CAT: %s\n",buff);
   if ((fp=mopen(buff,O_R|O_SILENT))==NULL) return 0;
   open_pipe();
   while ((c=fgetc(fp))!=EOF) 
      if (fputc(c,st_glob.outp)==EOF) break;
   spclose(st_glob.outp);
   mclose(fp);
   status &= ~S_INT;
   return 1;
}

/******************************************************************************/
/* GET INPUT WITHOUT ECHOING IT                                               */
/******************************************************************************/
char *         /* RETURNS: text entered */
get_password() /* ARGUMENTS: (none)     */
{
   static char buff[MAX_LINE_LENGTH];
   short i=0,c;
 
   unix_cmd("/bin/stty -echo");
   /*while ((c=fgetc(mystdin))!=10 && c!=13 && c!= -1 && i<MAX_LINE_LENGTH) */
   while ((c=fgetc(st_glob.inp))!=10 && c!=13 && c!= -1 && i<MAX_LINE_LENGTH) 
      buff[i++]=c;
   buff[i]=0;
   putchar('\n');
   unix_cmd("/bin/stty echo");
   return buff;
}

/******************************************************************************/
/* GET INPUT INTO ARBITRARILY SIZED BUFFER                                    */
/* Also get multiple lines if a line ended with \                             */
/******************************************************************************/
char *         /* RETURNS: text entered */
xgets(fp, lvl) /* ARGUMENTS:            */
   FILE *fp;   /*    Input stream       */
   int   lvl;  /*    Min stdin level, 0 if not reading from stdin  */
{
   char *ok;
   char *str;
   int   strsize;
   int i,j,strip=0;
   char *tmp, *cp;
   int done=0, len;

   if (!fp)
      return NULL;

   /* Initialize buffer */
   cp = str = (char *)xalloc(0, MAX_LINE_LENGTH+1);
   len = strsize = MAX_LINE_LENGTH;

   /* Loop over \-continued lines */
   do {

      /* If reading from command input, reset stuff */
      if (fp==st_glob.inp) { /* st_glob.inp */
         if (status & S_PAGER)
            spclose(st_glob.outp); 
   
         /* Make SIGINT abort fgets() */
         ints_on();
      }

      /* Get a line, (may be aborted by SIGINT) */
      ok=fgets(cp, len, fp);
      
      /* If command input, reset stuff */
      if (fp==st_glob.inp) { /* st_glob.inp */
   
         /* Stop SIGINT from aborting fgets() */
         ints_off();
   
         if (!ok) {
            /* If reading from tty, just reset the EOF */
/* XXX this should only happen for KEYBOARD input, not xfile XXX */
#if 0
            if (fp==st_glob.inp && !(status & S_BATCH)) 
#else
            if (isatty(fileno(st_glob.inp)))
#endif
            { /* mystdin */
               clearerr(fp); 
               if (!(flags & O_QUIET))
                  printf("\n"); 

            } else {

               /* Reading commands from a file */
               xfree_string(str);
#if 0
               /*
                * By request of the River, if one puts "r" in one's .cfrc file,
                * the user should be left at the RFP prompt.  PicoSpan and 
                * Yapp2.3 both do this.  This means that EOF from a sourced 
                * file must be ignored, and just go back to the previous 
                * stdin.
                */
   	       st_glob.inp = stdin; /* mystdin */
   	       mclose(fp);
   	       return ngets(str, st_glob.inp);
#endif

               if (stdin_stack_top > 0 + (orig_stdin[0].type==STD_SKIP)) {
                  pop_stdin();
                  if (stdin_stack_top >= lvl)
                     return xgets(st_glob.inp, lvl);
               }
               return NULL;
   	    }
         }
      }

      /* If SIGINT seen when getting a command, return empty command */
      if (!ok && (status & S_INT) && (fp==st_glob.inp)) { /* st_glob.inp */
         /* for systems where interrupts abort fgets */
         str[0] = '\0';
         return str;
      }

      /* Strip other characters if needed */
      if ((fp==st_glob.inp) && (flags & O_STRIP)) { /* st_glob.inp */
         tmp = (char *)xalloc(0, strlen(cp)+1);
         for (i=j=0; i<strlen(cp); i++) {
            if (isprint(cp[i]) || isspace(cp[i])) 
               tmp[j++]=cp[i];
            else 
               printf("%s ^%c",(strip++)? "":"Stripping bad input:", cp[i]+64);
         }
         if (strip) printf("\n");
         tmp[j]='\0';
         strcpy(cp,tmp);
         xfree_string(tmp);
      }

      /* If it ends with \\\n, delete both */
      if (ok && strlen(cp)>1 && cp[strlen(cp)-1]=='\n' && cp[strlen(cp)-2]=='\\') {
         cp[strlen(cp)-2]='\0';
      }

      /* If newline read, trash it and mark as done */
      if (ok && cp[0] && cp[strlen(cp)-1]=='\n') {
         cp[ strlen(cp)-1 ] =0;
         done = 1;
      } else if (!ok) { /* EOF */
         done = 1;
      } else { /* continues on next line */
         cp += strlen(cp);
         len = strsize-(cp-str); /* space left */
         if (len < 80) {
            strsize += 256;
            str = (char*)xrealloc_string(str, strsize);
            len += 256;
            cp  = str+strsize-len;
         }
         done=0;
      }

   } while (!done);

   if (ok)
      return str;
  
   xfree_string(str);
   return NULL;
}

/******************************************************************************/
/* GET INPUT WITHOUT OVERFLOWING BUFFER                                       */
/* Also get multiple lines if a line ended with \                             */
/******************************************************************************/
char *        /* RETURNS: text entered */
ngets(str,fp) /* ARGUMENTS:            */
   char *str; /*    Input buffer       */
   FILE *fp;  /*    Input stream       */
{
   char *ok;
   int i,j,strip=0;
   char tmp[MAX_LINE_LENGTH], *cp=str;
   int done=0, len=MAX_LINE_LENGTH;

   /* Loop over \-continued lines */
   do {

      /* If reading from command input, reset stuff */
      if (fp==st_glob.inp) { /* st_glob.inp */
         if (status & S_PAGER)
            spclose(st_glob.outp); 
   
         /* Make INT abort fgets() */
         ints_on();
      }

      /* Get a line, (may be aborted by SIGINT) */
      if (!fp) {
         str[0] = '\0';
         return NULL;
      } else
         ok=fgets(cp, len, fp);
      
      /* If command input, reset stuff */
      if (fp==st_glob.inp) { /* st_glob.inp */
   
         /* Stop INT from aborting fgets() */
         ints_off();
   
         if (!ok) { 
            /* If reading from tty, just reset the EOF */
/* XXX this should only happen for KEYBOARD input, not xfile XXX */
            if (fp==st_glob.inp && !(status & S_BATCH)) { /* mystdin */
               clearerr(fp); 
               if (!(flags & O_QUIET))
                  printf("\n"); 
            } else {
/* we might want the old stuff below, and here's why:
 * when your .cfrc contains "r" and there's new stuff, you got in 2.3
 * a message saying "Tried to close unopened file" because of the mclose
 * below.  However, the ngets allowed control to flow from a script to
 * stdin easily, so you wouldn't get a Stopping message.  Turns out this
 * is extremely problematic and not really necessary.  Basically,
 * you want to return all the way up to source() to close the file,
 * and Stopping is generated way down inside, in a get_command() in item.c
 */

               /* Reading commands from a file */
#if 0
   	       st_glob.inp = stdin; /* mystdin */
   	       mclose(fp);
   	       return ngets(str, st_glob.inp); /* why??? */
#endif
               if (stdin_stack_top > 0 + (orig_stdin[0].type==STD_SKIP)) {
                  pop_stdin();
                  return ngets(str, st_glob.inp);
               } else {
                  return NULL;
               }
   	    }
         }
      }

      /* If SIGINT seen when getting a command, return empty command */
      if (ok)
         cp[ strlen(cp)-1 ] =0; /* trash newline */
      else if ((status & S_INT) && (fp==st_glob.inp)) { /* st_glob.inp */
         /* for systems where interrupts abort fgets */
         str[0]='\0';
         return str;
      }
   
      /* Strip characters if needed */
      if ((fp==st_glob.inp) && (flags & O_STRIP)) { /* st_glob.inp */
         for (i=j=0; i<strlen(cp); i++) {
            if (isprint(cp[i]) || isspace(cp[i])) tmp[j++]=cp[i];
            else {
               printf("%s ^%c",(strip++)? "" : "Stripping bad input:", cp[i]+64);
            }
         }
         if (strip) printf("\n");
         tmp[j]='\0';
         strcpy(cp,tmp);
      }

      /* Check if continues on next line */
      done=1;
      if (cp[0] && cp[ strlen(cp)-1 ] == '\\') {
         cp += strlen(cp)-1;
         len = MAX_LINE_LENGTH-(cp-str); /* space left */
         if (len>1)
            done=0;
      }

   } while (!done);

   return (ok)? str : NULL;
}


/******************************************************************************/
/* READ A FILE INTO AN ARRAY OF STRINGS (1 ELT PER LINE)                      */
/******************************************************************************
Function:    char **grab_file(char *dir,char *filename, char flags)
Called by:   main
Arguments:   Filename to grab
Returns:     Array of char *'s with lines of file
Calls:
Description: This function will read in an entire file of text into
             memory, dynamically allocating enough space to hold it.
*******************************************************************************/
char **                     /* RETURNS: (nothing)                  */
grab_file(dir,filename,fl)  /* ARGUMENTS:                          */
char *dir;                  /*    Directory containing file        */
char *filename;             /*    Filename to read into memory     */
char fl;                    /*    Flags (see lib.h)                */
{
   char **mem;
   FILE *fp;
   char  buff[MAX_LINE_LENGTH];  /* filename */ 
   char  buff2[MAX_LINE_LENGTH]; /* each word is a line */
   char *buff3= NULL;            /* normal files */
   int lines, max;

   if (filename)
      sprintf(buff,"%s/%s",dir,filename);
   else
      strcpy(buff,dir);
   if ((fp=mopen(buff,(fl & GF_SILENT)? O_R|O_SILENT : O_R))==NULL)
      return 0;
   max=(fl & GF_HEADER)? HEADER_LINES : MAX_LINES;
   mem=xalloc(max, sizeof(char *));
   if (debug & DB_LIB)
      printf("MEM: %x size=%d  want %d  eltsize=%d\n", mem, sizeof(mem), 
       max, sizeof(char *));
   if (fl & GF_WORD) /* count each word as a line */
      for (lines=0; lines<max && fscanf(fp,"%s",buff2)==1; ) {
         /* what type of file is this? */
         if (buff2[0]=='#' && (fl & GF_IGNCMT))
            fgets(buff2, MAX_LINE_LENGTH, fp);
         else
            mem[lines++]=xstrdup(buff2);
      }
   else {
/* for (lines=0; lines<max && ngets(buff2,fp) && !(status & S_INT); lines++) */
      for (lines=0; lines<max && (buff3 = xgets(fp,0)) != NULL; ) {
         if (buff3[0]!='#' || !(fl & GF_IGNCMT))
            mem[lines++]=buff3;
         else
            xfree_string(buff3);
      }
   }
   mclose(fp);
   if (lines<max) 
      mem=xrealloc_array(mem,lines);
   else if (lines==MAX_LINES) {
      printf("Error: %s too long\n",buff);
      mem=xrealloc_array(mem,0);
   }
   return mem;
}


/******************************************************************************/
/* GRAB SOME MORE OF A FILE UNTIL WE FIND SOME STRING                         */
/******************************************************************************/
char **              /* RETURNS: (nothing)                  */
grab_more(fp,end,fl,endlen) /* ARGUMENTS:                          */
FILE *fp;            /*    Input file pointer               */
char *end;           /*    String start to stop after       */
char  fl;            /*    Flags (see lib.h)                */
   int *endlen;      /*    Actual length of stop string     */
{
   char **mem;
   char buff[MAX_LINE_LENGTH];
   char*buff2; /* Need variable length for large responses */
   int  lines,max;

   if (endlen)
      (*endlen)=0;

   max=(fl & GF_HEADER)? HEADER_LINES : MAX_LINES;
   mem=xalloc(max,sizeof(char *));
   if (debug & DB_LIB)
      printf("MEM: %x size=%d  want %d  eltsize=%d\n", mem, sizeof(mem), 
       max, sizeof(char *));
   if (fl & GF_WORD) /* count each word as a line */
      for (lines=0; lines<max && fscanf(fp,"%s",buff)==1; lines++) {
         mem[lines]=xstrdup(buff);
         if (end && !strncmp(buff,end, strlen(end))) {
            if (endlen)
               (*endlen)=strlen(buff);
            break;
         }
      }
   else 
      for (lines=0; lines<max && (buff2=xgets(fp,0))!= NULL; lines++) {
         if (end && buff2[0]==end[0] && buff2[1]==end[0]){
            mem[lines]=xstrdup(buff2+2);
            xfree_string(buff2);
         } else
            mem[lines]=buff2;
         if (end && (!strncmp(buff2,end, strlen(end)) 
                  || !strncmp(buff2,",R",2))) {
            if (endlen)
               (*endlen)=strlen(buff2);
            break;
         }
      }
   if (lines<max) mem=xrealloc_array(mem,lines);
   return mem;
}

/******************************************************************************/
/* GET INPUT UNTIL USER SAYS YES OR NO                                        */
/******************************************************************************/
int             /* RETURNS: 1 for yes, 0 no     */
get_yes(pr,err) /* ARGUMENTS:                   */
   char *pr;    /*    Prompt                    */
   int   err;   /*    Answer to return on error */
{
   char buff[MAX_LINE_LENGTH], *p=buff;
   
   for(;;) {
      if (!(flags & O_QUIET))
         wputs(pr);
      if (!ngets(buff,st_glob.inp)) /* st_glob.inp */
         return err;

      /* Skip leading whitespace */
      while (isspace(*p)) p++;

      if (match(p,"n_on") || match(p,"nop_e")) return 0;
      if (match(p,"y_es") || match(p,"ok")) return 1;
      printf("\"%s\" is invalid.  Try yes or no.\n", p);
   } 
}

#ifndef HAVE_STRNCASECMP
/******************************************************************************/
/* STANDARD strncasecmp() CODE FOR THOSE OS'S WHICH DON'T HAVE IT                */
/******************************************************************************/
int 
strncasecmp(a,b,n)
char *a,*b;
int  n;
{
   char *s;
   for (s=a; (tolower(*s) == tolower(*b)) && (s-a < n); s++, b++)
      if (*s == '\0')
         return 0;
   if (s-a >= n) return 0;
   return tolower(*s) - tolower(*b);
}
#endif

/******************************************************************************/
/* READ IN ASSOCIATIVE LIST                                                   */
/* ! and # begin comments and are not part of the list                        */
/* =filename chains to another file                                           */
/******************************************************************************/
assoc_t *               /* RETURNS: list on success, NULL on error */
grab_list(dir,filename, flags) /* ARGUMENTS:                     */
   char   *dir;         /*    Directory containing file   */
   char   *filename;    /*    Filename to read from       */
   int     flags;
{
   FILE    *fp;
   char     buff[MAX_LINE_LENGTH],
            name[MAX_LINE_LENGTH], /* Full pathname of filename */
           *loc;
   assoc_t *list;                /*    Array to fill in            */
   int      size = 0;            /* Current size of array */

   /* Compose filename */
   if (filename && dir) sprintf(name,"%s/%s",dir,filename);
   else if (dir)        strcpy(name,dir);
   else if (filename)   strcpy(name,filename);
   else return 0;

   /* Open the file to read */
   if ((fp=mopen(name,O_R|O_SILENT))==NULL) {
      if (!(flags & GF_SILENT))
         error("grabbing list ",name);
      return 0;
   }

   if (!(flags & GF_NOHEADER)) {

      /* Get the first line (skipping any comments) - this is the default */
      do {
         loc=ngets(buff,fp);
      } while (loc && (buff[0]=='#' || buff[0]=='!'));

      /* If empty, return null array */
      if (!loc) {
         (void)fprintf(stderr,"Error: %s is empty.\n",name);
         mclose(fp);
         return NULL;
      }

      /* Start the list, and save default in location 0 */
      list = (assoc_t *)xalloc(0, MAX_LINES*sizeof(assoc_t));
      list[0].name     = xstrdup("");
      list[0].location = xstrdup(buff);
      size = 1;
      if (debug & DB_LIB) 
         printf("Default: '%s'\n",buff);
      if (strchr(buff,':')) 
         (void)fprintf(stderr,"Warning: %s may be missing default.\n",name);
   } else
      list = (assoc_t *)xalloc(0, MAX_LINES*sizeof(assoc_t));
   
   /* Read until EOF */
   while (ngets(buff,fp)) {

      if (debug & DB_LIB) 
         printf("Buff: '%s'\n",buff);
      if (buff[0]=='#' || buff[0]=='\0') 
         continue; /* Skip comment and blank lines */

      /* Have a line, split into name and location */
      if ((loc = strchr(buff,':')) != NULL) {
         strncpy(name,buff,loc-buff);
         name[loc-buff]=0;
         loc++;

         if (size==(xsizeof(list)/sizeof(assoc_t)))
            list = (assoc_t *)xrealloc_string(list, (xsizeof(list)+MAX_LINES)*sizeof(assoc_t));

         list[size].name     = xstrdup(name);
         list[size].location = xstrdup(loc);
         if (debug & DB_LIB)
            printf("Name: '%s' Dir: '%s'\n",list[size].name, 
             list[size].location);
         size++;

      /* Chain to another file */
      } else if (buff[0]=='=' && strlen(buff)>1) {
         mclose(fp);

         if (buff[1]=='%') sprintf(name,"%s/%s",bbsdir,buff+2);
         else if (dir)     sprintf(name,"%s/%s",dir,buff+1);
         else              strcpy(name,buff+1);

         if ((fp=mopen(name,O_R|O_SILENT))==NULL) {
            error("grabbing list ",name);
            break;
         }
         ngets(buff,fp); /* read magic line */
         if (debug & DB_LIB) 
            printf("grab_list: magic %s\n",buff);

      } else {
         (void)fprintf(stderr,"Bad line read: %s\n",buff);
      }
   }
   mclose(fp);
   list = (assoc_t*)xrealloc_string(list, size*sizeof(assoc_t));
   return list;
}

/******************************************************************************/
/* FIND INDEX OF NAME IN AN ASSOCIATIVE LIST                                  */
/******************************************************************************/
int                    /* RETURNS: -1 on error, else index of elt in list */
get_idx(elt,list,size) /* ARGUMENTS:                                      */
   char    *elt;          /*    String to match                              */
   assoc_t *list;         /*    List of elements to search                   */
   int      size;         /*    Number of elements in the list               */
{      
   int   i;
    
   if (list[0].name && list[0].name[0] && match(elt,list[0].name))
      return 0; /* in case it's a list without default */
   for (i=1; i<size && !match(elt,list[i].name); i++);
   return (i<size)? i : -1;
}   

void
free_list(list)
   assoc_t *list;
{
   int i, sz;
   
   sz = xsizeof(list)/sizeof(assoc_t);
   for (i=0; i<sz; i++) {
      xfree_string(list[i].name);
      xfree_string(list[i].location);
   }
   xfree_string(list);
}

/******************************************************************************/
/* CONVERT TIMESTAMP INTO A STRING IN VARIOUS FORMATS                         */
/******************************************************************************/
char *          /* RETURNS: Date string */
get_date(t,sty) /* ARGUMENTS:           */
   time_t t;    /*    Timestamp         */
   int    sty;  /*    Output style      */
{
   static char buff[MAX_LINE_LENGTH];
   struct tm *tms;
   static char *fmt[]={
#ifdef NOEDATE
   /*  0 */ "%a %b %d %H:%M:%S %Y",         /* dates must be in 05 format */
   /*  1 */ "%a, %b %d, %Y (%H:%M)",  
#else
   /*  0 */ "%a %b %e %H:%M:%S %Y",         /* dates need not have leading 0 */
   /*  1 */ "%a, %b %e, %Y (%H:%M)", 
#endif
   /*  2 */ "%a", 
   /*  3 */ "%b",
   /*  4 */ "%e",
   /*  5 */ "%y",
   /*  6 */ "%Y",
   /*  7 */ "%H",
   /*  8 */ "%M",
   /*  9 */ "%S",
   /* 10 */ "%I",
   /* 11 */ "%p",
   /* 12 */ "%p",
#ifdef NOEDATE
   /* 13 */ "(%H:%M:%S) %B %d, %Y",
#else
   /* 13 */ "(%H:%M:%S) %B %e, %Y",
#endif
	/* 14 */ "%Y%m%d%H%M%S",
   /* 15 */ "%a, %d %b %Y %H:%M:%S",
   /* 16 HEX */ "",
   /* 17 */ "%m"
   /* 18 DEC */ "",
   };

   tms = localtime(&t);
   if (sty<0 || sty==16 || sty>18)
/*		sty=0; */
      sprintf(buff,"%X", t);
   else if (sty==18) 
      sprintf(buff,"%u", t);
   else {
      char tmp[80], *p; /* to test for %e support */
      strcpy(tmp, fmt[sty]);
      strftime(buff,MAX_LINE_LENGTH,fmt[4],tms);
      if (strchr(buff,'e')) { /* No %e support */
         /* Convert all e's to d's */
         for (p=tmp; *p; p++)
            if (*p=='e') 
               *p='d';
      }
      strftime(buff,MAX_LINE_LENGTH,tmp,tms);
   }
   return buff;
}

/******************************************************************************/
/* GENERATE STRING WITHOUT ANY "'_s IN IT                                     */
/* the value returned needs to be free'd by a call to xfree_string            */
/******************************************************************************/
char *       /* RETURNS: New string (always shorter than original) */
noquote(s,x) /* ARGUMENTS:          */
char *s;     /*    Original string  */
int x;       /*    Remove _'s too?  */
{
   int s_len=0; /* String Length of incomming string */
   char * ptr = NULL;
   char *p,*q,qu=0;

   /* Create buffer big enough to hold the arbitray length string
    * which is passed in
    */
   s_len = strlen(s) + 1;
   ptr = (char *)xalloc(0, s_len);

   /*
    *	Place string with  no quotes into ptr string
    */
   q=s;
   if (*q=='"' || *q=='\'') { qu= *q; q++; }
   for (p=ptr; *q; q++)
      if (x==0 || *q != '_')
         *p++ = *q;
   *p=0;
   p--;
   if (p>=ptr && *p==qu) *p=0;
   return ptr;
}

/******************************************************************************/
/* GENERATE STRING WITHOUT ANY _'s IN IT                                      */
/******************************************************************************/
char *      /* RETURNS: New string */
compress(s) /* ARGUMENTS:          */
char *s;    /*    Original string  */
{
   static char buff[MAX_LINE_LENGTH];
   char *p,*q;

   for (p=buff,q=s; *q; q++)
      if (*q != '_') 
         *p++ = *q;
   *p=0;
   return buff;
}

void
error(str1,str2)
char *str1,*str2;
{
   FILE *fp;
   char errorlog[MAX_LINE_LENGTH];
   char timestamp[MAX_LINE_LENGTH];
   time_t t;

   if (errno)
      fprintf(stderr,"Got error %d (%s) in %s%s\n",errno,sys_errlist[errno],
       str1,str2);
   sprintf(errorlog,"%s/errorlog",bbsdir);
   if ((fp=fopen(errorlog,"a")) != NULL) {
      time(&t);
      strcpy(timestamp,ctime(&t)+4);
      timestamp[20]='\0';
      if (errno)
         fprintf(fp,"%-8s %s Got error %d (%s) in %s%s\n",
          login, timestamp, errno,sys_errlist[errno],str1,str2);
      else
         fprintf(fp,"%-8s %s WARNING: %s%s\n", login, timestamp, str1,str2);
      fclose(fp);
   }
}

char *
lower_case(str)
char *str;
{
   static char buff[MAX_LINE_LENGTH];
   char *p,*q; 
   for (p=buff, q=str; *q; p++,q++)
      *p = tolower(*q);
   *p = '\0';
   return buff;
} 



void 
mkdir_all (path, mode)
   char * path;
   mode_t mode;
{
   char        *p;  /* Parse pieces of path */
   struct stat sb; /* Struct containing directory status */

   /* Make sure directory doesn't exist before creating it */
   if( stat( path, &sb) ){
      for( p=path; *p!='\0'; p++ ){
          /*  Make sure each piece of path  exists */
         if(*p == '/' && p > path ){
            *p = '\0';
             mkdir(path, mode);
             *p = '/'; 
         }
      }

      /* Create the entire directory before exit */ 
      if(mkdir(path, mode)){
          error("Creating directory ",path);
      }
   }
}

