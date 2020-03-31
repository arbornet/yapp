/* $Id: change.c,v 1.16 1997/02/16 01:33:25 kaylene Exp $ */
/* PHASE 1: Conference Subsystem 
         Be able to enter/exit the program, and move between conferences
         Commands: join, next, quit, source
         Files: rc files, cflist, login, logout, bull
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "conf.h"
#include "lib.h"
#include "joq.h"
#include "sum.h"
#include "item.h"
#include "range.h"
#include "macro.h"
#include "system.h"
#include "sep.h"
#include "xalloc.h"
#include "stats.h" /* for get_config */
#include "license.h" /* for get_conf_param */
#include "user.h" /* for get_sysop_login */
#include "sysop.h" /* for is_sysop() */
#include "driver.h" /* for command() */
#include "main.h" /* for wputs() */
#include "files.h" /* for mdump() */
#include "security.h" /* for check_acl() */

char *cfiles[]={ "logi_n", "logo_ut", "in_dex", "b_ull", "we_lcome", 
   "html_header", "rc", "sec_ret", "ul_ist", "origin_list", "obs_ervers", 
   "acl", "rc.www","" };

/******************************************************************************/
/* CHANGE SYSTEM PARAMETERS                                                   */
/******************************************************************************/
int              /* RETURNS: (nothing)     */
change(argc,argv) /* ARGUMENTS:             */
int    argc;      /*    Number of arguments */
char **argv;      /*    Argument list       */
{
   char buff[MAX_LINE_LENGTH];
   char done;
   short i,j;
	char **config;

   if (argc<2) {
      printf("Change what?\n");
      return 1;
   }

   for (j=1; j<argc; j++) {

      /* Security measure */
      if ((orig_stdin[stdin_stack_top].type & STD_SANE) 
           && match(argv[j],"noverbose"))
         continue;

      /* Process changing flags */
      for (done=0,i=0; option[i].name; i++) {
         if (match(argv[j],option[i].name)) {
            flags |=  option[i].mask;
            done=1;
            break;
         } else if(!strncmp(argv[j],"no",2) && match(argv[j]+2,option[i].name)){
            flags &= ~option[i].mask;
            done=1;
            break;
         }
      }
      if (done) continue;

      if        (match(argv[j],"n_ame")
       ||        match(argv[j],"full_name") 
       ||        match(argv[j],"u_ser")) {       
         printf("Your old name is: %s\n",st_glob.fullname);
         if (!(flags & O_QUIET))
            printf("Enter replacement or return to keep old? ");
         if (ngets(buff, st_glob.inp) && strlen(buff)) { /* st_glob.inp */

            /* Expand seps in name IF first character is % */
            if (buff[0]=='%') {
               char *str, *f;
               str = buff+1;
               f = get_sep(&str);
               strcpy(buff, f);
            }

            if (sane_fullname(buff))
               strcpy(st_glob.fullname,buff);
         } else
            printf("Name not changed.\n");
      } else if (match(argv[j],"p_assword")
       ||        match(argv[j],"passwd")) {      passwd(0,NULL);
      } else if (match(argv[j],"li_st")) {       
         if (partfile_perm()==SL_USER)
            edit(partdir,".cflist",0);
         else
            priv_edit(partdir,".cflist",2); /* 2=force install */
         refresh_list();
      } else if (match(argv[j],"cfonce")) {      edit(work,".cfonce",0);
      } else if (match(argv[j],"cfrc")) {        edit(work,".cfrc",0);
      } else if (match(argv[j],"cfjoin")) {      edit(work,".cfjoin",0);
      } else if (match(argv[j],"cgirc") || match(argv[j], "illegal")
       || match(argv[j], "matched")) {
         struct stat st;
         char file[MAX_LINE_LENGTH];
         char wwwdef[256], *wwwdir;
         sprintf(wwwdef, "%s/www", get_conf_param("bbsdir", BBSDIR));
         wwwdir = get_conf_param("wwwdir", wwwdef);
   
         if (!is_sysop(0))
            return 1;
         if (argv[j][0]=='c')
            sprintf(file,"%s/rc.%s", wwwdir, expand("cgidir", DM_VAR));
         else
            sprintf(file,"%s/%s", wwwdir, argv[j]);

         /* Assert existence of file */
         if (stat(file, &st)) {
            FILE *fp;
            if ((fp = fopen(file, "w"))==NULL) {
               error("creating ", file);
               return 1;
            }
            fclose(fp);
         }
         priv_edit(file, NULL, 0);
         return 1;
      } else if (match(argv[j],"ig_noreeof")) {  unix_cmd("/bin/stty eof ^-");
      } else if (match(argv[j],"noig_noreeof")){ unix_cmd("/bin/stty eof ^D");
      } else if (match(argv[j],"ch_at")) {       unix_cmd("mesg y");
      } else if (match(argv[j],"noch_at")) {     unix_cmd("mesg n");
      } else if (match(argv[j],"resign")) {      command("resign",0);
      } else if (match(argv[j],"sa_ne")) {       undefine(DM_SANE);
      } else if (match(argv[j],"supers_ane")) {  undefine(DM_SANE|DM_SUPERSANE);
      } else if (match(argv[j],"save_seen")) {
         if (confidx>=0) {
				if ((config = get_config(confidx)) != NULL)
				   write_part(config[CF_PARTFILE]);
         }
      } else if (match(argv[j],"sum_mary")) {
         if (!(st_glob.c_status & CS_FW)) {
            wputs("Sorry, you can't do that!\n");
            return 1;
         }
         printf("Regenerating summary file; please wait\n");
         refresh_sum(0,confidx,sum,part,&st_glob);
         save_sum(sum,(short)-1,confidx,&st_glob);
#ifdef SUBJFILE
         rewrite_subj(confidx); /* re-write subjects file */
#endif
      } else if (match(argv[j],"nosum_mary")) {
         if (!(st_glob.c_status & CS_FW)) {
            wputs("Sorry, you can't do that!\n");
            return 1;
         }
         sprintf(buff,"%s/sum",conflist[confidx].location);
         rm(buff,SL_OWNER);
#ifdef SUBJFILE
         sprintf(buff,"%s/subjects",conflist[confidx].location);
         rm(buff,SL_OWNER);
         clear_subj(confidx); /* re-write subjects file */
#endif
      } else if (match(argv[j],"rel_oad")) {
         if (confidx>=0) {
				if ((config = get_config(confidx)) != NULL)
				   read_part(config[CF_PARTFILE],part,&st_glob,confidx);
         }
         st_glob.sumtime = 0;
      } else if (match(argv[j],"config")) {
         if (!is_sysop(0))
            return 1;
         if (confidx>=0) {
            priv_edit(conflist[confidx].location, "config", 0);
            clear_cache();
            if ((config = get_config(confidx)) != NULL){
               st_glob.c_security = security_type(config, confidx);
               upd_maillist(st_glob.c_security,config, confidx);
            }
         }
         return 1;
      } else {
         for (i=0; cfiles[i][0]; i++) {
            if (match(argv[j],cfiles[i])) {
               if (i==11) {
                  if (!check_acl(CHACL_RIGHT, confidx)) {
                     if (!(flags & O_QUIET))
                        printf("Permission denied.\n");
                     return 1;
                  }
               } else {
                  if (!(st_glob.c_status & CS_FW)) {
                     printf("You aren't a %s.\n", fairwitness(0));
                     return 1;
                  }
               }
               priv_edit(conflist[confidx].location, compress(cfiles[i]), 0);
               sprintf(buff, "%s/%s", conflist[confidx].location, 
                compress(cfiles[i]));
               chmod(buff, (st_glob.c_security & CT_BASIC)? 0600 : 0644);

               /* Re-initialize acl when doing "change acl" */
               if (i==11)
                  reinit_acl();
               return 1;
            }
         }
         printf("Bad parameters near \"%s\"\n",argv[j]);
         return 2;
      }
   }
   return 1;
}

/******************************************************************************/
/* DISPLAY SYSTEM PARAMETERS                                                  */
/******************************************************************************/
int               /* RETURNS: (nothing)     */
display(argc,argv) /* ARGUMENTS:             */
int    argc;       /*    Number of arguments */
char **argv;       /*    Argument list       */
{
   time_t t;
   char *noconferr="Not in a conference!\n",*var;
   short  i,done,j;
	char **config;

   for (j=1; j<argc; j++) {

      /* Display flag settings */
      for (done=0,i=0; option[i].name; i++) {
         if (match(argv[j],option[i].name)) {
            printf("%s flag is %s\n",compress(option[i].name),
             (flags & option[i].mask)? "on" : "off" );
            done=1;
            break;
         }
      }
      if (match(argv[j],"ma_iltext")) {
         refresh_stats(sum,part,&st_glob);
         check_mail(1);
      }
      if (done) continue;

      if (match(argv[j],"fl_ags")) {
         for (done=0,i=0; option[i].name; i++) {
            printf("%-10s : %s%s",compress(option[i].name),
             (flags & option[i].mask)? "ON " : "off",
	     (i%4 == 3)? "\n":"    ");
         }
	 if (i%4) printf("\n");
      } else if (match(argv[j],"c_onference")) {
         refresh_stats(sum,part,&st_glob);
         sepinit(IS_START);
         confsep(expand("confmsg", DM_VAR),confidx,&st_glob,part,0);
      } else if (match(argv[j],"conferences")) {
         command("list",0);
      } else if (match(argv[j],"d_ate") || match(argv[j],"t_ime")) {
         (void)time(&t);
         (void)printf("Time is %s",ctime(&t));
      } else if (match(argv[j],"def_initions") || match(argv[j],"ma_cros")) {
         command("define", 0);
      } else if (match(argv[j],"v_ersion")) {
         extern char *regto;
         (void)printf("YAPP %s  Copyright (c)1995 Armidale Software\n%s\n",
          VERSION, regto);
      } else if (match(argv[j],"ret_ired")) {
         int c=0;

         refresh_sum(0,confidx,sum,part,&st_glob);
         printf("%ss retired:", topic(1));
         for (i=st_glob.i_first; i<=st_glob.i_last; i++) {
            if (sum[i-1].flags & IF_RETIRED) {
               if (!c) printf("\n");
               printf("%4d",i);
               c++;
            }
         }
         if (c)
            printf("\nTotal: %d %ss retired.\n",c, topic(0));
         else
            printf(" <none>\n");
      } else if (match(argv[j],"fro_zen")) {
         int c=0;
        
         refresh_sum(0,confidx,sum,part,&st_glob);
         printf("%ss frozen:", topic(1));
         for (i=st_glob.i_first; i<=st_glob.i_last; i++) {
            if (sum[i-1].flags & IF_FROZEN) {
               if (!c) printf("\n");
               printf("%4d",i);
               c++;
            }
         }
         if (c)
            printf("\nTotal: %d %ss frozen.\n",c, topic(0));
         else
            printf(" <none>\n");
      } else if (match(argv[j],"f_orgotten")) {
         int c=0;
         printf("%ss forgotten:", topic(1));
         for (i=st_glob.i_first; i<=st_glob.i_last; i++) {
            if (part[i-1].nr<0) {
               if (!c) printf("\n");
               printf("%4d",i);
               c++;
            }
         }
         if (c)
            printf("\nTotal: %d %ss forgotten.\n",c, topic(0));
         else
            printf(" <none>\n");
      } else if (match(argv[j],"sup_eruser")) {
         printf("fw superuser %s\n",(st_glob.c_status & CS_FW)?"yes":"no");
      } else if (match(argv[j],"fw_slist")
       ||        match(argv[j],"fair_witnesslist")
       ||        match(argv[j],"fair_witnesses")) {
         if (confidx<0) wputs(noconferr);
         else {
				if ((config = get_config(confidx)) != NULL)
               printf("fair witnesses: %s\n",config[CF_FWLIST]);
         }
      } else if (match(argv[j],"i_ndex")
       ||        match(argv[j],"ag_enda")) {
         sepinit(IS_START);
         confsep(expand("indxmsg", DM_VAR),confidx,&st_glob,part,0);
      } else if (match(argv[j],"li_st")) {         show_cflist();
      } else if (match(argv[j],"b_ulletin")) {
         sepinit(IS_START);
         confsep(expand("bullmsg", DM_VAR),confidx,&st_glob,part,0);
      } else if (match(argv[j],"w_elcome")) {
         sepinit(IS_START);
         confsep(expand("wellmsg", DM_VAR),confidx,&st_glob,part,0);
      } else if (match(argv[j],"logi_n")) {
         sepinit(IS_START);
         confsep(expand("linmsg", DM_VAR), confidx,&st_glob,part,0);
      } else if (match(argv[j],"logo_ut")) {
         sepinit(IS_START);
         confsep(expand("loutmsg", DM_VAR),confidx,&st_glob,part,0);
      } else if (match(argv[j],"cfjoin")) {
         more(work,".cfjoin");
      } else if (match(argv[j],"cfonce")) {
         more(work,".cfonce");
      } else if (match(argv[j],"cfrc")) {
         more(work,".cfrc");
#ifdef WWW
      } else if (match(argv[j],"origin_list")) {
         if (confidx<0) wputs(noconferr);
         else more(conflist[confidx].location,"originlist");
#endif
      } else if (match(argv[j],"ul_ist")) {
         if (confidx<0) wputs(noconferr);
         else more(conflist[confidx].location,"ulist");
      } else if (match(argv[j],"obs_ervers")) {
         if (confidx<0) wputs(noconferr);
         else more(conflist[confidx].location,"observers");
      } else if (match(argv[j],"acl")) {
         if (confidx<0) wputs(noconferr);
         else more(conflist[confidx].location,"acl");
      } else if (match(argv[j],"html_header")) {
         if (confidx<0) wputs(noconferr);
         else more(conflist[confidx].location,"htmlheader");
      } else if (match(argv[j],"rc")) {
         if (confidx<0) wputs(noconferr);
         else more(conflist[confidx].location,"rc");
      } else if (match(argv[j],"wwwrc") || match(argv[j],"rc.www")) {
         /* conference specific WWW modified rc file */
         if (confidx<0) wputs(noconferr);
         else more(conflist[confidx].location,"rc.www");
      } else if (match(argv[j],"log_messages")) {
         if (confidx<0) wputs(noconferr);
         else {
            printf("login message:\n");
            more(conflist[confidx].location,"login");
            printf("logout message:\n");
            more(conflist[confidx].location,"logout");
         }
      } else if (match(argv[j],"n_ew")) {
         refresh_sum(0,confidx,sum,part,&st_glob);
         sepinit(IS_ITEM);
         open_pipe();
         confsep(expand("linmsg", DM_VAR),confidx,&st_glob,part,0);
         check_mail(1);
      } else if (match(argv[j],"n_ame") || match(argv[j],"u_ser"))
         (void)printf("User: %s\n",fullname_in_conference(&st_glob));
      else if (match(argv[j],"p_articipants")) {
         participants(0,(char**)0);
      } else if (match(argv[j],"s_een")) {
         int c=0;
         FILE *fp;

         refresh_sum(0,confidx,sum,part,&st_glob);
   
         /* Display seen item status */
         open_pipe();
         if (status & S_PAGER) fp = st_glob.outp;
         else                     fp = stdout;
         fprintf(fp,"%s se re fl   lastseen             etime                mtime\n\n", topic(0));
         for (i=st_glob.i_first; i<=st_glob.i_last; i++) {
            if (!part[i-1].nr) continue;
            fprintf(fp,"%4d %2d %2d %2X   %s ",i,abs(part[i-1].nr),
             sum[i-1].nr, sum[i-1].flags, get_date(part[i-1].last,0)+4);
            fprintf(fp,"%s ", get_date(sum[i-1].first,0)+4);
            fprintf(fp,"%s\n", get_date(sum[i-1].last,0)+4);
            c++;
         }
         fprintf(fp,"total %d %ss in seen map\n\n",c, topic(0));
      } else if (match(argv[j],"s_ize"))
         xstat();
      else if (match(argv[j],"strings"))
         xdump();
      else if (match(argv[j],"fds"))
         mdump();
      else if (match(argv[j],"w_hoison")) 
         unix_cmd("who");
      else if ((var=expand(argv[j],~0)) != NULL)
         printf("%s = %s\n",argv[j],var);
      else {
         printf("Bad parameters near \"%s\"\n",argv[j]);
         return 2;
      }
   }
   return 1;
}
