/* MAIN.H: @(#)main.h 1.2 93/04/21 Copyright (c)1993 thalerd */
void wputchar PROTO((char c));
void wputs    PROTO((char *s));
void wfputs   PROTO((char *s,FILE *fp));
void wfputc   PROTO((char c,FILE *fp));
void wgets    PROTO((char *a, char *b));
