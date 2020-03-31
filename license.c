/* $Id: license.c,v 1.19 1997/08/28 00:02:47 thaler Exp $ */
/*
 * This module is the license server.  There is a license/ directory
 * under the BBSDIR which contains files which we lock.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <pwd.h>    /* for getpwuid */
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for getuid */
#endif
#include "yapp.h"
#include "struct.h"
#include "files.h"
#include "lib.h"  /* for xgets(), ngets() */
#include "user.h" /* for make_ticket() */
#include "globals.h"
#include "license.h"
#include "xalloc.h" /* for xfree_string */
#include "sum.h"    /* for get_hash */

#define CFADM "cfadm"
#define UNLIMITED -1
#define LIC_TIME_LEEWAY 300 /* Allow 5 minute difference between saved */
                            /* timestamp, and file timestamp           */

/* Allow 1 week difference between saved timestamp, and file timestamp */
#define INITIAL_LIC_TIME_LEEWAY (7*24*60*60)

       char  *regto = NULL;
static int lic_users = 0, lic_invocations = 0; 
static int lic_count = 0, lic_time = 0;
static int lic_expires = 0;
static FILE *lfp=NULL; /* license file pointer */

static int
lock_license(i)
   int i;
{
   struct stat st; 
   char filename[256];
   int err;

   sprintf(filename, "%s/%d", get_conf_param("licensedir", LICENSEDIR), i);
   err=stat(filename,&st);
   if (err) /* doesn't exist */
      lfp = mopen(filename, O_W|O_SILENT|O_NOBLOCK);
   else     /* does exist    */
      lfp = mopen(filename, O_RPLUS|O_LOCK|O_SILENT|O_NOBLOCK);
   return (lfp != NULL);
}

/*
 * Given 4 integers compute a checksum on them using a "secret"
 * algorithm.
 */
static char *
compute_checksum(max_users, max_count, curr_count, curr_time, exp_time)
   int max_users;
   int max_count;
   int curr_count;
   int curr_time;
   int exp_time;  /* IN: license expiration time, or 0 if never */
{
   char buff[MAX_LINE_LENGTH];

   sprintf(buff, "%08X%08X%08X%08X", 
    max_users, max_count, curr_count, curr_time);
   if (exp_time)
      sprintf(buff+strlen(buff), "%08X", exp_time);
   return make_ticket(buff, get_hash(hostname));
}

#define SECS_IN_DAY (60*60*24)

int
get_license()
{
   int i;
   struct stat st;
   char path[MAX_LINE_LENGTH];
   FILE *rfp;
   char buff[MAX_LINE_LENGTH];
   int offset;
   char lic_checksum[MAX_LINE_LENGTH], *lc;
   int  curr_time;

   sprintf(path, "%s/registered", get_conf_param("licensedir", LICENSEDIR));
   if (!stat(path,&st)) {
      if (st.st_mode != 0644)
         chmod(path, 0644);
      rfp     = mopen(path, O_RPLUS|O_LOCK); /* open and lock */
      if (rfp) {
         /* Read license information */
         regto = xgets(rfp, 0);
         if (ngets(buff, rfp))
            sscanf(buff, "%d %d", &lic_users, &lic_invocations);

         offset = ftell(rfp);
         if (ngets(buff, rfp)) 
            sscanf(buff, "%d %d", &lic_count, &lic_time);

         lc = ngets(lic_checksum, rfp);

         if (ngets(buff, rfp)) 
            sscanf(buff, "%d", &lic_expires);

         /* Test checksum */
         if (!lc || strcmp(lic_checksum,
          compute_checksum(lic_users, lic_invocations, lic_count, lic_time, 
           lic_expires))) {
            printf("Invalid checksum\n");
            lic_users = lic_invocations = lic_count = lic_time = 0;
         } else {

            if (st.st_mtime - lic_time > ((lic_count)? LIC_TIME_LEEWAY :
                INITIAL_LIC_TIME_LEEWAY)) {
               printf("WARNING: Timestamp problems with license registration!\n");
            }

            /* Process license expiration */
            if (lic_expires) {
               curr_time = time(NULL);
               if (curr_time > lic_expires) {
                   printf("Content-type: text/plain\n\n");
                   printf("The Yapp license has expired.\n");

                   error("license expired", "");
                   lic_users = lic_invocations = lic_count = lic_time = 0;
               }
            }

            /* Test/Update count: only done for "nobody" invocations 
             * This is to help avoid malicious users trying to use up
             * the daily count.
             */
            if ((lic_invocations!=UNLIMITED && getuid()==get_nobody_uid())
             || !lic_count) {
               curr_time = time(NULL);
               if ((lic_time/SECS_IN_DAY) == (curr_time/SECS_IN_DAY)) {
                  lic_count++;
                  if (lic_count > lic_invocations) {
                      char errbuff[MAX_LINE_LENGTH];
                      int secs_left = SECS_IN_DAY - (curr_time % SECS_IN_DAY);
                      int mins_left=0, hrs_left=0;
                      hrs_left  = (secs_left / 3600);
                      mins_left = (secs_left / 60) % 60;

                      printf("Content-type: text/plain\n\n");
                      printf("The Yapp license limit on hits/day has been exceeded.\n");
                      printf("Try again in %d hours, %d minutes.\n", 
                       hrs_left, mins_left);

                      sprintf(errbuff, "hits/day limit exceeded with %d:%02d left today", hrs_left, mins_left);
                      error(errbuff, "");
                      lic_users = lic_invocations = lic_count = lic_time = 0;
                  }
               } else { /* day rollover */
                  char rollpath[MAX_LINE_LENGTH];

                  sprintf(rollpath, "%s/usagelog", bbsdir);
                  strcpy(buff, ctime((time_t *)&lic_time));
                  sprintf(buff+10, ": %d\n", lic_count);
                  write_file(rollpath, buff);
                  lic_count = 1;
               }

               /* Write new information */
               if (!lic_count || lic_users) {
                  lic_time = curr_time;
                  fseek(rfp, offset, SEEK_SET);
                  fprintf(rfp, "%d %d\n%s\n", lic_count, lic_time, 
                   compute_checksum(lic_users, lic_invocations, lic_count, 
                   lic_time, lic_expires));
                  if (lic_expires)
                     fprintf(rfp, "%d\n", lic_expires);
                  ftruncate(fileno(rfp), ftell(rfp));
               }
            }
         }
         mclose(rfp); /* close file & free lock */
      } else {
         char *cfadm = get_conf_param("cfadm", CFADM);
         struct passwd *pwd = getpwuid(st.st_uid);
         printf("Couldn't open and lock %s\n", path);
         if (strcmp(pwd->pw_name, cfadm))
            printf("Notice: %s should be owned by %s!\n", path, cfadm);
         pwd = getpwuid(geteuid());
         if (strcmp(pwd->pw_name, cfadm))
            printf("Notice: bbs should be setuid %s!\n", cfadm);
      }
   } else 
      printf("License registration information not found (%s)\n", path);


   if (getuid()==get_nobody_uid())
      return (lic_invocations != 0);

   if (getuid()!=get_nobody_uid() && lic_users == UNLIMITED)
      return 1;
/*
   if (lic_users == UNLIMITED || !isatty(0))
      return 1;
 */
  
   for (i=1; i<=lic_users && !lock_license(i); i++);
   if (i<=lic_users) {
/*printf("Got license #%d\n", i);*/
      return 1;
   }
   printf("There are already %d copies being used.  Try again later.\n", lic_users);
   return 0;
}

void
release_license()
{
   if (lic_users != UNLIMITED && lfp)
      mclose(lfp);
   if (regto)
      xfree_string(regto);
}

/*****************************/
/* Routines to configure Yapp according to /etc/yapp.conf
 * if it exists (otherwise try /usr/local/etc/yapp.conf, then ~/yapp.conf,
 * and finally ./yapp.conf).
 */
static assoc_t *conf_params=NULL;
static int     nconf_params=0;

static void
read_config2(filename)
   char *filename;
{
   conf_params = grab_list("/etc", filename, GF_SILENT|GF_NOHEADER);
   if (!conf_params)
      conf_params =grab_list("/usr/local/etc", filename, GF_SILENT|GF_NOHEADER);
   if (!conf_params)
      conf_params =grab_list("/usr/bbs", filename, GF_SILENT|GF_NOHEADER);
   

   if (!conf_params) {
      struct passwd *pwd = getpwuid(geteuid());
      if (pwd)
         conf_params =grab_list(pwd->pw_dir, filename, GF_SILENT|GF_NOHEADER);
   }
   if (!conf_params)
      conf_params =grab_list(".", filename, GF_SILENT|GF_NOHEADER);
   if (conf_params)
      nconf_params = xsizeof(conf_params) / sizeof(assoc_t);
}

void
read_config()
{
   read_config2("yapp3.1.conf");
   if (!conf_params)
      read_config2("yapp.conf");
}

char *
get_conf_param(name, def)
   char *name;
   char *def;
{
   int i;

   if (!nconf_params) return def;
   i = get_idx(name, conf_params, nconf_params);
   return (i<0)? def : conf_params[i].location;
}

void
free_config()
{
   free_list(conf_params);
}

int
get_hits_today()
{
   char **file;
   int x;
   file = grab_file(get_conf_param("licensedir", LICENSEDIR), "registered", 
    GF_NOHEADER);
   if (!file)
      return 0;
   x = (xsizeof(file)>2)? atoi(file[2]) : 0;
   xfree_array(file);
   return x;
}
