/* ARCH.H: @(#)arch.h 1.5 93/06/07 Copyright (c)1993 thalerd */
void get_resp PROTO((FILE *fp, response_t *re, SHORT fast, SHORT num));
void get_item PROTO((FILE *fp, SHORT n, response_t *re, sumentry_t *sum));

#define GR_ALL    0x0000
#define GR_OFFSET 0x0001
#define GR_HEADER 0x0002
