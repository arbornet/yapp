/* XALLOC.H: @(#)xalloc.h 1.5 93/06/07 Copyright (c)1993 thalerd */
short  xsizeof  PROTO((void *arr));
char **xalloc   PROTO((int num, int eltsize));
void   xcheck   PROTO(());
void   xdump    PROTO(());
char  *xstrdup  PROTO((char *str));
void   xfree_array  PROTO((void *arr));
void   xfree_string PROTO((void *arr));
char *xrealloc_string PROTO((void *arr, int num));
char **xrealloc_array PROTO((void *arr, int num));
void   xstat    PROTO(());
