/*  (c)1997 Armidale Software     All Rights Reserved
 *
 *  This program creates a web account from an existing Unix account.
 *  The user's web password is duplicated from the user's Unix password.
 *  This means that THIS PROGRAM MUST RUN SETUID ROOT or else it typically
 *  cannot get the encrypted form of the user's password.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <ctype.h>
#include <string.h>
#include <sys/param.h>
#include <netdb.h>     /* for MAXHOSTNAMELEN */
#include <unistd.h>
#include <dirent.h>

#define HAVE_GETHOSTNAME /* BSD Unix     : FreeBSD,BSDI,SunOS, etc. */
#undef  HAVE_SYSINFO     /* System V Unix: Solaris, etc.            */
#undef  HAVE_GETSPNAM    /*                Solaris                  */
#undef HAVE_DBM_OPEN    /* Most platforms have dbm_open()          */

#ifdef HAVE_SYSINFO
#include <sys/systeminfo.h>
#endif
#ifdef HAVE_DBM_OPEN
#include <ndbm.h>
#include <fcntl.h>
#endif
#ifdef HAVE_GETSPNAM
#include <shadow.h>
#define GETSPNAM getspnam
#else
#define GETSPNAM getpwnam
#endif

/*
 * The defines below are the DEFAULTs which Yapp uses when the yapp.conf
 * file doesn't override some option.  Do not change these, or they won't
 * match what Yapp uses.
 */
#define USER_HOME "/usr/bbs/home"
#define PASS_FILE "/usr/bbs/etc/.htpasswd"
#define USER_FILE "/usr/bbs/etc/passwd"
#define USERDBM   "false"
#define NOBODY    "nobody"
#define CFADM     "cfadm"
#define USRADM    "usradm"
#define VERSION   "3.1.0"

struct passwd nobody, cfadm, usradm;
  
char *conf[]={ "/etc/yapp3.1.conf", "/usr/local/etc/yapp3.1.conf", 
               "/usr/bbs/yapp3.1.conf", 
               "./yapp3.1.conf",
               "/etc/yapp.conf", "/usr/local/etc/yapp.conf", 
               "/usr/bbs/yapp.conf", 
               "./yapp.conf",
               NULL };

#define MAX_PARAMS 20
char param_key[MAX_PARAMS][20], 
     param_value[MAX_PARAMS][80];
int  num_params=0;

/* Fill in the param arrays above from the yapp.conf file */
void
get_yapp_conf()
{
   int   i;
   FILE *fp;
   char buff[256], *ptr;

   for (i=0; conf[i]; i++) {
      if ((fp=fopen(conf[i], "r")) != NULL) {
         while (num_params<MAX_PARAMS && fgets(buff, sizeof(buff), fp)) {
            if (buff[0]=='#'
             || !(ptr = strchr(buff, ':')))
               continue;
            *ptr++ = '\0';
            if (ptr[ strlen(ptr)-1 ]=='\n')
                ptr[ strlen(ptr)-1 ]='\0';
            strcpy(param_key[num_params], buff);
            strcpy(param_value[num_params++], ptr);
         }
         fclose(fp);
         return;
      }
   }
}
  
/* Look up the value of a yapp configuration option */
char *              /* OUT: value of requested parameter */
get_conf_param(name, def)
   char *name;      /* IN : parameter name to get value of */
   char *def;       /* IN : default value if not found */
{
   int i;

   for (i=0; i<num_params; i++) {
      if (!strcmp(param_key[i], name))
         return param_value[i];
   }
   return def;
}

/* Get the home directory used when logging in from the web */
char *             /* OUT: home directory */
get_homedir(login)
   char *login;    /* IN: login */
{
   static char home[MAXPATHLEN];
   sprintf(home, "%s/%c/%c/%s", get_conf_param("userhome", USER_HOME), 
    tolower(login[0]), tolower(login[1]), login);
   return home;
}

/* Get the directory holding the user's participation files */
char *            /* OUT: participation file directory */
get_partdir(login)
   char *login;   /* IN : login */
{
   char buff[MAXPATHLEN];
   static char partdir[MAXPATHLEN];

   strcpy(buff, get_conf_param("partdir","work"));
   if (!strcmp(buff, "work"))
      strcpy(partdir, get_homedir(login));
   else {
     sprintf(partdir, "%s/%c/%c/%s", buff,
      tolower(login[0]), tolower(login[1]), login);
   }
   return partdir;
}

/* Figure out where the user's information should be stored */
char *              /* OUT: filename of user's information file */
get_userfile(login, uid, gid)
   char  *login;    /* IN : login */
   uid_t *uid;      /* OUT: uid to own file */
   gid_t *gid;      /* OUT: gid to own file */
{
static char buff[256];
   char *file;
   char *home = get_homedir(login);

#ifdef HAVE_DBM_OPEN 
   /* If userdbm=true then it's in ~/<login> */
   if (!strcmp(get_conf_param("userdbm",USERDBM),"true")) {
      sprintf(buff, "%s/%c/%c/%s/%s", get_conf_param("userhome", USER_HOME),
       tolower(login[0]), tolower(login[1]), login, login);
      if (uid)
         *uid = nobody.pw_uid;
      if (gid)
         *gid = nobody.pw_gid;
      return buff;
   }
#endif

   /* Otherwise it's in the location specified by userfile */
   file = get_conf_param("userfile", USER_FILE);
   if (file[0]=='~' && file[1]=='/') {
      sprintf(buff, "%s/%s", home, file+2);
      file = buff;
      if (uid)
         *uid = nobody.pw_uid;
      if (gid)
         *gid = nobody.pw_gid;
   } else {
      if (uid)
         *uid = cfadm.pw_uid;
      if (gid)
         *gid = cfadm.pw_gid;
   }
   return file;
}

#ifdef HAVE_DBM_OPEN 
int /* RETURNS: 1 on success, 0 on failure */
save_dbm(db, keystr, valstr)
   DBM  *db;
   char *keystr;
   char *valstr;
{
   datum dkey, dval;
   
   dkey.dptr  = keystr;
   dkey.dsize = strlen(keystr)+1;
   dval.dptr  = valstr;
   dval.dsize = strlen(valstr)+1;
   return dbm_store(db, dkey, dval, DBM_REPLACE);
}  

char *
get_dbm(userfile, keystr)
   char *userfile;
   char *keystr;
{
   datum dkey, dval; 
   DBM *db;

   db = dbm_open(userfile, O_RDWR, 0644);
   if (!db)
      return "";
   dkey.dptr  = keystr;
   dkey.dsize = strlen(keystr)+1;
   dval = dbm_fetch(db, dkey);
   dbm_close(db);
   return (dval.dptr)? dval.dptr : "";
}
#endif /* HAVE_DBM_OPEN */

/* Save user information in the appropriate file */
void
create_user_info(login, fullname, email)
   char *login;    /* IN: login */
   char *fullname; /* IN: full name */
   char *email;    /* IN: email address */
{
   FILE *fp, *tmp;
   uid_t uid;
   gid_t gid;
   char *userfile = get_userfile(login, &uid, &gid);
   char  tmpname[MAXPATHLEN];
   char  buff[256];

#ifdef HAVE_DBM_OPEN 
   /* Support DBM files */
   if (!strcmp(get_conf_param("userdbm",USERDBM),"true")) {
      datum dkey, dval;
      DBM  *db;

      if ((db = dbm_open(userfile, O_RDWR|O_CREAT, 0644))==NULL) {
         perror("dbm_open");
         exit(1);
      }
      if (save_dbm(db, "fullname", fullname)
       || save_dbm(db, "email", email)) {
         perror("dbm_store");
         exit(1);
      }
      dbm_close(db);

      chown(userfile, uid, gid);
      chmod(userfile, 0644);

      return;
   }
#endif /* HAVE_DBM_OPEN */

   /* Support flat text files */

   /* Open new userfile */
   sprintf(tmpname, "%s.%d", userfile, (int)getpid());
   if ((tmp = fopen(tmpname, "w"))==NULL) {
      sprintf(buff, "fopen %s", tmpname);
      perror(buff);
      exit(1);
   }

   /* Find login in user file */
   if ((fp = fopen(userfile, "r"))!=NULL) {
      char str[256];
      int  len = strlen(login)+1;

      sprintf(str, "%s:", login);

      while (fgets(buff, sizeof(buff), fp)) {
         if (!strncmp(buff, str, len))
            break;
         else
            fprintf(tmp, "%s", buff);
      }
   }

   /* Add user's info to userfile */
   fprintf(tmp, "%s:%s:%s\n", login, fullname, email);

   /* Copy remainder of file */
   if (fp) {
      while (fgets(buff, sizeof(buff) ,fp))
         fprintf(tmp, "%s", buff);
      fclose(fp);
   }

   /* Commit the changes */
   fclose(tmp);
   if (rename(tmpname, userfile)) {
      perror("rename");
      exit(1);
   }

   chown(userfile, uid, gid);
   chmod(userfile, 0644);
}

/* Get the local hostname for use in constructing a full email address */
void
get_local_hostname(buff, len)
   char *buff;    /* OUT: fully-qualified domain name of local host */
   int   len;     /* IN : size of buffer */
{
   buff[0]='\0';
#ifdef HAVE_GETHOSTNAME
   if (gethostname(buff,len))
      perror("getting host name");
#else
#ifdef HAVE_SYSINFO 
   if (sysinfo(SI_HOSTNAME, buff, len)<0)
      perror("getting host name");
#endif
#endif

   /* If hostname is not fully qualified, try to get it from /etc/resolv.conf */
   if (!strchr(buff, '.')) {
      FILE *fp;
      if ((fp = fopen("/etc/resolv.conf", "r")) != NULL) {
         char buff[256], field[80], value[80];
         while (fgets(buff, sizeof(buff), fp)) {
            if (sscanf(buff, "%s%s", field, value)==2
             && !strcmp(field,"domain")) {
               sprintf(buff+strlen(buff), ".%s", value);
               break;
            }
         }
         fclose(fp);
      }
   }
}

/* Create a web password file entry and home directory */
void
create_web_account(login, passwd, uid, gid)
   char *login;  /* IN : login to create */
   char *passwd; /* IN : encrypted passwd */
   uid_t uid;    /* IN : user ID of 'nobody' */
   gid_t gid;    /* IN : group ID of 'nobody' */
{
   FILE *fp, *tmp;
   char *passfile = get_conf_param("passfile", PASS_FILE);
   char  home[MAXPATHLEN];
   char  tmpname[MAXPATHLEN];
   char  buff[256];

   if (strcmp(passfile, get_conf_param("userfile", USER_FILE))) {

   /* Open new .htpasswd file */
   sprintf(tmpname, "%s.%d", passfile, (int)getpid());
   if ((tmp = fopen(tmpname, "w"))==NULL) {
      sprintf(buff, "fopen %s", tmpname);
      perror(buff);
      exit(1);
   }

   /* Find login in pass file */
   if ((fp = fopen(passfile, "r"))!=NULL) {
      char str[256];
      int  len = strlen(login)+1;

      sprintf(str, "%s:", login);

      while (fgets(buff, sizeof(buff), fp)) {
         if (!strncmp(buff, str, len))
            break;
         else
            fprintf(tmp, "%s", buff);
      }
   }

   /* Add password to .htpasswd */
   fprintf(tmp, "%s:%s\n", login, passwd);

   /* Copy remainder of file */
   if (fp) {
      while (fgets(buff, sizeof(buff) ,fp))
         fprintf(tmp, "%s", buff);
      fclose(fp);
   }

   /* Commit the changes */
   fclose(tmp);
   if (rename(tmpname, passfile)) {
      perror("rename");
      exit(1);
   }
   chown(passfile, cfadm.pw_uid, cfadm.pw_gid);
   chmod(passfile, 0644);
   }

   /* Make user's web home directory (and any subdirs needed before it) */
   sprintf(home, "%s/%c", get_conf_param("userhome", USER_HOME), 
    tolower(login[0]));
   mkdir(home, 0755);
   if (chown(home, uid, gid))
      perror(home);
   sprintf(home+strlen(home), "/%c", tolower(login[1]));
   mkdir(home, 0755);
   if (chown(home, uid, gid))
      perror(home);
   sprintf(home+strlen(home), "/%s", login);
   mkdir(home, 0755);
   if (chown(home, uid, gid))
      perror(home);
}

void
usage()
{
   fprintf(stderr, "Yapp %s (c)1996 Armidale Software\n usage: webuser [-dehlprsuv] [login]\n", VERSION);
   fprintf(stderr, " -d        Disable logins to account\n");
   fprintf(stderr, " -e        Enable disabled account\n");
   fprintf(stderr, " -h        Help (display this text)\n");
   fprintf(stderr, " -l        List all web accounts\n");
   fprintf(stderr, " -p        Change password on account\n");
   fprintf(stderr, " -r        Remove account\n");
   fprintf(stderr, " -s        Show account information\n");
   fprintf(stderr, " -u        Create/update web account and password (default)\n");
   fprintf(stderr, " -v        Version (display this text)\n");
   fprintf(stderr, " login     Specify Unix login\n");
   exit(1);
}

char *           /* OUT: encrypted web password, or NULL if no account found */
get_web_password(login)
   char *login;  /* IN : login to find */
{
   char *passfile = get_conf_param("passfile", PASS_FILE);
   FILE *fp;
   static char buff[256];

#ifdef HAVE_DBM_OPEN
   if (!strcmp(get_conf_param("userdbm",USERDBM),"true")
    && !strcmp(get_conf_param("userfile",USER_FILE), 
               get_conf_param("passfile",PASS_FILE))) {
      char *userfile = get_userfile(login, NULL, NULL);
      return get_dbm(userfile, "passwd");
   }
#endif /* HAVE_DBM_OPEN */

   /* Find login in pass file */
   if ((fp = fopen(passfile, "r"))!=NULL) {
      char str[256];
      int  len = strlen(login)+1;

      sprintf(str, "%s:", login);

      while (fgets(buff, sizeof(buff), fp)) {
         if (!strncmp(buff, str, len)) {
            if (buff[ strlen(buff)-1 ]=='\n')
                buff[ strlen(buff)-1 ]='\0';
            fclose(fp);
            return buff+len; /* start of password */
         }
      }
      fclose(fp);
   } 
   return 0;
}

void
get_user_info(login, fullname, email)
   char *login;    /* IN : Unix login    */
   char *fullname; /* OUT: Full name     */
   char *email;    /* OUT: Email address */
{
   static char buff[256];
   FILE *fp;
   char *userfile = get_userfile(login, NULL, NULL);

#ifdef HAVE_DBM_OPEN
   if (!strcmp(get_conf_param("userdbm",USERDBM),"true")) {
      strcpy(fullname, get_dbm(userfile, "fullname"));
      strcpy(email,    get_dbm(userfile, "email"));
      return;
   }
#endif /* HAVE_DBM_OPEN */

   /* Find login in user file */
   if ((fp = fopen(userfile, "r"))!=NULL) {
      int  len = strlen(login)+1;
      char str[256];

      sprintf(str, "%s:", login);

      while (fgets(buff, sizeof(buff), fp)) {
         if (!strncmp(buff, str, len)) {
            (void)strtok(buff, ":\n");
            strcpy(fullname, strtok(NULL, ":\n"));
            strcpy(email, strtok(NULL, ":\n"));
            break;
         }
      }
      fclose(fp);
   }
}

/* Make a web account with the same password as a Unix account */
void
update(login)
   char *login; /* IN: Unix login */
{
#ifdef HAVE_GETSPNAM
   struct spwd *spw;
#endif
   struct passwd *pw, user;
   char  hostname[MAXHOSTNAMELEN];
   char  email[256];

   /* If root, usradm, or nobody, then allow arbitrary login to be specified */
   pw = getpwuid(getuid());
   if (pw && (pw->pw_uid==0 || pw->pw_uid==usradm.pw_uid
    || pw->pw_uid==nobody.pw_uid))
      pw = getpwnam(login);
   else if (strcmp(login, getlogin())) {
      printf("webuser: Permission denied\n");
      exit(1);
   }
   if (pw==NULL) {
      printf("webuser: no such login\n");
      exit(1);
   }
   if (!pw->pw_uid) {
      printf("webuser: cannot create a web account with root access\n");
      exit(1);
   }
   memcpy(&user, pw, sizeof(user)); /* save everything */

   /* Construct home directory */
#ifdef HAVE_GETSPNAM
   spw = getspnam(pw->pw_name); /* get encrypted password */
   create_web_account(user.pw_name,spw->sp_pwdp,nobody.pw_uid,nobody.pw_gid);
#else
   create_web_account(user.pw_name,user.pw_passwd,nobody.pw_uid,nobody.pw_gid);
#endif

   /* Construct email address */
   get_local_hostname(hostname, sizeof(hostname));
   sprintf(email, "%s@%s", user.pw_name, hostname);

   /* Add web user information */
   create_user_info(user.pw_name, strtok(user.pw_gecos, ","), email);
}

/* Re-enable a disabled web account */
void
enable(login)
   char *login; /* IN: Web login */
{
   char *old_ep;

   /* Only root and usradm may do this */
   if (getuid() != 0 && getuid() != usradm.pw_uid) {
      printf("webuser: Permission denied\n");
      exit(1);
   }

   /* Get old password */
   old_ep = get_web_password(login);
   if (!old_ep) {
      printf("webuser: no such login '%s'\n", login);
      exit(1);
   }

   /* Make sure it isn't already enabled */
   if (strncmp(old_ep, "*:", 2)) {
      printf("webuser: '%s' is already enabled\n", login);
      exit(1);
   }

   /* Change password */
   create_web_account(login, old_ep+2, nobody.pw_uid, nobody.pw_gid);
}

/* Disable a web account */
void
disable(login)
   char *login; /* IN: Web login */
{
   char *old_ep;
   char  new_ep[256];

   /* Only root and usradm may do this */
   if (getuid() != 0 && getuid() != usradm.pw_uid) {
      printf("webuser: Permission denied\n");
      exit(1);
   }

   /* Get old password */
   old_ep = get_web_password(login);
   if (!old_ep) {
      printf("webuser: no such login '%s'\n", login);
      exit(1);
   }

   /* Make sure it isn't already disabled */
   if (!strncmp(old_ep, "*:", 2)) {
      printf("webuser: '%s' is already disabled\n", login);
      exit(1);
   }

   /* Change password */
   sprintf(new_ep, "*:%s", old_ep);
   create_web_account(login, new_ep, nobody.pw_uid, nobody.pw_gid);
}

/* Recursively remove a directory and everything under it */
void
recursive_rmdir(path)
   char *path;
{
   DIR *dp;
   struct dirent *fp;
   char buff[MAXPATHLEN];
   struct stat st;

   if ((dp = opendir(path)) == NULL) {
      perror(path);
      return;
   }
   while ((fp = readdir(dp)) != NULL) {
      if (!strcmp(fp->d_name, ".") || !strcmp(fp->d_name, ".."))
         continue;
      sprintf(buff, "%s/%s", path, fp->d_name);
      if (stat(buff, &st)) {
         perror(buff);
         continue;
      }
      if (st.st_mode & S_IFDIR) {
         recursive_rmdir(buff);
      } else {
         if (unlink(buff))
            perror(buff);
      }
   }
   closedir(dp); 
   if (rmdir(path))
      perror(path);
}

/* Remove a web account */
void
rmuser(login)
   char *login; /* IN: Web login */
{
   FILE *fp, *tmp;
   char *passfile = get_conf_param("passfile", PASS_FILE);
   uid_t uid;
   gid_t gid;
   char *userfile = get_userfile(login, &uid, &gid);
   char  tmpname[MAXPATHLEN];
   char  buff[MAXPATHLEN];
   char *home = get_homedir(login);
   char *epass;

   /* Only root and usradm may do this */
   if (getuid() != 0 && getuid() != usradm.pw_uid) {
      printf("webuser: Permission denied\n");
      exit(1);
   }

   /* See if account exists */
   epass = get_web_password(login);
   if (!epass) {
      printf("webuser: no such login '%s'\n", login);
      exit(1);
   }

   /* Prompt for verification */
   printf("Delete all files for %s [no]? ", login);
   fgets(buff, 80, stdin);
   if (tolower(buff[0]) != 'y') {
      printf("webuser: Deletion aborted\n");
      exit(1);
   }

   /* REMOVE PASSWORD FILE ENTRY... */

   if (strcmp(passfile, userfile)) {

      /* Open new .htpasswd file */
      sprintf(tmpname, "%s.%d", passfile, (int)getpid());
      if ((tmp = fopen(tmpname, "w"))==NULL) {
         sprintf(buff, "fopen %s", tmpname);
         perror(buff);
         exit(1);
      }
   
      /* Copy all of passfile except any line(s) for specified login */
      if ((fp = fopen(passfile, "r"))!=NULL) {
         char str[256];
         int  len = strlen(login)+1;
   
         sprintf(str, "%s:", login);
   
         while (fgets(buff, sizeof(buff), fp)) {
            if (strncmp(buff, str, len))
               fprintf(tmp, "%s", buff);
         }
         fclose(fp);
      }
   
      /* Commit the changes */
      fclose(tmp);
      if (rename(tmpname, passfile)) {
         perror("rename");
         exit(1);
      }
      chown(passfile, cfadm.pw_uid, cfadm.pw_gid);
      chmod(passfile, 0644);
   }

   /* If using a combined userfile, remove the entry from there */
   if (strncmp(userfile, home, strlen(home))) {
      /* Open new userfile */
      sprintf(tmpname, "%s.%d", userfile, (int)getpid());
      if ((tmp = fopen(tmpname, "w"))==NULL) {
         sprintf(buff, "fopen %s", tmpname);
         perror(buff);
         exit(1);
      }
   
      /* Find login in user file */
      if ((fp = fopen(userfile, "r"))!=NULL) {
         char str[256];
         int  len = strlen(login)+1;
   
         sprintf(str, "%s:", login);
   
         while (fgets(buff, sizeof(buff), fp)) {
            if (strncmp(buff, str, len))
               fprintf(tmp, "%s", buff);
         }
         fclose(fp);
      }
   
      /* Commit the changes */
      fclose(tmp);
      if (rename(tmpname, userfile)) {
         perror("rename");
         exit(1);
      }
   
      chown(userfile, uid, gid);
      chmod(userfile, 0644);
   }

   /* Remove the web home directory */
   recursive_rmdir(home);

   /* If there is no Unix account of the same name */
   if (!getpwnam(login)) {
      /* remove the partfile dir */
      recursive_rmdir(get_partdir(login));
   }
}

time_t          /* OUT: timestamp of most recent file in user's partfile dir */
get_last_change_time(login)
   char *login; /* IN : login to check */
{
   DIR *dp;
   struct dirent *fp;
   struct stat st;
   char file[MAXPATHLEN];
   time_t last = 0;
   char *partdir = get_partdir(login);
 
   if ((dp = opendir(partdir)) == NULL)
      return 0;
   while ((fp = readdir(dp)) != NULL) {
      sprintf(file, "%s/%s", partdir, fp->d_name);
      if (stat(file, &st) || !(st.st_mode & S_IFREG))
         continue;
      if (st.st_mtime > last)
         last = st.st_mtime;
   }
   closedir(dp); 
   return last;
}

/* List all web accounts */
void
listall() 
{
   char *passfile = get_conf_param("passfile", PASS_FILE);
   FILE *fp;
   char buff[256], fullname[256], email[256], tstr[40];
   time_t tm, ctm;

   printf("S Login      Date   Email                               Full name\n");

   /* For each entry in the web password file... */
   if ((fp = fopen(passfile, "r"))!=NULL) {
      char *p;

      while (fgets(buff, sizeof(buff), fp)) {
         p = strchr(buff, ':');
         if (!p) continue;
         (*p)='\0';

         get_user_info(buff, fullname, email);
         tm = get_last_change_time(buff);
         strcpy(tstr, ((tm)? ctime(&tm)+4 : "--- --"));
         tstr[6]='\0';
         time(&ctm);
         if (strncmp(tstr+16, ctime(&ctm)+20, 4)) {
            strncpy(tstr, tstr+16, 4);
            tstr[4]='\0';
         }
        
         printf("%c %-10s %-6s %-35s %s\n", ((p[1]=='*')? 'D':'-'),
          buff, tstr, email, fullname);
      }
      fclose(fp);
   } 
}

void
show(login)
   char *login; /* IN: Web login */
{
   char *epass; /* encrypted web password */
   char  fullname[256], email[256];
   time_t tm;

   epass = get_web_password(login);
   if (!epass) {
      printf("webuser: no such login '%s'\n", login);
      exit(1);
   }
   printf("Web login     : %s\n", login);
   printf("Status        : %s\n", 
    (!strncmp(epass, "*:", 2))? "Disabled" : "Active");

#ifdef HAVE_DBM_OPEN
   if (!strcmp(get_conf_param("userdbm",USERDBM),"true")) {
      DBM *db;
      char *userfile = get_userfile(login, NULL, NULL);
      if ((db = dbm_open(userfile, O_RDONLY, 0644))!=NULL) {
         datum dkey, dval; 
         /* XXX need to alphabetize this list when printing it 
          * either A) malloc array, fill in, qsort, and walk it
          *    or  B) 2 nested for loops and keep the best next key in 1st one
          */
         for (dkey=dbm_firstkey(db); dkey.dptr!=NULL; dkey=dbm_nextkey(db)) {
            dbm_fetch(db, dkey);
            dval = dbm_fetch(db, dkey);
            dkey.dptr[0] = toupper(dkey.dptr[0]);
            printf("%-14s: %s\n", dkey.dptr, dval.dptr);
         }
         dbm_close(db);
      }
   } else {
#endif /* HAVE_DBM_OPEN */
   get_user_info(login, fullname, email);
   printf("Full name     : %s\n", fullname); 
   printf("Email address : %s\n", email);
#ifdef HAVE_DBM_OPEN
   }
#endif /* HAVE_DBM_OPEN */

   tm = get_last_change_time(login);
   printf("Last read time: %s", ((tm)? ctime(&tm) : "never\n"));
}

void
chpass(login)
   char *login; /* IN: Web login */
{
   char *p, *old_ep;
   char newpass1[256];
   char newpass2[256];

   /* If root or usradm, then allow arbitrary login to be specified */
   if (login && strcmp(login, getlogin()) 
    && getuid()!=0 && getuid()!=usradm.pw_uid) {
      printf("webuser: Permission denied\n");
      exit(1);
   }

   /* Make sure a current account exists */
   old_ep = get_web_password(login);
   if (!old_ep) {
      printf("webuser: no such login '%s'\n", login);
      exit(1);
   }

   /* Get new password */
   p = getpass("New password:");
   strcpy(newpass1, crypt(p, old_ep));
   memset(p, 0, strlen(p));

   p = getpass("Retype new password:");
   strcpy(newpass2, crypt(p, old_ep));
   memset(p, 0, strlen(p));

   if (strcmp(newpass1, newpass2)) {
      printf("webuser: password mismatch\n");
      exit(1);
   }

   /* Change password */
   create_web_account(login, newpass1, nobody.pw_uid, nobody.pw_gid);
}

int
main(argc, argv)
   int    argc;
   char **argv;
{
   struct passwd *pw;
   char *login=NULL;
   char *options="dehlprsuv";    /* Command-line options */
   int i;
   char cmd = 'u';

   while ((i = getopt(argc, argv, options)) != -1) {
      switch(i) {
         case 'd': cmd = i; break;
         case 'e': cmd = i; break;
         case 'r': cmd = i; break;
         case 'p': cmd = i; break;
         case 's': cmd = i; break;
         case 'u': cmd = i; break;
         case 'l': cmd = i; break;
         default:  usage();
      }
   }
   argc -= optind;
   argv += optind;

   if (argc>0) {
      login=argv[0];
      argc--;
      argv++;
   }
   if (argc>0)
      usage();

   if (geteuid()) {
      printf("This program must be installed setuid root\n");
      exit(0);
   }

   /* Read in yapp configuration file */
   get_yapp_conf();

   /* Look up some standard logins to get UIDs for file permissions */
   if ((pw = getpwnam(get_conf_param("nobody", NOBODY)))==NULL) {
      perror("nobody");
      exit(1);
   }
   memcpy(&nobody, pw, sizeof(cfadm)); /* save uid/gid */
   if ((pw = getpwnam(get_conf_param("cfadm", CFADM)))==NULL) {
      perror("cfadm");
      exit(1);
   }
   memcpy(&cfadm, pw, sizeof(cfadm)); /* save uid/gid */
   if ((pw = getpwnam(get_conf_param("usradm", USRADM)))==NULL) {
      usradm.pw_uid = 0;
      usradm.pw_gid = 0;
   } else
      memcpy(&usradm, pw, sizeof(usradm)); /* save uid/gid */

   if (!login)
      login = getlogin();
   if (!login) {
      perror("getlogin");
      exit(1);
   }

   switch(cmd) {
   case 'd': disable(login); break;
   case 'e': enable(login);  break;
   case 'l': listall();      break;
   case 'p': chpass(login);  break;
   case 'r': rmuser(login);  break;
   case 's': show(login);    break;
   case 'u': update(login);  break;
   }

   exit(0);
}
