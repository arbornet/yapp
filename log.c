#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "yapp.h"
#include "struct.h"
#include "globals.h"
#include "macro.h"
#include "log.h"

char *  /* RETURNS: pointer to static buffer */
expand_sep(str, fl, type)
   char *str; 
   int   fl;
   int   type;
{
   static char buff[MAX_LINE_LENGTH];
   char oldeval[MAX_LINE_LENGTH];
   int tmp_status;

   tmp_status = status;
   strcpy(oldeval, evalbuf);
   evalbuf[0] = '\0';
   status |=  S_EXECUTE;
   if (type==M_RFP)
      itemsep(str, fl);
   else
      confsep(str, confidx, &st_glob, part, fl);
   status &= ~S_EXECUTE;  
   strcpy(buff, evalbuf);
   strcpy(evalbuf, oldeval);
   status = tmp_status;

   return buff;
}

static char *      /* OUT: sepstr */
find_event(event, logfile)
   char  *event;   /* IN : event name */
   char **logfile; /* OUT: log file name */
{
   char buff[MAX_LINE_LENGTH];

   /* Look up variables: <event>log, <event>logsep */
   sprintf(buff, "%slog", event);
   (*logfile) = expand(buff, DM_VAR);
   if (!*logfile)
      return NULL;
   strcat(buff, "sep");
   return expand(buff, DM_VAR);
}

void
custom_log(event, type)
   char *event;
   int   type;
{
   char *logfile, *sepstr;
   char *str, file[MAX_LINE_LENGTH];
   sepstr = find_event(event, &logfile);
   if (!sepstr || !sepstr[0]) 
      return;
   strcpy(file, expand_sep(logfile, 1, type));
   str = expand_sep(sepstr, 0, type);
   if (file && file[0] && str && str[0])
      write_file(file, str);
}

/******************************************************************************/
int
logevent(argc,argv)   /* ARGUMENTS:             */
   int    argc;       /*    Number of arguments */
   char **argv;       /*    Argument list       */
{ 
   char *buff, *q;

   if (argc!=4) {
      printf("usage: log <event> <filename> <sepstring>\n");
      return 1;
   }

   buff = (char *)xalloc(0, strlen(argv[1]) + strlen(argv[2]) + strlen(argv[3]) + 20);

   q = (argv[2][0]=='"')? "" : "\"";
   sprintf(buff, "constant %slog %s%s%s", argv[1], q, argv[2], q);
   command(buff, 0);
   q = (argv[3][0]=='"')? "" : "\"";
   sprintf(buff, "constant %slogsep %s%s%s", argv[1], q, argv[3], q);
   command(buff, 0);

   xfree_string(buff);
   return 1;
}
