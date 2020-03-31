/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Define if you have the fstat utility. */
#define HAVE_FSTAT 1

/* Define if your compiler gives warnings about protos when functions
   are defined with the K&R style.                                    */
/* #undef USE_INT_PROTOS */

/* Define if fdopen() doesn't behave (System V) */
/* #undef STUPID_REOPEN */

/* Define if stdio.h or errno.h defines sys_errlist */
#define HAVE_SYS_ERRLIST 1

/* Define if crypt() only uses 8 bytes of the key */
#define SHORT_CRYPT 1

/* Define if rand() only gives 16-bit numbers */
#define SHORT_RAND 1

/* Define if you have the cuserid function.  */
/* #undef HAVE_CUSERID */

/* Define if you have the dbm_open function.  */
#define HAVE_DBM_OPEN 1

/* Define if you have the flock function.  */
#define HAVE_FLOCK 1

/* Define if you have the gethostname function.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have the inet_addr function.  */
#define HAVE_INET_ADDR 1

/* Define if you have the inet_network function.  */
#define HAVE_INET_NETWORK 1

/* Define if you have the lockf function.  */
#define HAVE_LOCKF 1

/* Define if you have the mktime function.  */
#define HAVE_MKTIME 1

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if you have the strncasecmp function.  */
#define HAVE_STRNCASECMP 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the sysinfo function.  */
/* #undef HAVE_SYSINFO */

/* Define if you have the timelocal function.  */
#define HAVE_TIMELOCAL 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <ndbm.h> header file.  */
#define HAVE_NDBM_H 1

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the crypt library (-lcrypt).  */
#define HAVE_LIBCRYPT 1
/* config.h.bot - start */

/* Force int32 to be 4 bytes */
#if (SIZEOF_LONG == 8)
#define int32 int
#define u_int32 unsigned int
#else
#define int32 long
#define u_int32 unsigned long
#endif

/* Set up prototype macro */
#ifndef PROTO
#ifdef __STDC__
#define PROTO(x) x
#else
#define PROTO(x) ()
#endif
#endif

/* config.h.bot - end */
