/* STRUCT.H: @(#)struct.h 1.14 93/06/06 Copyright (c)1993 thalerd */
#include <stdio.h>
#include <sys/types.h>

/* Flag type */
typedef unsigned long flag_t; /* expand type when we need > 16 option flags */

/* Command line option flag */
typedef struct {
    char   *name;     /* name used with define, change */
    flag_t  mask;     /* bitmask */
    char    deflt;
} option_t;

/* Macros */
typedef struct macro_tmp {
   char          *name;
   unsigned short mask;
   char          *value;
   struct macro_tmp *next;
} macro_t;

/* Error */
typedef struct {
    int     severity; /* severity level, 0 informational */
    char   *message;  /* Format string for printing error */
} error_t;

typedef struct {
   char  *name;
   int   token_type;
} keyword_t;

typedef struct {
   char *name;
   char *location;
} assoc_t;

typedef struct {
   short  flags; /* item flags (see IF_xxx in yapp.h)           */
   short  nr;    /* number of responses (not inc. initial text) */
   time_t last;  /* item file modification time */
   time_t first; /* item file creation time */
/* char  *subj;  Subj must be separate so sum.c can block dump sum file */
} sumentry_t;

typedef struct {
   short nr;
   long  last;
} partentry_t;

typedef struct {
   char *name;
   int (*func) PROTO((int,char **));
} dispatch_t;

/* Global status structure */
typedef struct {
unsigned int c_security;           /* cf security type */
#ifdef NEWS
   long   c_article;               /* highest article # seen */
#endif
   short  c_status,                /* cf status flags */
          c_confitems,             /* # sum file entries */
          i_first,                 /* first item in conference */
          i_last,                  /* last item in conference */
          i_current,               /* current item */
          i_next,                  /* next item */
          i_prev,                  /* prev item */
	  i_newresp,               /* # of old items with new responses */
	  i_brandnew,              /* # of brand new items */
	  i_unseen,                /* # of unseen items */
          i_numitems,              /* total # of active items */
          r_totalnewresp,          /* total # of new responses in cf */
          r_first,                 /* first resp to process */
          r_last,                  /* last resp to process */
          r_current,               /* current response */
          r_max,                   /* highest response # of current item */
          r_lastseen,              /* highest response # seen */
          l_current;               /* current line # of response */
   char   fullname[MAX_LINE_LENGTH]; /* fullname in current cf */

   /* Range specifiers */
   char   string[MAX_LINE_LENGTH]; /* "string" range           */
   char   author[MAX_LINE_LENGTH]; /* author (login) specified */
   short  rng_flags;               /* item status flags for range */
   time_t since,                   /* since <date> range */
          before;                  /* before <date> range */

   off_t  mailsize;                /* last size of mailbox */

   time_t sumtime,                 /* lastmod of sum file */
          parttime;                /* lastmod of participation file */
   FILE  *outp;                    /* output stream (pager) pointer */
   FILE  *inp;                     /* input stream pointer */
   short  opt_flags,               /* option flags */
          count;                   /* count of something */
} status_t;

/* Response */
typedef struct {
   char  *fullname; /* Author's full name                   */
   char  *login;    /* Author's login                       */
   int    uid;      /* Author's UID                         */
   short  flags;
   char **text;     /* The actual text lines                */
   short  numchars; /* How many characters in the response? */
   time_t date;     /* Timestamp of entry                   */
   long   offset,   /* Offset to start of ,R line           */
          textoff,  /* Offset to start of actual text       */
          endoff;   /* Offset to start of next response     */
   short  parent;   /* This is a response to what # (+1)?   */
#ifdef NEWS
   char  *mid;      /* Message ID string (for Usenet)       */
   long   article;  /* Article number    (for Usenet)       */
#endif
} response_t;

typedef struct {
   int    type;
   union {
      int   i;
      char *s;  
   } val;
} entity_t;

typedef struct {
   int   type;
   int   fd;
   FILE *fp;
} stdin_t;
