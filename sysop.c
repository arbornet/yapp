/* $Id: sysop.c,v 1.12 1998/02/13 10:56:18 thaler Exp $ */

/* SYSOP.C - cfadm-only commands like cfcreate and cfdelete */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>    /* for system() */
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for geteuid */
#endif
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "user.h" /* for get_sysop_login() */
#include "lib.h"  /* for xgets() etc */
#include "xalloc.h"
#include "files.h" /* for O_W */
#include "sum.h"   /* for get_status() */
#include "conf.h"  /* for leave() */
#include "macro.h" /* for conference() */
#include "system.h" /* for SL_OWNER */
#include "stats.h" /* for get_config() */

/******************************************************************************/
/* CHECK WHETHER USER QUALIFIES AS THE SYSOP/CFADM                            */
/******************************************************************************/
int
is_sysop(silent)
   int silent;   /* Skip error message? */
{
   if (uid==get_nobody_uid()) {
      if (!strcmp(login, get_sysop_login()))
         return 1;
   } else if (uid==geteuid())
      return 1;
   if (!silent)
      printf("Permission denied.\n");
   return 0;
}

void
reload_conflist()
{
   char path[MAX_LINE_LENGTH];
   char buff[MAX_LINE_LENGTH];
   int  i;

   if (!(flags & O_QUIET))
      printf("Reloading conflist...\n");
   strcpy(buff, (confidx>=0)? conflist[confidx].name : "");
   strcpy(path, (joinidx>=0)? conflist[joinidx].name : "");
   free_list(conflist);
   free_list(desclist);
   conflist = grab_list(bbsdir,"conflist",0);
   desclist = grab_list(bbsdir,"desclist",0);
   maxconf = xsizeof(conflist)/sizeof(assoc_t);
   for (i=1; i<maxconf; i++) {
      if (!strcmp(conflist[0].location,conflist[i].location)
       ||   match(conflist[0].location,conflist[i].name))
         defidx=i;
      if (buff[0] && !strcmp(buff, conflist[i].name))
         confidx=i;
      if (path[0] && !strcmp(path, conflist[i].name))
         joinidx=i;
   }
   if (defidx<0) 
      printf("Warning: bad default %s\n", conference(0));
}

/******************************************************************************/
/* CREATE A CONFERENCE                                                        */
/******************************************************************************/
int                 /* RETURNS: (nothing)     */
cfcreate(argc,argv) /* ARGUMENTS:             */
int    argc;        /*    Number of arguments */
char **argv;        /*    Argument list       */
{
   char *cfshort=NULL, *cflong=NULL, *cfemail=NULL, 
        *cfsubdir=NULL, *cftype=NULL, *cfhosts=NULL;
   char path[MAX_LINE_LENGTH], 
        buff[MAX_LINE_LENGTH], 
        confdir[MAX_LINE_LENGTH], 
        cfpath[MAX_LINE_LENGTH];
   int ok=1, chacl, previdx;

   if (!is_sysop(0))
      return 1;

   /* Get configuration information */
   if (!(flags & O_QUIET))
      printf("Short name (including underlines): ");
   cfshort = xgets(st_glob.inp, 0);
   if (!cfshort || !cfshort[0]) ok=0;

   if (ok) {
      if (!(flags & O_QUIET))
         printf("Enter one-line description\n> ");
      cflong = xgets(st_glob.inp, 0);
      if (!cflong || !cflong[0]) ok=0;
   }

   if (ok) {
      if (!(flags & O_QUIET))
         printf("Subdirectory [%s]: ", compress(cfshort));
      cfsubdir = xgets(st_glob.inp, 0);
      if (!cfsubdir[0]) {
         xfree_string(cfsubdir);
         cfsubdir = xstrdup(compress(cfshort));
      }
      sprintf(confdir, "%s/confs", get_conf_param("bbsdir", BBSDIR));
      sprintf(cfpath, "%s/%s", get_conf_param("confdir", confdir), cfsubdir);

      if (!cfsubdir || !cfsubdir[0]) ok=0;
   }

   if (ok) {
      if (!(flags & O_QUIET))
         printf("Fairwitnesses: ");
      cfhosts = xgets(st_glob.inp, 0);
      if (!cfhosts || !cfhosts[0]) ok=0;
   }
      
   if (ok) {
      if (!(flags & O_QUIET))
         printf("Security type: ");
      cftype = xgets(st_glob.inp, 0);
      if (!cftype || !cftype[0]) ok=0;
   }
      
   if (ok) {
      sprintf(buff, "Let a %s change the access control list? ", 
       fairwitness(0));
      chacl = get_yes(buff, DEFAULT_ON);
   }
      
   if (ok) {
      if (!(flags & O_QUIET))
         printf("Email address(es) (only used for mail type %ss): ",
          conference(0));
      cfemail = xgets(st_glob.inp, 0);
      if (!cfemail) ok=0;
   }
      
   /* Create the conference */

   if (ok) {
      if (!(flags & O_QUIET))
         printf("Creating conflist entry...\n");
      sprintf(path, "%s/conflist", bbsdir);    /* create conflist entry */
      sprintf(buff, "%s:%s\n", cfshort, cfpath);
      ok = write_file(path, buff);
   }
   
   if (ok) {
      if (!(flags & O_QUIET))
         printf("Creating desclist entry...\n");
      sprintf(path, "%s/desclist", bbsdir);    /* create desclist entry */
      sprintf(buff, "%s:%s\n", compress(cfshort), cflong);
      ok = write_file(path, buff);
   }
      
   if (ok && cfemail[0]) {
      char **array=explode(cfemail," ,",1);  /* In email addresses*/
      int n=xsizeof(array);                  /* Number of email addresses*/
      int i;

      if (!(flags & O_QUIET))
         printf("Creating maillist entry...\n");

      sprintf(path, "%s/maillist", bbsdir); /* create maillist entry */
      for(i=0; i<n && ok; i ++){
            sprintf(buff, "%s:%s\n", array[i], compress(cfshort));
            ok = write_file(path, buff);
      }
      xfree_array(array);
   }
   

   if (ok) {
      if (!(flags & O_QUIET))
         printf("Creating directory...\n");
      mkdir_all(cfpath,0755);
   }
   
   if (ok) {
      if (!(flags & O_QUIET))
         printf("Creating config file...\n");
      sprintf(path, "%s/config", cfpath);       /* create config file */
      sprintf(buff, "!<pc02>\n.%s.cf\n0\n%s\n%s\n%s\n", cfsubdir, cfhosts,
       cftype, cfemail);
      ok = write_file(path, buff);
      chmod(path, 0644);
   }
   
   if (ok) {
      if (!(flags & O_QUIET))
         printf("Creating login file...\n");
      sprintf(path, "%s/login", cfpath);        /* create login file */
      sprintf(buff, 
       "Welcome to the %s %s.  This file may be edited by a %s.\n", 
       compress(cfshort), conference(0), fairwitness(0));
      ok = write_file(path, buff);
      chmod(path, 0644);
   }
      
   if (ok) {
      if (!(flags & O_QUIET))
         printf("Creating logout file...\n");
      sprintf(path, "%s/logout", cfpath);       /* create logout file */
      sprintf(buff, "You are now leaving the %s %s.  This file may be edited by a %s.\n", compress(cfshort), conference(0), fairwitness(0));
      write_file(path, buff);
      chmod(path, 0644);
   }

   reload_conflist();    /* Must be done before load_acl() */
   previdx = confidx;    /* Save confidx to restore after expanding macros */
   confidx = get_idx( compress(cfshort), conflist, maxconf);
   if (confidx < 0)
      ok = 0;
   load_acl(confidx); /* load acl for new conference */

   if (ok) {
      if (!(flags & O_QUIET))
         printf("Creating acl file...\n");
      sprintf(path, "%s/acl", cfpath);       /* create logout file */
      sprintf(buff, "r %s\n", expand("racl", DM_VAR));
      write_file(path, buff );
      sprintf(buff, "w %s\n", expand("wacl", DM_VAR));
      write_file(path, buff );
      sprintf(buff, "c %s\n", expand("cacl", DM_VAR));
      write_file(path, buff );
      sprintf(buff, "a %s\n", (chacl)? "+f:ulist" : "+sysop");
      write_file(path, buff );
      chmod(path, 0644);
   }
   /* Restore original conference index */
   confidx=previdx;
   
   /* Free up space */
   xfree_string(cfshort);
   xfree_string(cflong);
   xfree_string(cfemail);
   xfree_string(cfsubdir);
   xfree_string(cftype);
   xfree_string(cfhosts);

   custom_log("cfcreate", M_OK);

   load_acl(confidx); /* reload acl for current conference */

   return 1;
}

/******************************************************************************/
/* DELETE CURRENT OR OTHER CONFERENCE                                         */
/******************************************************************************/
int                 /* RETURNS: (nothing)     */
cfdelete(argc,argv) /* ARGUMENTS:             */
int    argc;        /*    Number of arguments */
char **argv;        /*    Argument list       */
{
   char *cfshort;
   char path[MAX_LINE_LENGTH];
   char buff[MAX_LINE_LENGTH];
   sumentry_t  fr_sum[MAX_ITEMS];
   status_t    fr_st;
   partentry_t part2[MAX_ITEMS]; 
   int idx = confidx;   /* the conference index to delete */
   int i, max;
   FILE *fp;
   int perm;

   if (!is_sysop(0))
      return 1;

   /* If no conference was specified, prompt for one */
   if (argc>1)
      cfshort = argv[1];
   else {
      if (!(flags & O_QUIET))
         printf("Short name (including underlines): ");
      cfshort = xgets(st_glob.inp, 0);
   }

   idx = get_idx(cfshort, conflist, maxconf);
   if (argc<2)
      xfree_string(cfshort);
   if (idx<0) {
      printf("Cannot access %s %s.\n",conference(0),cfshort);
      return 1;
   }            

   /* Verify that conference has no active items */
   get_status(&fr_st,fr_sum,part2,idx); 
   if (fr_st.i_first<=fr_st.i_last) {
      printf("%s is not empty.\n", conference(1));
      return 1;
   }

   /* Leave conference if we're in it now */
   if (confidx>=0 && confidx==idx) {
      if (!(flags & O_QUIET))
         printf("Leaving %s...\n", conference(0));
      leave(0,(char**)0);
   }

   /* Remove all maillist entries */
   if (fr_st.c_security & CT_EMAIL) {
      assoc_t *maillist;
      if ((maillist=grab_list(bbsdir,"maillist",0)) != NULL) {
         sprintf(path, "%s/maillist", bbsdir);
         if ((fp = mopen(path, O_W))!=NULL) {
            fprintf(fp, "!<hl01>\n%s\n", maillist[0].location);
            max = xsizeof(maillist) / sizeof(assoc_t);
            for (i=1; i<max; i++)
               if (!match(maillist[i].location, conflist[idx].name))
                  fprintf(fp,"%s:%s\n", maillist[i].name, maillist[i].location);
            mclose(fp);
         }
         free_list(maillist);
      }
   }

   /* If we're using cfadm-owned participation files, delete them */
   if ((perm = partfile_perm()) == SL_OWNER) {
      char **ulst;

      if (!(flags & O_QUIET))
         printf("Removing members' participation files...\n");

      ulst=grab_recursive_list(conflist[idx].location, "ulist");
      if (ulst) {
         int i,j,k,n,m;
         char **ucflist,
              **config;
         FILE  *newfp;
      
         config=get_config(idx);
         if (config) {
            n = xsizeof(ulst);
            for (i=0; i<n; i++) {
               get_partdir(path, ulst[i]);
               sprintf(buff, "%s/%s", path, config[CF_PARTFILE]);
               rm(buff, SL_OWNER);
   
               ucflist = grab_file(path,".cflist", GF_WORD|GF_SILENT|GF_IGNCMT);
               m = xsizeof(ucflist);
               if (ucflist) {
                  sprintf(buff,"%s/.cflist",path);
                  newfp = mopen(buff, O_W);
                  if (newfp) {
                     for (j=0; j<m; j++) {
                        k = get_idx(ucflist[j], conflist, maxconf);
                        if (strcmp(conflist[k].location,conflist[idx].location))
                           fprintf(newfp, "%s\n", ucflist[j]);
                     }
                     mclose(newfp);
                  }
                  xfree_array(ucflist);
               }
            }
         }
         xfree_array(ulst);
      }
   }

   /* Remove conflist entries */
   if (!(flags & O_QUIET))
      printf("Removing conflist entries...\n");
   sprintf(path, "%s/conflist", bbsdir);
   if ((fp = mopen(path, O_W))!=NULL) {
      fprintf(fp, "!<hl01>\n%s\n", conflist[0].location);
      max = xsizeof(conflist) / sizeof(assoc_t);
      for (i=1; i<max; i++)
         if (strcmp(conflist[i].location, conflist[idx].location))
            fprintf(fp, "%s:%s\n", conflist[i].name, conflist[i].location);
      mclose(fp);
   }

   /* Remove desclist entry */
   if (!(flags & O_QUIET))
      printf("Removing desclist entry...\n");
   sprintf(path, "%s/desclist", bbsdir);
   if ((fp = mopen(path, O_W))!=NULL) {
      fprintf(fp, "!<hl01>\n%s\n", desclist[0].location);
      max = xsizeof(desclist) / sizeof(assoc_t);
      for (i=1; i<max; i++)
         if (!match(desclist[i].name, conflist[idx].name))
            fprintf(fp, "%s:%s\n", desclist[i].name, desclist[i].location);
      mclose(fp);
   }

   /* Delete the whole subdirectory */
   if (!(flags & O_QUIET))
      printf("Removing directory...\n");
   sprintf(buff, "rm -rf %s", conflist[idx].location); 
   system(buff);

   reload_conflist();
   custom_log("cfdelete", M_OK);

   return 1;
}


int                /* Returns Nothing */ 
upd_maillist(security, config, idx)
   short security; /* Conference security type */   
   char ** config; /* Configuration file for conference */
   int idx;        /* Conference of addresses to update */
{
   char path[MAX_LINE_LENGTH];/* path and name of output file */
   FILE *fp;                  /* Pointer to output file */
   assoc_t *maillist;         /* Contents of maillist */
   int      max_addr=0;       /* Max number of conference addresses */
   int      i,j=0;            /* Current line number in maillist */
   int      max=0;            /* Total number of current lines in maillist */
   char **  addr;             /* Conference addresses */ 
   char *   conf_nm=compress(conflist[idx].name); 


   if (security & CT_EMAIL) {
      if(config && xsizeof(config)>CF_EMAIL){
         addr=explode(config[CF_EMAIL]," ,",1);
         max_addr = xsizeof(addr);
      }

      if ((maillist=grab_list(bbsdir,"maillist",0)) != NULL) {
         max = xsizeof(maillist) / sizeof(assoc_t);
         sprintf(path, "%s/maillist", bbsdir);
     
         /* Output the contents of the maillist array */
         if ((fp = mopen(path, O_W))!=NULL) {
            fprintf(fp, "!<hl01>\n%s\n", maillist[0].location);
            for (i=1; i<max; i++){
               if(strcmp(conf_nm,maillist[i].location))
                  fprintf(fp,"%s:%s\n", maillist[i].name, maillist[i].location);
                else if(j < max_addr){
                  /* Output the current address for the conference */ 
                  fprintf(fp,"%s:%s\n", addr[j],conf_nm);
                  j++;
               }
                  
            }
            /* Add any additional addresses */
            for(;j<max_addr; j++)
               fprintf(fp,"%s:%s\n", addr[j],conf_nm);
            mclose(fp);
         }
         free_list(maillist);
      }
      if(addr)
         xfree_array(addr);
   }

}
