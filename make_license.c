/* $Id: make_license.c,v 1.7 1997/01/07 23:04:10 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h> /* for atoi() */
#include <sys/errno.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include "yapp.h"
#ifndef HAVE_GETHOSTNAME
#ifdef HAVE_SYSINFO
#include <sys/systeminfo.h>
#endif
#endif
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

char hostname[MAX_LINE_LENGTH];

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

int
get_hash(str)
   char *str;
{
   int32 ret=0;
   char *p;

   for (p=str; *p; p++)
      ret = (ret*4) ^ (*p);
   return ret;
}

/*
 * Generate a ticket that proves that the key (i.e. encrypted password)
 * was known at time tm.  This assumes that the PASS_FILE file is only
 * readable by cfadm.
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

/*
 * Given 4 integers compute a checksum on them using a "secret"
 * algorithm.
 */      
char *   
compute_checksum(max_users, max_count, curr_count, curr_time, exp_time)
   int max_users;
   int max_count;
   int curr_count;
   int curr_time;
   int exp_time;  /* IN: expiration time, or 0 for never */
{
   char buff[MAX_LINE_LENGTH];
   int hosthash = get_hash(hostname);
  
   sprintf(buff, "%08X%08X%08X%08X",
    max_users, max_count, curr_count, curr_time);
   if (exp_time)
      sprintf(buff+strlen(buff), "%08X", exp_time);
   return make_ticket(buff, hosthash);
}

main(argc, argv)
   int argc;
   char **argv;
{
   char buff[MAX_LINE_LENGTH];
   char regto[MAX_LINE_LENGTH];
   char timestamp[MAX_LINE_LENGTH];
   int max_users, max_count, curr_count, curr_time, exp_time;
   FILE *fp;

   if (argc<2) {
      printf("Usage: %s filename\n", argv[0]);
      exit(1);
   }

#ifdef HAVE_GETHOSTNAME
   gethostname(buff,MAX_LINE_LENGTH);
#else
#ifdef HAVE_SYSINFO
   sysinfo(SI_HOSTNAME, buff, MAX_LINE_LENGTH);
#else
   strcpy(buff, "unknown");
#endif
#endif
   printf("Hostname [%s]: ", buff);
   fgets(hostname, MAX_LINE_LENGTH, stdin);
   if (hostname[0] && hostname[strlen(hostname)-1]=='\n')
      hostname[strlen(hostname)-1]='\0';
   if (!hostname[0])
      strcpy(hostname, buff);

   printf("Registration line...\n> ");
   fgets(regto, MAX_LINE_LENGTH, stdin);

   printf("Max Users: ");
   scanf("%d", &max_users);

   printf("Max Count: ");
   scanf("%d", &max_count);

   curr_count = 0;
   curr_time = time(NULL);

   printf("Timestamp [%d]: ", curr_time);
   fgets(timestamp, MAX_LINE_LENGTH, stdin); /* eat previous newline */
   fgets(timestamp, MAX_LINE_LENGTH, stdin); /* get real time */
   if (timestamp[0] && timestamp[0]!='\n') 
      curr_time = atoi(timestamp);

   exp_time = curr_time + 60*60*24*60; /* 60 days */
   printf("Default expiration: %s", ctime(&exp_time));
   printf("Expiration timestamp [%d]: ", exp_time);
   fgets(timestamp, MAX_LINE_LENGTH, stdin); 
   if (timestamp[0] && timestamp[0]!='\n')
      exp_time = atoi(timestamp);

   if ((fp = fopen(argv[1], "w"))==NULL) {
      printf("Couldn't open %s\n", argv[1]);
      exit(1);
   }
   
   fprintf(fp, "%s", regto);
   fprintf(fp, "%d %d\n", max_users, max_count);
   fprintf(fp, "%d %d\n", curr_count, curr_time);
   fprintf(fp, "%s\n", compute_checksum(max_users, max_count, curr_count, 
    curr_time, exp_time));
   if (exp_time)
      fprintf(fp, "%d\n", exp_time);
   fclose(fp);
   exit(0);
}
