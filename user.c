/* $Id: user.c,v 1.29 1998/02/13 10:56:20 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <pwd.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h> 
#endif
#ifdef HAVE_SYS_FILE_H
# include <sys/file.h> /* for X_OK */
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <signal.h>
#include <ctype.h>
#include <errno.h> /* for debugging only */
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "macro.h"
#include "files.h"
#include "lib.h"
#include "xalloc.h"
#include "user.h"
#include "system.h"
#include "license.h" /* for get_conf_param() */
#include "driver.h"  /* for endbbs() */
#include "main.h"    /* for wfputs() */
#include "sep.h"     /* for fitemsep() */

#ifdef HAVE_INFORMIX_OPEN
#include "database.h"
#endif HAVE_INFORMIX_OPEN

#ifdef SHORT_RAND
#define RAND  myrand
#define SRAND mysrand

#define A 0x41C64E6D /* 1103515245L */
#define B 0x3039     /*      12345L */
u_int32 myseed=0;

void
mysrand(seed)
   u_int32 seed;
{
   myseed = seed;
}

u_int32
myrand()
{
   myseed = (A*myseed + B) & 0x7FFFFFFF;
   return myseed;
}
#else
#define RAND  rand
#define SRAND srand
#endif

static time_t listtime;
static int    listsize;

int 
get_nobody_uid()
{
   static int nobody_uid = 0;
   
   /* Resolve uid for "nobody" if needed */
   if (!nobody_uid) { 
      struct passwd *nuid = NULL;
      if ((nuid = getpwnam(get_conf_param("nobody",NOBODY))) != NULL)
         nobody_uid = nuid->pw_uid;
   }
   return nobody_uid;
}

char *
get_sysop_login()
{
   struct passwd *pwd = NULL;
   static char cfadm[20], *log=NULL;

   if (log)
      return log;

   pwd = getpwuid(geteuid());
   if (pwd) 
      strcpy(cfadm, pwd->pw_name);
   else
      strcpy(cfadm, "cfadm");

   if (getuid()==get_nobody_uid()) 
      strcpy(cfadm, get_conf_param("sysop", cfadm));
   log = cfadm;
   return log;
}

#ifdef WWW
char *
get_userfile(who, suid)
   char *who;      /* IN: login */
   int  *suid;     /* OUT: suid mode */
{
static char buff[256];
   char *file = get_conf_param("userfile", USER_FILE);
#ifdef HAVE_DBM_OPEN
   if (match(get_conf_param("userdbm",USERDBM),"true")) {
      sprintf(buff, "%s/%c/%c/%s/%s", get_conf_param("userhome", USER_HOME),
       tolower(who[0]), tolower(who[1]), who, who);
      file = buff;
      (*suid) = SL_USER;
      return file;
   }
#endif
   if (file[0]=='~' && file[1]=='/') {
      sprintf(buff, "%s/%c/%c/%s/%s", get_conf_param("userhome", USER_HOME),
       tolower(who[0]), tolower(who[1]), who, file+2);
      file = buff;
      (*suid) = SL_USER;
   } else
      (*suid) = SL_OWNER;
   return file;
}

/*
 * A "local" user is one whose identity exists only inside Yapp,
 * rather than having their own Unix account.
 */
static int /* returns 1 if found, 0 if not */
get_local_user(uid, login, fullname, home, email)
   int  *uid;      /* UNUSED: user ID (or NULL) */
   char *login;    /* IN: login to find */
   char *fullname; /* OUT: full name (or NULL) */
   char *home;     /* OUT: home directory to use (or NULL) */
   char *email;    /* OUT: email address (or NULL) */
{
   FILE *fp;
   char buff[256], **field, *filename;
   int suid;

#if 0
   /*
    * Real Unix users always use their own home directory, and
    * never count as a "local" user.  However, we must allow
    * 'participants' command (etc) to find local users!  If this
    * is left in, then "participants <weblogin>" fails.  When
    * taken out, thaler uses the web directory.
    */
   if (getuid() != get_nobody_uid())
      return 0;
#endif

   /* Retrieve information from user file (could be in user dir) */
   if (debug & DB_USER)
      printf("called get_local_user(%d,\"%s\")\n", (uid)?*uid:0, login);

   filename = get_userfile(login, &suid);

#ifdef HAVE_DBM_OPEN
   if (match(get_conf_param("userdbm",USERDBM),"true")) {
      struct stat st;
      char buff[MAX_LINE_LENGTH];

      sprintf(buff, "%s.db", filename);
      if (stat(buff, &st))
         return 0;

      if (home) {
         sprintf(home, "%s/%s", get_conf_param("userhome", USER_HOME), login);
         if (access(home,X_OK))
            sprintf(home, "%s/%c/%c/%s", get_conf_param("userhome", USER_HOME), tolower(login[0]), tolower(login[1]), login);
      }
      if (fullname)
         strcpy(fullname, get_dbm(filename, "fullname", SL_USER));
      if (email)
         strcpy(email, get_dbm(filename, "email", SL_USER));
      return 1;
   }
#endif

#ifdef HAVE_INFORMIX_OPEN
   if (match(get_conf_param("useinformix","false"),"true")) {
      if (home) {
         sprintf(home, "%s/%s", get_conf_param("userhome", USER_HOME), login);
         if (access(home,X_OK))
            sprintf(home, "%s/%c/%c/%s", get_conf_param("userhome", USER_HOME), tolower(login[0]), tolower(login[1]), login);
      }
      if (fullname)
         strcpy(fullname, get_informix(F_LOGIN,login,get_conf_param(F_FULLNAME,F_FULLNAME),SL_OWNER));
      if (email)
         strcpy(email, get_informix(F_LOGIN,login,get_conf_param(F_EMAIL, F_EMAIL),SL_OWNER));
   }
#endif /*HAVE_INFORMIX_OPEN */

   if (suid==SL_USER) {
      /* Here, it's okay for it not to exist, since that just means it's
       * not a web user
       */
      if ((fp = smopenr(filename, O_R|O_SILENT))==NULL)
         return 0;
   } else {
      if ((fp = mopen(filename, O_R))==NULL)
         return 0;
   }
   while (ngets(buff,fp)) {
      field = explode(buff, ":", 0);
      if (!strcmp(field[0], login)) {
         if (fullname)
            strcpy(fullname, field[1]);
         if (email)
            strcpy(email, field[2]);
         if (home) {
            sprintf(home, "%s/%s", get_conf_param("userhome", USER_HOME), login);
            if (access(home,X_OK))
               sprintf(home, "%s/%c/%c/%s", get_conf_param("userhome", USER_HOME), tolower(login[0]), tolower(login[1]), login);
         }
         xfree_array(field);
         if (suid==SL_USER)
            smclose(fp);
         else
            mclose(fp);
         if (debug & DB_USER)
            printf("get_local_user found (%d,\"%s\",\"%s\")\n", (uid)?*uid:0, login, 
             fullname);
         return 1;
      }
      xfree_array(field);
   }
   if (suid==SL_USER)
      smclose(fp);
   else
      mclose(fp);

   return 0;
}
#endif

/* getuid=nobody, uid=nobody, login there: search WWW by login (%e in rsep),
         also when nobody authenticates
   getuid=nobody, uid != 0, login there: search Unix by uid  (%e in rsep)
                  uid=0, login there: participants with login in ulist
                  uid!=0, login=0:    participants with uid in ulist
                  uid=0,  login=0:    init() to get current user
 */

/* Here's the cases in the order they're covered:
      getuid=nobody,  uid=0, login=0          -> get current WWW user info
      getuid=nobody,  uid=nobody, login=0     -> get current WWW user info
      getuid!=nobody, uid=0, login=0          -> get current Unix user info
                      uid=0, login there      -> search Unix, then WWW by login
                      uid != 0, login=0       -> search Unix by uid
                      uid=nobody, login there -> search WWW by login
                      uid other, login there  -> grab extra info for Unix user
 */

/* Get the real user information */
int /* returns 1 if found, 0 if not */
get_user(uid, login, fullname, home, email)
   int  *uid;      /* IN: user ID or 0 for lookup by login */
   char *login;    /* IN: if uid is 0, login is name to find */
   char *fullname;
   char *home;
   char *email;
{
   char *tmp, **namestr;
   struct passwd *pwd = NULL;
#if 0
   struct passwd pw;
#endif

   if (debug & DB_USER)
      printf("called get_user(%d,\"%s\")\n", *uid, login);

   /* Case 1: Get current WWW user 
      getuid=nobody,  uid=0, login=0          -> get current WWW user info
      getuid=nobody,  uid=nobody, login=0     -> get current WWW user info
    */
   if (getuid()==get_nobody_uid() && (!*uid || *uid==get_nobody_uid())
    && !login[0] && (tmp=getenv("REMOTE_USER")) && tmp[0]) {
      *uid = get_nobody_uid();
      strcpy(login, tmp);
      return get_local_user(uid, login, fullname, home, email);
   }

#if 0
   /* First check if login is a local user */
   if (login[0] && get_local_user(uid, login, fullname, home, email))
      return 1;

   if (debug & DB_USER)
      printf("didn't get local user\n");
#endif

   /* If blank spec, get Unix user
    * getuid!=nobody, uid=0, login=0          -> get current Unix user info
    */
   if ((*uid)==0 && !login[0]) {
      (*uid) = getuid();

      tmp = getlogin();
#ifdef HAVE_CUSERID
      if (!tmp)
         tmp = cuserid(NULL);
#endif

      /*
       * For some really odd reason, I have observed Solaris reporting
       * the login (from both getlogin() and cuserid()) as "LOGIN".
       * Let's just pretend we got a NULL returned instead.
       */
      if (tmp && !strcmp(tmp, "LOGIN"))
         tmp = NULL;

      if (tmp) {
         if (!(pwd = getpwnam(tmp))) {
            if (debug & DB_USER)
               printf("getpwnam failed to get anything for login %s\n", tmp);
            return 0;
         }

         /* Verify that UID = Login's UID */
         if (pwd->pw_uid == *uid)
            strcpy(login, tmp);
      }
   } else if ((*uid)==0) { /* search by login */
    /* uid=0, login there      -> search Unix, then WWW by login */
      if (!(pwd = getpwnam(login))) /* search Unix first then WWW */
         return get_local_user(uid, login, fullname, home, email); 
      *uid = pwd->pw_uid;
   }

   /* Ok, now *uid is valid */
   if (!login[0]) {
      /* uid != 0, login=0       -> search Unix by uid */
      if (!(pwd = getpwuid(*uid)))
         return 0;
      strcpy(login, pwd->pw_name);
   }
   if (!pwd)
      pwd = getpwuid(*uid);
   if (debug & DB_USER)
      printf("get_user saw (%d,\"%s\",\"%s\")\n", *uid, login, fullname);

#if 0
   /* If "nobody", change to REMOTE_USER */
   /* 4/4: Problem: an email lookup fails when searching WWW by:
    *         getuid=nobody, uid=nobody, login=there
    *      and gets the current user instead.
    * I'm not even sure what this section is for.  Removed 4/4 to 
    * see if anything breaks.
    */
   if (*uid == get_nobody_uid()
    && (!strcmp(login, get_conf_param("nobody",NOBODY)) 
     || !strcmp(pwd->pw_name, get_conf_param("nobody",NOBODY)))
    && (tmp=getenv("REMOTE_USER")) && tmp[0]) {
      pwd = &pw;
      strcpy(login, tmp);
      return get_local_user(uid, login, fullname, home, email);
/*
      return get_user(0, login, fullname, home, email);
*/
   }
#endif

   /* uid=nobody, login there -> search WWW by login */
   if (*uid == get_nobody_uid() && strcmp(login, get_conf_param("nobody",NOBODY)))
      return get_local_user(uid, login, fullname, home, email);

   /* uid other, login there  -> grab extra info for Unix user */
   if (!pwd)
      return 0;
   strcpy(home,    pwd->pw_dir);

   namestr=explode(pwd->pw_gecos,expand("gecos",DM_VAR), 0);
   strcpy(fullname, (xsizeof(namestr)>0)? namestr[0] : "Unknown");
   xfree_array(namestr);
   if (debug & DB_USER)
      printf("get_user found (%d,\"%s\",\"%s\")\n", *uid, login, fullname);
   sprintf(email, "%s@%s", login, hostname);

   return 1;
}

/*
 * The slow way: search through userfile.   This only works if
 * userdbm is false.  
 */
char * /* returns 1 if found, 0 if not */
email2login(email)
   char *email;
{
   char *userfile;
   int   suid;
   FILE *fp;
   char buff[MAX_LINE_LENGTH];
   static char login[MAX_LINE_LENGTH];
   char **partsA;

#ifdef HAVE_DBM_OPEN  
   if (match(get_conf_param("userdbm",USERDBM),"true")) {
      /*
       * If it's true, then we don't have a good way to verify registered 
       * users at the moment.  In the future, we might want to have a dbm 
       * file indexed by email address.
       * XXX
       */
      return NULL;
   }
#endif

#ifdef HAVE_INFORMIX_OPEN
   if (match(get_conf_param("useinformix","false"),"true")) {
      return get_informix(F_EMAIL,email,F_LOGIN,SL_OWNER);
   }

#endif

   /* Make sure there's a single user file */
   login[0]='\0';
   userfile = get_userfile(login, &suid);
   if (suid==SL_USER)
      return NULL; /* No good way to do this either */
   if ((fp = mopen(userfile, O_R))==NULL)
      return NULL;

   partsA = explode(email, "@", 1);

   /* Search for email address */
   while (ngets(buff,fp)) {
      char **field  = explode(buff, ":", 0);
      char **partsB = explode(field[2], "@", 1);

      if (!strcmp(partsA[0], partsB[0]) 
       && strlen(partsA[1]) >= strlen(partsB[1])
       && !strcmp(partsA[1]+strlen(partsA[1])-strlen(partsB[1]), partsB[1])) {
         strcpy(login, field[0]);
         xfree_array(field);
         xfree_array(partsA);
         xfree_array(partsB);
         mclose(fp);
         return login;
      }
      xfree_array(partsB);
      xfree_array(field);
   }
   xfree_array(partsA);
   mclose(fp);
   return NULL;
}

#ifdef WWW
/*  This section allows secure WWW POST'ing in Yapp.  From a secure GET
 *  script, we generate a form with a HIDDEN ticket, which is
 *  {time, login, ( password, time )_DES } 
 *
 *  When the post comes in, we will recrypt the ticket and compare for
 *  validity.
 *
 *  ticket with 2 arguments outputs the ticket generated
 */

#define TIMEDELTA 60*30  /* 30 min lifetime */

static unsigned char itoa64[] =         /* 0 ... 63 => ascii - 64 */
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void
to64(s, v, n)
  register char *s;
  register long v;
  register int n;
{
    while (--n >= 0) {
        *s++ = itoa64[v&0x3f];
        v >>= 6;
    }
}

#ifdef INCLUDE_EXTRA_COMMANDS
/* Given a login, return the user's encrypted password */
static char *
get_passwd(who)
   char *who;
{
static char line[256], *w;
   FILE *fp;

   if (!who || !*who)
      return NULL;

#ifdef HAVE_DBM_OPEN
   if (match(get_conf_param("userdbm",USERDBM),"true")
    && !strcmp(get_conf_param("userfile",USER_FILE),
               get_conf_param("passfile",PASS_FILE))) {
      int suid;
      char *userfile = get_userfile(login, &suid);
      return get_dbm(userfile, "passwd", SL_USER);
   }
#endif /* HAVE_DBM_OPEN */

   /* Check local user file */
   if ((fp = mopen(get_conf_param("passfile", PASS_FILE), O_R))==NULL) {
      error("opening", " user file");
      exit(1);
   }
   while (ngets(line,fp)) {
     if (line[0]=='#' || !line[0]) 
         continue;
     if ((w = strchr(line, ':')) != NULL) {
        *w = '\0';
        if (!strcmp(who,line)) {
            mclose(fp);
            return w+1;
        }
     }
   }
   mclose(fp);
   return NULL;
}
#endif

/*
 * Generate a ticket that proves that the key (i.e. encrypted password)
 * was known at time tm.  This assumes that the PASS_FILE file is only
 * readable by cfadm.  Used by %{ticket} macro, and license file.
 */
char *
make_ticket(key, tm)
   char *key;
   int   tm;
{
#ifdef SHORT_CRYPT
   static char buff[MAX_LINE_LENGTH];
   char *p;
   int  len, i;
#endif
   char salt[3];

   salt[2]='\0';
   (void)SRAND(tm);
   to64(&salt[0],RAND(),2);
   salt[2]='\0';
#ifdef SHORT_CRYPT
   len = strlen(key);
   p = key;
   strcpy(buff, crypt(p, salt));
   p += 8;
   while (p-key < len) {
      strcat(buff, crypt(p, salt)+2);
      p += 8;
   }
   return buff;
#else
   return crypt(key,salt);
#endif
}

#ifdef INCLUDE_EXTRA_COMMANDS
/* The %{ticket} macro should expand to:
 *   ticket(0, who);
 */

char * /* RETURNS: ticket or NULL on error */
get_ticket(tm, who)
   int   tm; /* 0 = now, or timestamp */
   char *who; /* login */
{
    char *password = NULL;
    time_t rtm;
static char buff[256];

    /* Validate timestamp */
    time(&rtm);
    if (!tm)
       tm = rtm;
    if (tm > rtm || tm < rtm-TIMEDELTA) {
       return NULL;
    }

    /* Validate login */
    password = get_passwd(who);
    if (!password) {
/*printf("Failed to find local password for %s\n", who);*/
       return NULL;
    }

    /* Output ticket */
    sprintf(buff, "%d:%s:%s", tm, who, make_ticket(password, tm));
    return buff;
}

static int  /* RETURNS: 1 on success, 0 on failure */
do_authenticate(ticket)
   char *ticket;
{
   time_t tm;
   char **field;
   char *ticket1;
   int ret;

   /* Only "nobody" can authenticate */
   if (!(status & S_NOAUTH)) {
      printf("You are already authenticated.\n");
      return 0;
   }
   
   field = explode(ticket, ":", 0);
   if (xsizeof(field) != 3)
      ret = 0;
   else {
      tm = atoi(field[0]); 
      ticket1 = get_ticket(tm, field[1]);
      if (ticket1 && !strcmp(ticket, ticket1)) {
         strcpy(login, field[1]);
         status &= ~S_NOAUTH;
         ret = 1;
      } else {
/*printf("ticket comparison failed !%s!%s!\n", ticket, ticket1);*/
         ret = 0;
      }
   }
   xfree_array(field);
   return ret;
}
#endif /* INCLUDE_EXTRA_COMMANDS */

void
get_partdir(partdir, login)
   char *partdir;
   char *login;
{
   char buff[MAX_LINE_LENGTH];

   strcpy(buff, get_conf_param("partdir","work"));
   if (match(buff, "work"))
      strcpy(partdir, work);
   else {
      struct stat *sb;

      sprintf(partdir, "%s/%c/%c/%s", buff,
       tolower(login[0]), tolower(login[1]), login);
      mkdir_all(partdir,0700);
   }
}

/*
 * Assuming login is already set, do everything else necessary to initialize
 * stuff for the user.  This is usually only called once, except when
 * nobody "authenticates" it is called again.  When this happens,
 * init() has already set the user to be the Unix nobody.  We have
 * just changed login to its new value, uid=nobody, getuid=nobody,
 * and we want to get the info for the new account.
 */
void
login_user()
{
   char buff[MAX_LINE_LENGTH], *mail;
static old_flags = 0, first_call=1; /* preserve original flags throughout */

   if (first_call) {
      first_call = 0;
      old_flags = (flags & (O_READONLY|O_OBSERVE));
   }

   /* Reset home */
   if (!get_user(&uid, login, st_glob.fullname, home, email)) {
      error("reading ","user entry");
      endbbs(2);
   }  

   /* Set mailbox macro */
   mail = getenv("MAIL");
   if (!mail) {
      sprintf(buff, "%s/%s", get_conf_param("maildir", MAILDIR), login);
      mail = buff;
   }
   def_macro("mailbox", DM_VAR, mail);

   /* Set global variable "work" */
   if (access(home,X_OK)) {
      if (strcmp(login, get_conf_param("nobody",NOBODY)))
         error("accessing ",home);
   
      /* If can't access home directory, continue as an observer */
      flags |= O_READONLY|O_OBSERVE;
   } else { 
      /* restore original readonly/observe flag settings */
      flags = (flags & ~(O_READONLY|O_OBSERVE)) | old_flags;
   }
   sprintf(work,"%s/.cfdir",home);
   if (access(work,X_OK)) 
      strcpy(work, home);
   strcpy(buff,work);

   /* Set partdir (must be done before refresh_list() below) */
   get_partdir(partdir, login);
      
   /* Read in PARTDIR/.cflist */
   listtime = 0;
   refresh_list();
      
   /* Execute user's cfonce file */
   source(work,".cfonce", 0, SL_USER);

#ifdef INCLUDE_EXTRA_COMMANDS
   /* If "work" changed above, do the cflist again */
   if (strcmp(work,buff)) { /* for cfdir command */
      source(work,".cfonce", 0, SL_USER);
      strcpy(buff,work);
      refresh_list();
   }
#endif /* INCLUDE_EXTRA_COMMANDS */
   
   /* Reset fullname -- why??? */
   if (!get_user(&uid, login, st_glob.fullname, home, email)) {
      error("reading ","user entry");
      endbbs(2);
   }  
}

int
partfile_perm()
{
   static int ret = -1;

   if (ret<0) 
      ret=(match(get_conf_param("partdir","work"),"work"))? SL_USER : SL_OWNER;
   return ret;
}

void 
add_password(login, password, fp)
   char *login;
   char *password;
   FILE *fp;    /* Password file */
{     
   char *cpw, salt[3];
   
   (void)SRAND((int)time((time_t *)NULL));
   to64(&salt[0],RAND(),2);
   salt[2]='\0';
   cpw = crypt(password,salt);
   fprintf(fp, "%s:%s\n", login, cpw);
{
char buff[256];
sprintf(buff, " !%s!", salt);
error("warning", buff);
}
}

/* Sanity check on fullname */
int /* 1 if sane, 0 if illegal */
sane_fullname(str)
   char *str;
{
   char *p;
   for (p=str; *p; p++)
      if (*p==':' || *p=='|' || !isprint(*p)) {
         return 0;
      }
   return 1;
}

/* Sanity check on email */
int
sane_email(str)
   char *str;
{
   char *p;
   for (p=str; *p; p++)
      if (*p==':' || *p=='|' || !isprint(*p)) {
         return 0;
      }
   return 1;
}

/* others contains a list of variables to save to the dbm file,
 * excluding fullname, email, passwd 
 */
static int
save_user_info(newlogin, newname, newemail, newpass, others)
   char *newlogin;
   char *newname;
   char *newemail;
   char *newpass;
   char *others;   /* IN: other vars to save (space-sep), or NULL for none */
{
   FILE *fp, *out=stdout;
   char cmd[MAX_LINE_LENGTH], *userfilename;
   int suid;
   userfilename = get_userfile(newlogin, &suid);

#ifdef HAVE_DBM_OPEN
   if (match(get_conf_param("userdbm",USERDBM),"true")) {
      if (!save_dbm(userfilename, "fullname", newname, SL_USER)
       || !save_dbm(userfilename, "email", newemail, SL_USER)
       || (!strcmp(get_conf_param("userfile",USER_FILE),
                   get_conf_param("passfile",PASS_FILE))
        && !save_dbm(userfilename, "passwd", newpass, SL_USER))
      ) {
         sprintf(cmd,"ERROR: Couldn't modify user file %s \n",userfilename);
         wfputs(cmd, out);
         return 0;
      }

      /* step through others and save those, except fullname/email/passwd */
      if (others) {
         char **list = explode(others, " ", 1);
         int n = xsizeof(list);
         register int i;
         for (i=0; i<n; i++) {
            char *p = expand(list[i], DM_VAR);
            if (p && p[0] && !save_dbm(userfilename, list[i], p, SL_USER)) {
               sprintf(cmd,"ERROR: Couldn't modify user file %s \n",
                userfilename);
               wfputs(cmd, out);
               xfree_array(list);
               return 0;
            }
         }
         xfree_array(list);
      }

   } else {
#endif /* HAVE_DBM_OPEN */

      if (suid==SL_USER)
         fp = smopenw(userfilename, O_A);
      else
         fp = mopen(userfilename, O_A);
      if (fp==NULL) {
         error("opening ",userfilename);
         sprintf(cmd,"ERROR: Couldn't modify user file %s \n",userfilename);
         wfputs(cmd, out);
         return 0;
      }  
      fprintf(fp, "%s:%s:%s\n", newlogin, newname, newemail);
      if (suid==SL_USER)
         smclose(fp);
      else
         mclose(fp);

#ifdef HAVE_DBM_OPEN
   }
#endif

   /* Add password to passfile */
   if ((fp = mopen( get_conf_param("passfile", PASS_FILE), O_A))==NULL) {
      error("opening", " passwd file");
      exit(1);
   }  

   /* Encrypt password and save in file */
   add_password(newlogin, newpass, fp);
   mclose(fp);

   return 1;
}

/*
 * Generate a random 8-character password in static buffer
 * make sure it's not guessable by someone knowing what time the
 * account was created.
 */
char *
random_password()
{
   static char buff[10];
   int seed;
   struct timeval tv;
   
   gettimeofday(&tv, NULL);
   seed = (getpid() << 16) | (tv.tv_usec & 0xFFFF);
   srandom(seed);
   to64(buff,   random(), 4);
   to64(buff+4, random(), 4);
   buff[8]='\0';

   return buff;
}

/* Send email to current user with their password */
int                  /* OUT: 1 on success, 0 on failure             */
email_password(pass)
   char *pass;       /* IN : plaintext password to include in email */
{
   char buff[MAX_LINE_LENGTH], filename[MAX_LINE_LENGTH], 
        filein[MAX_LINE_LENGTH], *str;
   FILE *fin, *fout;

   /* Compose email with their password */
   sprintf(filein  ,"%s/templates/newuser.email", expand("wwwdir", DM_VAR));
   sprintf(filename,"/tmp/notify.%d", getpid());
   def_macro("password", DM_VAR, pass);
   if ((fout=mopen(filename,O_W))==NULL) {
      error("opening ", filename);
      return 0;
   }
   if ((fin =mopen(filein  ,O_R))==NULL) {
      error("opening ", filein);
      return 0;
   }
   while ((str= xgets(fin, stdin_stack_top)) != NULL) {
      fitemsep(fout,str,0);
      xfree_string(str);
   }
   mclose(fin);
   mclose(fout);
   undef_name("password");

   /* Send it off */
   sprintf(buff, "%s -t < %s", get_conf_param("sendmail",SENDMAIL),
    filename);
   unix_cmd(buff);
   unlink(filename);
   return 1;
}

/* command: newuser login  - just checks for validity */
/* This allows an unauthenticated user to create a new login */
/* usage: newuser login newpass newpasstoo newname newemail */
/* cfadm gets prompts, anyone else except 'nobody' gets same web account */
/* when verifyemail is false, userdbm is true, syntax will be:
 * newuser newlogin newpass newpasstoo newname newemail extras
 */
int
newuser(argc, argv)
   int argc;
   char **argv;
{
   struct stat st; 
   char *p, *others = NULL;
   char newlogin[MAX_LINE_LENGTH];
   char newpass1[MAX_LINE_LENGTH];
   char newpass2[MAX_LINE_LENGTH];
   char newname[MAX_LINE_LENGTH];
   char newemail[MAX_LINE_LENGTH];
   char homedir[MAX_LINE_LENGTH];
   FILE *out=stdout;
   int cpid, wpid;
   char cmd[MAX_LINE_LENGTH];
   int normaluser;

   if (status & S_EXECUTE)
      out = NULL;

   normaluser = (getuid() == get_nobody_uid() || getuid()==geteuid())? 0 : 1;

/* #ifdef XXX */
/*
 * 8/10/96 put this code back in because we currently only support running
 *         newuser from the nobody account
 */
   /* Only the "nobody" account can do newuser */
   if (getuid() != get_nobody_uid()) {
      wfputs("You already have a login.\n", out);
      return 1;
   }
/* #endif XXX */

   /* Get new login */
   if (normaluser) {
      strcpy(newlogin, login);
      strcpy(newname,  fullname);
      strcpy(newemail, email);
   } else {
      if (argc>1)
         strcpy(newlogin, argv[1]);
      else {
         printf("Choose a login: ");
         ngets(newlogin, st_glob.inp);
      }

      /* Sanity check on login */
      if (strlen(newlogin)<2) {
        wfputs("Login must be at least 2 characters long\n", out);
        return 1;
      }
   for (p=newlogin; *p; p++)
         if (!isalnum(*p)) {
           sprintf(cmd, "Illegal character '%c' (ascii %d) in login.  Please choose another login.\n", *p, *p);
           wfputs(cmd, out);
           return 1;
         }
   
      /* Check to see if login is already in Unix or local use */
      if (getpwnam(newlogin)) {
         int err;
         sprintf(cmd,"/usr/local/bin/webuser %s", newlogin);
         err = unix_cmd(cmd);
         if (err)
            wfputs("Couldn't create web account.\n", out);
         else
            wfputs("Web account created\n", out);
         return 1;
      }
      if (get_local_user(NULL, newlogin, NULL, NULL, NULL)) {
         sprintf(cmd,"The login \"%s\" is already in use.  Please choose another login\n", newlogin);
         wfputs(cmd, out);
         return 1;
      }

      /* First command form simply tests legality of login */
      if (argc < 3) {
         wfputs("Login is legal\n", out);
         return 1;
      }
   }

   if (match(get_conf_param("verifyemail",VERIFY_EMAIL),"true")) {
      strcpy(newpass1, random_password(newlogin));
   } else {
      /* Get new pass1 */
      if (!normaluser && argc>2)
         strcpy(newpass1, argv[2]);
      else {
         printf("New password: ");
         strcpy(newpass1, get_password());
      }

      /* Get new pass2 */
      if (!normaluser && argc>3)
         strcpy(newpass2, argv[3]);
      else {
         printf("New password (again): ");
         strcpy(newpass2, get_password());
      }
   
      /* Sanity checks on passwords */
      if (strcmp(newpass1,newpass2)) {
         wfputs("The two copies of your password do not match. Please try again.",
          out);
         return 1;
      }          
      argc-=2;
      argv+=2;
   }

   if (!normaluser) {
      /* Get new fullname */
      if (argc>2)
         strcpy(newname, argv[2]);
      else {
         printf("Full name: ");
         ngets(newname, st_glob.inp);
      }
   
      /* Sanity check on fullname */
      if (!sane_fullname(newname)) {
         wfputs("Illegal character in fullname.  Please try again.\n", out);
         return 1;
      }
   
      /* Get new email */
      if (argc>3)
         strcpy(newemail, argv[3]);
      else {
         printf("Email address: ");
         ngets(newemail, st_glob.inp);
      }

      /* Sanity check on email */
      if (!sane_email(newemail)) {
         wfputs("Illegal character in email address.  Please try again.\n", out);
         return 1; 
      }  

#ifdef HAVE_DBM_OPEN
      /* argv[4] contains a list of variables to save to the dbm file,
       * excluding fullname, email, passwd 
       */
      if (argc>4)
         others = argv[4];
#endif
   }
   
   /* Make a home directory */
   sprintf(homedir, "%s/%c/%c/%s", get_conf_param("userhome", USER_HOME), 
    tolower(newlogin[0]), tolower(newlogin[1]), newlogin);
   /* FORK */
   fflush(stdout);
   if (status & S_PAGER)
      fflush(st_glob.outp);

   cpid=fork();
   if (cpid) { /* parent */
      if (cpid<0) return 1; /* error: couldn't fork */
      while ((wpid = wait((int *)0)) != cpid && wpid != -1);
   } else { /* child */
      signal(SIGINT,SIG_DFL);
      signal(SIGPIPE,SIG_DFL);
      close(0);

      setuid(getuid());
      setgid(getgid());
      
      mkdir_all(homedir,0755);
      exit(0);
   }

   /* Now test to make sure it succeeded */
   if (stat(homedir,&st) || st.st_uid!=getuid()) {
      error("creating ", homedir);
      wfputs("System administrator needs to check file permissions on web home dir.\n", out);
      return 1;
   }

   /* Save newuser info to user file (could be done in user dir) */
   save_user_info(newlogin, newname, newemail, newpass1, others);

   /* Set partdir */
   get_partdir(partdir, newlogin);

   /* Copy templates to user's home directory */
   {
      char src[MAX_LINE_LENGTH];
      char dest[MAX_LINE_LENGTH];

      sprintf(src,  "%s/defaults/.cflist", bbsdir);
      sprintf(dest, "%s/.cflist", partdir);
      copy_file(src, dest, partfile_perm()); /* copy default .cflist */

      sprintf(src,  "%s/defaults/.cfrc", bbsdir);
      sprintf(dest, "%s/.cfrc", homedir);
      copy_file(src, dest, SL_USER); /* copy default .cfrc */

      sprintf(src,  "%s/defaults/.cfonce", bbsdir);
      sprintf(dest, "%s/.cfonce", homedir);
      copy_file(src, dest, SL_USER); /* copy default .cfonce */
   }

   if (match(get_conf_param("verifyemail",VERIFY_EMAIL),"true")) {

      /* Set fields which might be referenced */
      strcpy(login, newlogin);
      strcpy(st_glob.fullname, newname);
      strcpy(email, newemail);
      strcpy(home,  homedir);

      email_password(newpass1);
      wfputs("You are now registered and should receive email shortly.\n", out);
   } else {
      wfputs("You are now registered and may log in.\n", out);
   }
   return 1;
}

#ifdef INCLUDE_EXTRA_COMMANDS
/* command: authenticate ticket */
/* This can authenticate a user according to the Yapp local user file ONLY */
int
authenticate(argc, argv)
   int argc;
   char **argv;
{
   if (argc!=2) {
      printf("Usage: authenticate ticket\n");
      return 1;
   }

   /* Trash newline */
   if (argv[1][ strlen(argv[1])-1 ]=='\n')
       argv[1][ strlen(argv[1])-1 ] ='\0';
   if (do_authenticate(argv[1]))
      login_user();
   else
      printf("Authentication failed.\n");
   return 1;
}
#endif /* INCLUDE_EXTRA_COMMANDS */
#endif /* WWW */

/*******************************/
/* Functions to modify .cflist */
/*******************************/

/*
 * Reload cflist if different timestamp than when we last read it
 * or if different size (in case changed in < 1 second)
 */
void
refresh_list() 
{
   char path[MAX_LINE_LENGTH];
   struct stat st; 

   sprintf(path,"%s/%s",partdir,".cflist");
   if (!stat(path,&st) && (st.st_mtime!=listtime || st.st_size!=listsize)) {
      xfree_array(cflist);
      cflist=grab_file(partdir,".cflist",GF_WORD|GF_SILENT|GF_IGNCMT);
      listtime = st.st_mtime;
      listsize = st.st_size;
      current = -1;

      /* Set up cfliststr */
      xfree_string(cfliststr);
      cfliststr=(char *)xalloc(0, st.st_size+3);
      cfliststr[0]=' '; /* leading space */
      implode(cfliststr+1, cflist, " ", 0);
      strcat(cfliststr, " "); /* trailing space */
   }
   if (!cfliststr)
      cfliststr = xstrdup(" ");
}

/* 
 * Append conference name to user's .cflist 
 */
void
add_cflist(cfname)
   char *cfname;
{
   char path[MAX_LINE_LENGTH];
   FILE *fp;
   int i;
   int perm = partfile_perm();

   sprintf(path,"%s/%s",partdir,".cflist");

   if (perm==SL_USER) {
      fp = smopenw(path, O_A);
      fprintf(fp, "%s\n", cfname);
      smclose(fp);
   } else { /* SL_OWNER */
      fp = mopen(path, O_A);
      fprintf(fp, "%s\n", cfname);
      mclose(fp);
   }

   /* Manually add to cflist in memory 
    * We can't just use refresh_list() since it may have changed
    * less than 1 second ago.
    */
   i = xsizeof(cflist);
   if (i)
      cflist = xrealloc_array(cflist, i+1);
   else
      cflist = xalloc(1, sizeof(char *));
   cflist[i] = xstrdup(cfname);

   /* Append to cfliststr */
   i = strlen(cfliststr);
   cfliststr = xrealloc_string(cfliststr, i+strlen(cfname)+2);
   if (i)
      strcat(cfliststr, " ");
   strcat(cfliststr, cfname);
}

/********************************************************/
/* Delete conference from .cflist                       */
/********************************************************/
int                /* RETURNS: 1 if deleted, 0 if error */
del_cflist(cfname) /* ARGUMENTS:                        */
   char *cfname;   /*    Conference name to delete      */
{
   char  path[MAX_LINE_LENGTH];
   FILE *newfp;
   int   i, sz;
   int perm = partfile_perm();

   refresh_list();

   /* First see if it's in the list at all */
   sz = xsizeof(cflist);
   for (i=0; i<sz && strcmp(cfname, cflist[i]); i++);
   if (i==sz)
      return 0;

   sprintf(path,   "%s/%s",partdir,".cflist");
   
   if (perm==SL_USER)
      newfp = smopenw(path, O_W);
   else /* SL_OWNER */
      newfp = mopen(path, O_W);
   if (newfp==NULL) {
      printf("Couldn't open .cflist for writing\n");
      return 0;
   }

   for (i=0; i<sz; i++)
      if (strcmp(cfname, cflist[i]))
         fprintf(newfp, "%s\n", cflist[i]);

   if (perm==SL_USER)
      smclose(newfp);
   else /* SL_OWNER */
      mclose(newfp);

   refresh_list();
   return 1;
}

void
show_cflist()
{
   int i;

   refresh_list();
   for (i=0; i<xsizeof(cflist); i++)
      printf("%s %s\n",(current==i)? "-->" : "   ",cflist[i]);
}

/* The "cflist" command */
int
do_cflist(argc, argv)
   int    argc;
   char **argv;
{
   int i;

   if (argc>2 && match(argv[1], "a_dd")) {
      for (i=2; i<argc; i++)
         add_cflist(argv[i]);
   } else if (argc>2 && match(argv[1], "d_elete"))
      for (i=2; i<argc; i++)
         del_cflist(argv[i]);
   else if (argc==2 && match(argv[1], "s_how"))
      show_cflist();

   else if (argc==2 && match(argv[1], "r_estart"))
      if (cflist && xsizeof(cflist)){
         current=-1;
         do_next(0,NULL);
      }

   else {
      printf("usage: cflist add <%s> ...\n", conference(0));
      printf("       cflist delete <%s> ...\n", conference(0));
      printf("       cflist show\n");
      printf("       cflist restart\n");
   }
   return 1;
}

/*
 * Change user fullname (the permanent, global one)
 */
int
chfn(argc,argv)   /* ARGUMENTS:             */
   int    argc;         /*    Number of arguments */
   char **argv;         /*    Argument list       */
{
   char newname[MAX_LINE_LENGTH];
   char newemail[MAX_LINE_LENGTH];
   FILE *fp=NULL, *tmp=NULL, *out=stdout;
   char *userfile, *oldname=NULL;
   char tmpname[MAX_LINE_LENGTH];
   char line[MAX_LINE_LENGTH], *w;
   char *oldemail=NULL;
   int   found=0, suid;
#ifdef HAVE_DBM_OPEN
   int  dbm = 0;
   char oldnamebuff[MAX_LINE_LENGTH];
   char oldemailbuff[MAX_LINE_LENGTH];
#endif /* HAVE_DBM_OPEN */

#ifdef HAVE_INFORMIX_OPEN
   int  informix = 0;
#ifndef HAVE_DBM_OPEN
   char oldnamebuff[MAX_LINE_LENGTH];
   char oldemailbuff[MAX_LINE_LENGTH];
#endif
#endif /* HAVE_INFORMIX_OPEN */

   if (status & S_EXECUTE)
      out = NULL;

   /* If it's a Unix user, do a normal chfn command */
   if (getuid()!=get_nobody_uid()) {
      unix_cmd("/usr/bin/chfn");

      /* Reload fullname */
      if (!get_user(&uid, login, fullname, home, email)) {
         error("reading ","user entry");
         endbbs(2);
      }    
      return 1;
   }

   /* Insure that the user has authenticated */
   if (status & S_NOAUTH) {
      wfputs("You are not authenticated.\n", out);
      return 1;
   } 
   
   userfile = get_userfile(login, &suid);

#ifdef HAVE_DBM_OPEN
   if (match(get_conf_param("userdbm",USERDBM),"true")) {
      strcpy(oldnamebuff, get_dbm(userfile, "fullname", SL_USER));
      strcpy(oldemailbuff, get_dbm(userfile, "email", SL_USER));
      if (!oldnamebuff[0] && !oldemailbuff[0]) {
         error ("opening ", userfile);
         return 1;
      }
      found = 1;
      dbm = 1;
      oldname  = oldnamebuff;
      oldemail = oldemailbuff;
   }

   if (!dbm) {
#endif /* HAVE_DBM_OPEN */

#ifdef HAVE_INFORMIX_OPEN
   if (match(get_conf_param("useinformix","false"),"true")) {
      strcpy(oldnamebuff, get_informix(F_LOGIN,login, "fullname", SL_OWNER));
      strcpy(oldemailbuff, get_informix(F_LOGIN,login, "email", SL_OWNER));
      if (!oldnamebuff[0] && !oldemailbuff[0]) {
         error ("Error reading from  ", "Informix Database");
         return 1;
      }
      found = 1;
      informix = 1;
      oldname  = oldnamebuff;
      oldemail = oldemailbuff;
   }

   if (!informix) {
#endif /* HAVE_INFORMIX_OPEN */

      /* Open old userfile (could be in user dir) */
      if (suid==SL_USER)
         fp = smopenr(userfile, O_R);
      else
         fp = mopen(userfile, O_R);
      if (fp==NULL) {
         error ("opening ", userfile);
         exit(1);
      }

      /* Open new userfile */
      sprintf(tmpname, "%s.%d", userfile, getpid());
      if (suid==SL_USER)
         tmp = smopenw(tmpname, O_W);
      else
         tmp = mopen(tmpname, O_W);
      if (tmp==NULL) {
         error ("opening ", tmpname);
         exit(1);
      }

      /* Find login in user file */
      while (ngets(line,fp)) {
         if (line[0]=='#' || !line[0]) {
            fprintf(tmp, "%s\n", line);
            continue;
         }
         if ((w = strchr(line, ':')) != NULL) {
            *w = '\0';
            if (strcmp(login, line))
               fprintf(tmp, "%s:%s\n", line, w+1);
            else {
               found=1;
               oldname = w+1;
               oldemail = strchr(w+1, ':');
               if (!oldemail) continue;
               *oldemail++ = '\0';
               break;
            }
         }
      }

      /* Verify that it's a user account, not a Unix account */
      if (!found) {
         wfputs("Couldn't find local Yapp user information.\n", out);
         if (suid==SL_USER) {
            smclose(tmp);
            smclose(fp);
         } else {
            mclose(tmp);
            mclose(fp);
         }
         return 1;
      }

#ifdef HAVE_INFORMIX_OPEN
   }
#endif  /* HAVE_INFORMIX_OPEN */

#ifdef HAVE_DBM_OPEN
   }
#endif
     
   /* Get new fullname */
   if (argc>1) 
      strcpy(newname, argv[1]);
   else {
      if (!(flags & O_QUIET)) {
         printf("Your old name is: %s\n", oldname);
         printf("Enter replacement or return to keep old? ");
      }
      if (!ngets(newname, st_glob.inp) || !strlen(newname)) {
         wfputs("Name not changed.\n", out);
         strcpy(newname, oldname);
      } else
         wfputs("Fullname changed.\n", out);
   }

   /* Get new email */
   if (argc>2) 
      strcpy(newemail, argv[2]);
   else {
      if (!(flags & O_QUIET)) {
         printf("Your old email address is: %s\n", oldemail); 
         printf("Enter replacement or return to keep old? ");
      } 
      if (!ngets(newemail, st_glob.inp) || !strlen(newemail)) {
         wfputs("Email address not changed.\n", out);
         strcpy(newemail, oldemail);
      } else
         wfputs("Email address changed.\n", out);
   }

   /* Sanity check on fullname */
   if (!sane_fullname(newname)) {
      wfputs("Illegal character in fullname.  Please try again.\n", out);
#ifdef HAVE_DBM_OPEN
      if (!dbm) {
#endif

#ifdef HAVE_INFORMIX_OPEN
      if (!informix) {
#endif /* HAVE_INFORMIX_OPEN */

         if (suid==SL_USER) {
            smclose(tmp);
            smclose(fp);
         } else {
            mclose(tmp);
            mclose(fp);
         }

#ifdef HAVE_INFORMIX_OPEN
      }
#endif /*HAVE_INFORMIX_OPEN */
#ifdef HAVE_DBM_OPEN
      }
#endif
      return 1;
   }

   /* Sanity check on email */
   if (!sane_email(newemail)) {
      wfputs("Illegal character in email address.  Please try again.\n", out);
#ifdef HAVE_DBM_OPEN
      if (!dbm) {
#endif

#ifdef HAVE_INFORMIX_OPEN
      if (!informix) {
#endif /*HAVE_INFORMIX_OPEN */
         if (suid==SL_USER) {
            smclose(tmp);
            smclose(fp);
         } else {
            mclose(tmp);
            mclose(fp);
         }

#ifdef HAVE_INFORMIX_OPEN
      }
#endif /* HAVE_INFORMIX_OPEN */
#ifdef HAVE_DBM_OPEN
      }
#endif
      return 1;
   }
      
#ifdef HAVE_DBM_OPEN
   if (dbm) {
      save_dbm(userfile, "fullname",  newname, SL_USER);
      save_dbm(userfile, "email", newemail, SL_USER);
      return 1;
   } 
#endif /* HAVE_DBM_OPEN */

#ifdef HAVE_INFORMIX_OPEN
   if (informix) {
      informix_update(login,F_FULLNAME,newname,SL_OWNER);
      informix_update(login, F_EMAIL, newemail, SL_OWNER);
      return 1;
   } 
#endif /* HAVE_INFORMIX_OPEN */

   /* Insert new one */
   fprintf(tmp, "%s:%s:%s\n", login, newname, newemail);

   /* Copy remainder of file */
   while (ngets(line,fp))
      fprintf(tmp, "%s\n", line);

   if (suid==SL_USER) {
      smclose(tmp);
      smclose(fp);
   } else {
      mclose(tmp);
      mclose(fp);
   }

   move_file(tmpname, userfile, suid);
/*
   if (rename(tmpname, userfile)) {
      error("renaming ", tmpname);
      exit(1);
   }
*/

   return 1;
}

/*
 * Change user password
 * usage: passwd [oldpass [newpass1 [newpass2]]]
 */
int
passwd(argc,argv)   /* ARGUMENTS:             */
   int    argc;         /*    Number of arguments */
   char **argv;         /*    Argument list       */
{
   char oldpass[MAX_LINE_LENGTH], 
        newpass1[MAX_LINE_LENGTH],
        newpass2[MAX_LINE_LENGTH];
   FILE *fp=NULL, *tmp=NULL, *out=stdout;
   char *passfile=NULL;
   char tmpname[MAX_LINE_LENGTH];
   char line[MAX_LINE_LENGTH], *w=NULL;
   char *cpw;
   int   found=0;
#ifdef HAVE_DBM_OPEN
   int  dbm = 0;
   char savedpass[MAX_LINE_LENGTH];
   char *userfile=NULL;
#endif /* HAVE_DBM_OPEN */

   if (status & S_EXECUTE)
      out = NULL;

   /* If it's a Unix user, do a normal passwd command */
   if (getuid()!=get_nobody_uid()) { 
      unix_cmd("/usr/bin/passwd");
      return 1;
   }

   if (status & S_NOAUTH) {
      wfputs("You are not authenticated.\n", out);
      return 1;
   } 

#ifdef HAVE_DBM_OPEN
   if (match(get_conf_param("userdbm",USERDBM),"true")
    && !strcmp(get_conf_param("userfile",USER_FILE),
               get_conf_param("passfile",PASS_FILE))) {
      int suid;
      userfile = get_userfile(login, &suid);
      strcpy(savedpass, get_dbm(userfile, "passwd", SL_USER));
      if (!savedpass[0]) {
         error ("opening ", userfile);
         return 1;
      }
      found = 1;
      dbm = 1;
   }
   
   if (!dbm) {
#endif /* HAVE_DBM_OPEN */

   
      /* Open old passfile */
      passfile = get_conf_param("passfile", PASS_FILE);
      if ((fp = mopen(passfile, O_R))==NULL) {
         error ("opening ", passfile);
         exit(1);
      }

      /* Open new passfile */
      sprintf(tmpname, "%s.%d", passfile, getpid());
      if ((tmp = mopen(tmpname, O_W))==NULL) {
         error ("opening ", tmpname);
         exit(1);
      }

      /* Find login in pass file */
      while (ngets(line,fp)) {
         if (line[0]=='#' || !line[0]) {
            fprintf(tmp, "%s\n", line);
            continue;
         }
         if ((w = strchr(line, ':')) != NULL) {
            *w = '\0';
            if (strcmp(login, line))
               fprintf(tmp, "%s:%s\n", line, w+1);
            else {
               found=1;
               break;
            }
         }
      }
   
      /* Verify that it's a user account, not a Unix account */
      if (!found) {
         wfputs("You're not using a local Yapp account.\n", out);
         mclose(tmp);
         mclose(fp);
         return 1;
      }
#ifdef HAVE_DBM_OPEN
   }
#endif

     
   /* Get old password */
   if (argc>1)
      strcpy(oldpass, argv[1]);
   else {
      wfputs("Old password: ", out);
      strcpy(oldpass, get_password());
   }

   /* Verify old password */
#ifdef HAVE_DBM_OPEN
   if (dbm) {
      if (strcmp(oldpass, savedpass)) {
         wfputs("You did not enter your old password correctly.\n", out);
         return 1;
      }
   } else {
#endif


      cpw = crypt(oldpass,w+1);
      if (strcmp(cpw, w+1)) {
         wfputs("You did not enter your old password correctly.\n", out);
         mclose(tmp);
         mclose(fp);
         return 1;
      }
#ifdef HAVE_DBM_OPEN
   }
#endif


   if (argc>2) 
      strcpy(newpass1, argv[2]);
   else {
      wfputs("New password: ", out);
      strcpy(newpass1, get_password());
   }  
      
   if (argc>3)
      strcpy(newpass2, argv[3]);
   else {
      wfputs("Retype new password: ", out);
      strcpy(newpass2, get_password());
   }  
                    
   if (!newpass1[0] || !newpass2[0] || strcmp(newpass1, newpass2)) {
      wfputs("The two copies of your password do not match. Please try again.\n", out);
#ifdef HAVE_DBM_OPEN
      if (!dbm) {
#endif
         mclose(tmp);
         mclose(fp);
#ifdef HAVE_DBM_OPEN
      }
#endif
      return 1;
      
   }

#ifdef HAVE_DBM_OPEN
   if (dbm && !strcmp(get_conf_param("userfile",USER_FILE),
                      get_conf_param("passfile",PASS_FILE))) {
      save_dbm(userfile, "passwd",  newpass1, SL_USER);
      wfputs("Password changed.\n", out);
      return 1;
   }
#endif /* HAVE_DBM_OPEN */

   /* Encrypt new one */
   add_password(login, newpass1, tmp);
/*
   (void)SRAND((int)time((time_t *)NULL));
   to64(&salt[0],RAND(),2);
   cpw = crypt(newpass1,salt);
   fprintf(tmp, "%s:%s\n", login, cpw);
*/

   /* Copy remainder of file */
   while (ngets(line,fp))
      fprintf(tmp, "%s\n", line);

   if (fp) mclose(fp);
   if (tmp) mclose(tmp);

   if (rename(tmpname, passfile)) {
      error("renaming ", tmpname);
      exit(1);
   }

   wfputs("Password changed.\n", out);
   return 1;
}
