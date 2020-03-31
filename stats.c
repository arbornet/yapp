/* $Id: stats.c,v 1.9 1997/08/28 00:02:04 thaler Exp $ */
/* STATS.C 
 * This module will cache subjects and authors for items in conferences
 * Loading entries is delayed until reference time
 * NUMCACHE conferences may be cached at a time
 *
 * Now we want to be able to make use of the subjects file (if it exists)
 * to initialize the whole cache entry at once.
 *
 * Problem: 
 *
 * Another process updates subjfile, want to make sure we re-read it
 * and not append it.
 *
 * Solution: ONLY append to subjfile when entering a new item
 *           let "set sum" and "set nosum" rewrite and delete it
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "lib.h"
#include "stats.h"
#include "xalloc.h"
#include "system.h"
#include "edit.h" /* for despace */

#define NUMCACHE 2

typedef struct {
   short  idx;                /* conference index              */
   int    subjfile;           /* # subjects in subject file    */
#define SF_DUNNO -1
#define SF_NO    0
   char **config;             /* config info                   */
   char  *subj[MAX_ITEMS];
   char  *auth[MAX_ITEMS];
} cache_t;

static cache_t cache[NUMCACHE];
static int start=1;

static void load_subj PROTO((int i, SHORT idx, SHORT item, sumentry_t *sum));

/******************************************************************************/
/* FREE AUTHORS/SUBJECTS                                                      */
/* Called from get_cache() and clear_cache() above                            */
/******************************************************************************/
static void    /* RETURNS: (nothing)   */
free_elts(arr) /* ARGUMENTS:           */
char **arr;    /*    Array of subjects */
{
   int i;

   for (i=0; i<MAX_ITEMS; i++) {
      if (arr[i])
         xfree_string(arr[i]);
      arr[i]=0;
   }
}

/*
 * GET INDEX OF CF
 *
 * Called from store_auth(), store_subj(), get_subj(), get_auth(), and
 * get_config() below to find location in the cache.
 */
static int     /* RETURNS: Cache location index */
get_cache(idx) /* ARGUMENTS:                    */
   int idx;    /*   Conference #                */
{
   short i;

   /* Initialize cache */
   if (start) {
      for (i=0; i<NUMCACHE; i++)
         cache[i].idx = -1;
      start = 0;
      i=0;
   } else {

      /* Find cf if already cached */
      for (i=0; i<NUMCACHE && idx!=cache[i].idx; i++);
      if (i<NUMCACHE)
         return i;

      /* Find one to evict */
      for (i=0; cache[i].idx == confidx; i++); /* never evict current cf */

      /* Evict it */
      if (cache[i].idx>=0) {
         xfree_array(cache[i].config);
         free_elts(cache[i].subj);
         free_elts(cache[i].auth);
      }
   }

   /* Initialize with new conference info */
   cache[i].idx      = idx;
   cache[i].subjfile = SF_DUNNO;
   if (!(cache[i].config=grab_file(conflist[idx].location,"config", 0)))
      return -1;
   return i;
}

/* 
 * Free up all the space in the cache.  This is called right before
 * the program exits by endbbs().
 */
void
clear_cache()
{
   int i;

   for (i=0; i<NUMCACHE; i++) {
      if (cache[i].idx < 0) continue;
      xfree_array(cache[i].config);
      free_elts(cache[i].subj);
      free_elts(cache[i].auth);
      cache[i].idx = -1;
   }
}

#ifdef SUBJFILE
void
clear_subj(idx)
   short idx;   /* Conference # */
{
   int i;

   if ((i = get_cache(idx))<0)
      return;

   cache[i].subjfile = SF_NO;
}

/* Rewrite the entire subjects file */
void
rewrite_subj(idx)
   short idx;   /* Conference # */
{
   char buff[MAX_LINE_LENGTH];
   char filename[MAX_LINE_LENGTH];
   int i;

   if ((i = get_cache(idx))<0)
      return;

/*
printf("rewrite: before... subjfile=%d confitems=%d\n",
 cache[i].subjfile, st_glob.c_confitems);
*/

   /* Make sure subject file is up to date */
   if (cache[i].subjfile <= st_glob.c_confitems) {
      int j, st;

      sprintf(filename, "%s/%s", conflist[idx].location, "subjects");

      /* Make sure we have subjects 1-st_glob.c_confitems */
      st = cache[i].subjfile;
      if (st<0) st=0;
      for (j=st; j<=st_glob.c_confitems; j++) {
         if (!(sum[j].flags & IF_ACTIVE)) continue;

         if (!cache[i].subj[j])
            load_subj(i,idx,j, sum);
      }

      /* Append the new authors/subjects to the file */
      rm(filename, SL_OWNER);
      for (j=st; j<st_glob.c_confitems; j++) {
         if (!cache[i].subj[j])
            sprintf(buff, "\n");
         else
            sprintf(buff, "%s:%s\n", cache[i].auth[j], cache[i].subj[j]);
         if (!write_file(filename, buff))
            break;
         cache[i].subjfile = j+1;
/*printf("rewrite: %d writing: %s", j, buff);*/
      }
   }
/*
printf("rewrite: after... subjfile=%d confitems=%d\n",
 cache[i].subjfile, st_glob.c_confitems);
*/
}

/*
 * Write out entries to the conference subjects file
 */
int
update_subj(idx, item)
   short idx;        /* Conference # */
   short item;       /* Item #       */
{
   char buff[MAX_LINE_LENGTH];
   char filename[MAX_LINE_LENGTH];
   int i;

   if ((i = get_cache(idx))<0)
      return;

   if (cache[i].subjfile == SF_NO)
      return;

   /* Append the new author/subject to the file */
   sprintf(filename, "%s/%s", conflist[idx].location, "subjects");
   sprintf(buff, "%s:%s\n", cache[i].auth[item], cache[i].subj[item]);
   if (!write_file(filename, buff))
      return 0;
   cache[i].subjfile = item+1;
/*
printf("update: writing '%s:%s'\n", cache[i].auth[item], cache[i].subj[item]);
printf("update: after writing to %s... subjfile=%d confitems=%d\n",
 filename, cache[i].subjfile, st_glob.c_confitems);
*/
}
#endif

/*
 * READ ITEM INFORMATION INTO THE CACHE
 * Called from get_subj() and get_auth() below
 */
static void               /* RETURNS: (nothing) */
load_subj(i,idx,item,sum) /* ARGUMENTS:         */
   int         i;         /*    Cache index     */
   short       idx;       /*    Conference #    */
   short       item;      /*    Item #          */
   sumentry_t *sum;
{
   char **header;
   char   path[MAX_LINE_LENGTH];
   u_int32 tmp;

#ifdef SUBJFILE
{
/*
printf("load: before... subjfile=%d confitems=%d\n",
 cache[i].subjfile, st_glob.c_confitems);
*/
   if (cache[i].subjfile!=SF_NO) {
      if (!(header = grab_file(conflist[idx].location, "subjects", GF_SILENT)))
         cache[i].subjfile = SF_NO;
      else {
         char **field;
         int l, sz = xsizeof(header);

         for (l=0; l<sz; l++) {
            int nf;

            if (field = explode(header[l], ":", 0)) {
               nf = xsizeof(field);

               if (nf>0 && !cache[i].auth[l])
                  cache[i].auth[l]   = xstrdup(field[0]);
               if (nf>1 && !cache[i].subj[l])
                  cache[i].subj[l]   = xstrdup(field[1]);
               xfree_array(field);
            }
         }
         cache[i].subjfile = sz;
/*
printf("load: after loading... subjfile=%d confitems=%d\n",
 cache[i].subjfile, st_glob.c_confitems);
*/
      }
      xfree_array(header);
      if (cache[i].auth[item] && cache[i].subj[item])
         return;
   }
/*
printf("load: after... subjfile=%d confitems=%d\n",
 cache[i].subjfile, st_glob.c_confitems);
*/
}
#endif

   sprintf(path,"%s/_%d",conflist[idx].location,item+1);
   header = grab_file(path,NULL,GF_HEADER);
   if (xsizeof(header)<6) {
      sum[item].nr=0;
      dirty_sum(item);
   } else {
      char *subj = NULL, *auth = NULL;
      register int l;
      int n = xsizeof(header);

      for (l=0; l<n; l++) {
         if (header[l][0]!=',') continue;
         if (header[l][1]=='H') {
            if (subj)
               xfree_string(subj);
            subj = despace(header[l]+2);
         } else if (header[l][1]=='U') {
            char *a = strchr(header[l]+2,',');
            if (auth)
               xfree_string(auth);
            auth = xstrdup((a)? a+1 : "Unknown");
         }
      }

      if (!cache[i].subj[item])
         cache[i].subj[item]   = subj;
      else if (subj)
         xfree_string(subj);
      if (!cache[i].auth[item]) 
         cache[i].auth[item]   = auth;
      else if (auth)
         xfree_string(auth);

#if (SIZEOF_LONG == 8)
      sscanf(header[5]+2,"%x", &tmp);
#else
      sscanf(header[5]+2,"%lx",&tmp);
#endif
      if (tmp != sum[item].first) {
         sum[item].first = tmp;
         dirty_sum(item);
      }
   }
   xfree_array(header);
}

/*
 * Currently unused
 */
void
store_auth(idx, item, str)
   short idx;
   short item;
   char *str;
{
   int i;

   if ((i = get_cache(idx))<0)
      return;
   if (!cache[i].auth[item])
      cache[i].auth[item] = xstrdup(str);
}

/* 
 * Called from item_sum(), enter(), do_enter(), and incorporate2(),
 * i.e. when loading an old item or creating a new one
 */
void
store_subj(idx, item, str)
   short idx;
   short item;
   char *str;
{
   int i;

   if ((i = get_cache(idx))<0)
      return;
   if (cache[i].subj[item])
      xfree_string(cache[i].subj[item]);
   cache[i].subj[item] = xstrdup(str);
}

/* LOOKUP A SUBJECT */
char *                   /* RETURNS: subject string, or "" on failure */
get_subj(idx, item, sum) /* ARGUMENTS:              */
   short       idx;      /*   Conference index      */
   short       item;     /*   Item number           */
   sumentry_t *sum;      /*   Summary array         */
{
   int i;
	
/*
   if (sum[item].flags & IF_RETIRED)
		return 0;
*/
   if ((i = get_cache(idx))<0)
      return "";
   if (!cache[i].subj[item])
      load_subj(i,idx,item, sum);
   return cache[i].subj[item];
}

/*
 * LOOKUP AN AUTHOR 
 * 
 * This is used by the A=login range spec
 */
char *                   /* RETURNS: author (login) string */
get_auth(idx, item, sum) /* ARGUMENTS:                     */
   short       idx;      /*   Conference #                 */
   short       item;     /*   Item #                       */
   sumentry_t *sum;      /*   Item summary array           */
{
   int i;
	
/*
   if (sum[item].flags & IF_RETIRED)
		return 0;
*/
   if ((i = get_cache(idx))<0)
      return 0;
   if (!cache[i].auth[item])
      load_subj(i,idx,item, sum);
   return cache[i].auth[item];
}

char **
get_config(idx)
   short idx;
{
   int i;

   if ((i = get_cache(idx))<0)
      return 0;
   return cache[i].config;
}
