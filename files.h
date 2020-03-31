/* MOPEN.H: @(#)files.h 1.2 93/05/11 Copyright (c)1993 thalerd */

/* mopen() flags: note that on some operating systems (e.g. Solaris), 
 * the combination of O_R and O_LOCK is illegal, so don't do it!
 */
#include <fcntl.h>
#define O_PASSTHRU 0x00000FFF  /* actual flags to open() */
#define O_PRIVATE  0x00001000  /* mode 0600 (vs. 0644)         */
#define O_NOCREATE 0x00002000  /* don't create (must exist)    */
#define O_LOCK     0x00004000  /* need to lock it?             */
#define O_PIPE     0x00008000  /* is this a pipe?              */
#define O_SILENT   0x00010000  /* is this a pipe?              */
#define O_NOBLOCK  0x00020000  /* don't block, fail on no lock */
#define O_R        (O_RDONLY|O_NOCREATE)              /* "r"  */
#define O_W        (O_WRONLY|O_CREAT|O_TRUNC |O_LOCK) /* "w"  */
#define O_A        (O_WRONLY|O_CREAT|O_APPEND|O_LOCK) /* "a"  */
#define O_RPLUS    (O_RDWR  |O_NOCREATE      |O_LOCK) /* "r+" */
#define O_WPLUS    (O_RDWR  |O_CREAT|O_TRUNC |O_LOCK) /* "w+" */
#define O_APLUS    (O_RDWR  |O_CREAT|O_APPEND|O_LOCK) /* "a+" */

FILE *mopen  PROTO((char *file,long fl));
int   mclose PROTO((FILE *fp));
void  mcheck PROTO(());
void  mdump  PROTO(());
void  madd   PROTO((int fd, char *file, SHORT flg, int pid));
int   get_pid PROTO((FILE *fp));
