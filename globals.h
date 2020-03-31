/* GLOBALS.H: @(#)globals.h 1.2 94/01/20 (c)1993 thalerd */

/* GLOBAL VARS */
#include <stdio.h>
#include <sys/types.h>

/* Status info */
extern flag_t        flags;
extern unsigned char mode;
extern flag_t        status;
extern flag_t        debug;
extern stdin_t       saved_stdin[STDIN_STACK_SIZE];
extern stdin_t       orig_stdin[STDIN_STACK_SIZE];
extern int           stdin_stack_top;

/* Conference info */
extern int     current;     /* current index to cflist */
extern int     confidx;     /* current index to conflist */
extern int     defidx;
extern int     joinidx;     /* current index to conflist */
extern char  **cflist;                    /* User's cflist */
extern char   *cfliststr;                 /* cflist in a string */
extern char  **fw;                        /* List of FW's for current conf */

/* System info */
extern char     bbsdir[MAX_LINE_LENGTH];
extern char     helpdir[MAX_LINE_LENGTH];
extern assoc_t *conflist; /* System table of conferences */
extern assoc_t *desclist; /* System table of conference descriptions */
extern int      maxconf;       /* maximum index to conflist */
extern option_t option[];
extern option_t debug_opt[];
extern char     hostname[MAX_LINE_LENGTH];

/* Info on the user */
extern int     uid;
extern char    login[L_cuserid];
extern char    fullname[MAX_LINE_LENGTH]; /* Full name from passwd file */
extern char    email[MAX_LINE_LENGTH];  /* User's email address */
extern char    home[MAX_LINE_LENGTH];  /* User's home directory */
extern char    work[MAX_LINE_LENGTH];  /* User's work directory */
extern char    partdir[MAX_LINE_LENGTH];  /* Location of user's partfiles */

/* Item info */
extern status_t    st_glob,
                   st_new;
extern response_t  re[MAX_RESPONSES];
extern sumentry_t  sum[MAX_ITEMS];
extern partentry_t part[MAX_ITEMS];

extern char evalbuf[MAX_LINE_LENGTH];
