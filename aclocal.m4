dnl Check to see which way fdopen() behaves.

AC_DEFUN(AC_STUPID_REOPEN,
[
   dnl Tell what we are looking for
   AC_MSG_CHECKING(whether fdopen is stupid)

   dnl Make possibly cached check
   AC_CACHE_VAL(ac_cv_stupid_reopen,
   [
      AC_TRY_RUN([#include <stdio.h>
FILE *inp, *fp1, *fp2, *saved_inp1;
int 
new_stdin(fd)
   int fd;
{
   int old = dup(0);
   close(0);
   dup(fd);
   inp = fdopen(0, "r");
   return old;

}
main()
{
   int saved_fd1;
   char buff[80];

   inp = stdin;
   fp1 = fopen("LICENSE", "r");
   new_stdin(fileno(fp1));
   fscanf(inp,"%s", buff);
   fp2 = fopen("README", "r");
   saved_inp1 = inp;
   saved_fd1 = new_stdin(fileno(fp2));
   fscanf(inp,"%s", buff);
   close(0);
   dup(saved_fd1);
   inp = fdopen(0, "r");
   close(saved_fd1);
   inp = saved_inp1;
   fscanf(inp,"%s", buff);
   exit(buff[0]=='T');
}
      ], ac_cv_stupid_reopen=no, ac_cv_stupid_reopen=yes, ac_cv_stupid_reopen=no)
   ])
   if test $ac_cv_stupid_reopen = yes; then
      AC_DEFINE(STUPID_REOPEN)
   fi

   dnl Show result
   AC_MSG_RESULT($ac_cv_stupid_reopen)
])

dnl See if we should manually define sys_errlist
AC_DEFUN(AC_CHECK_SYS_ERRLIST,
[AC_MSG_CHECKING([whether sys_errlist is defined])
AC_CACHE_VAL(ac_cv_sys_errlist, 
[AC_TRY_COMPILE([#include <stdio.h>
#include <errno.h>], [extern char *sys_errlist[];],
  eval "ac_cv_sys_errlist=no",
  eval "ac_cv_sys_errlist=yes")])dnl 
   if test $ac_cv_sys_errlist = yes; then
      AC_DEFINE(HAVE_SYS_ERRLIST)
   fi
   AC_MSG_RESULT($ac_cv_sys_errlist)
])

dnl See if crypt() only uses 8 characters
AC_DEFUN(AC_SHORT_CRYPT,
[
   dnl Tell what we are looking for
   AC_MSG_CHECKING(whether crypt only uses 8 characters)

   dnl Make possibly cached check
   AC_CACHE_VAL(ac_cv_short_crypt,
   [
      AC_TRY_RUN([#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

main()
{
   char buff[256];

   strcpy(buff, crypt("12345678",  "O2"));
   exit(!strcmp(buff, crypt("123456789", "O2")));
}
      ], ac_cv_short_crypt=no, ac_cv_short_crypt=yes, ac_cv_short_crypt=no)
   ])
   if test $ac_cv_short_crypt = yes; then
      AC_DEFINE(SHORT_CRYPT)
   fi

   dnl Show result
   AC_MSG_RESULT($ac_cv_short_crypt)
])

dnl See if rand() is the standard BSD 32-bit generator
AC_DEFUN(AC_SHORT_RAND,
[
   dnl Tell what we are looking for
   AC_MSG_CHECKING(whether rand only gives 16-bit numbers)

   dnl Make possibly cached check
   AC_CACHE_VAL(ac_cv_short_rand,
   [
      AC_TRY_RUN([main()
{
   srand(0);
   exit(rand()!=12345);
}
      ], ac_cv_short_rand=no, ac_cv_short_rand=yes, ac_cv_short_rand=no)
   ])
   if test $ac_cv_short_rand = yes; then
      AC_DEFINE(SHORT_RAND)
   fi

   dnl Show result
   AC_MSG_RESULT($ac_cv_short_rand)
])
