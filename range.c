/* $Id: range.c,v 1.8 1997/08/28 00:07:48 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdlib.h> /* for atoi */
#include <string.h>
#include <time.h>
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#include "yapp.h"
#include "struct.h"
#include "item.h"
#include "range.h"
#include "globals.h"
#include "dates.h"
#include "macro.h"
#include "lib.h"
#include "sum.h"
#include "stats.h"
#include "xalloc.h"   /* for xfree */

/* 
         if (part[i].nr < sum[i].nr) st->i_newresp++;
 * the problem with the following (^^^?) is that we may have recently read A
   response, but not ALL the responses -- Ok acc to Russ
 * 
 * Latest from refresh_stats:
 *  else if (part[i].last < sum[i].last) st->i_newresp++;
 */
/*
Changed per Russ's suggestion:
The motivation is so that your own responses can advance the
timestamp but not the last-seen response number, thus showing
your own response to you only *after* an addtional response
has been made (altering the timestamp again).  If you do not
have this feature, you have the (rather ugly) alternatives
of items instantly becoming "new" again when you respond
to them [if you advance neither], or of the system not showing you 
your own response to establish the context for the followup [if
you advance the # resp].

Not:
   if ((spec & OF_NEWRESP) && p->nr && (abs(p->nr) < s->nr)) return 1;

Problem is that item comes up as new when there are no new responses.
*/

/* The problem with this one is that items come up as new when
 * some response is censored or scribbled.
 * As Marcus conceived it, this is a feature, not a bug.  If a response
 * is censored (especially by an authority), it should call attention
 * to itself.  Such is the idea as I remember it.  --Russ
 * if (p->nr && (p->last < s->last)) return 1;
 */

/*
 * Test to see if an item is newresp.  If SENSITIVE, then censored/scribbled
 * items come up new also, even if there are no new responses.
 */
int
is_newresp(p, s)
   partentry_t *p;
   sumentry_t  *s;
{
   if ((flags & O_SENSITIVE) && p->nr && (p->last < s->last)) return 1;
   if (p->nr && (abs(p->nr) < s->nr) && (p->last < s->last))  return 1;
   return 0;
}

/*
 * if (part[i].last < sum[i].last) *
 * if (st->parttime < sum[i].last) before change new partfile *
 * Latest from refresh_stats:
 *  if (part[i].last < sum[i].last)
 */

/* Test to see if an item is brandnew */
int
is_brandnew(p, s)
   partentry_t *p;
   sumentry_t  *s;
{
   return ((!p->nr && s->nr) && (p->last < s->last));
}

/*****************************************************************************/
/* TEST WHETHER ITEM IS COVERED BY THE SPECIFIED SUBSET PARAMETERS           */
/*****************************************************************************/
char                        /* OUT: 1 if in subset, 0 else */
cover(i,idx,spec,act,sum,part,st) /* ARGUMENTS: */
   short        i;          /* IN : Item number                  */
   short        idx;        /* IN : Conference index             */
   short        spec;       /* IN : Specifiers                   */
   short        act;        /* IN : Action flag                  */
   sumentry_t  *sum;        /*      Item summary                 */
   partentry_t *part;       /*      User participation info      */
   status_t    *st;
{
   sumentry_t *s;
   partentry_t *p;
   char newstring[MAX_LINE_LENGTH];

   /* if (spec & OF_NONE) return 0; */
   if (act==A_SKIP) 
      return 0;

   /* I'm guessing the following refresh is so we catch changes while 
    * reading items. -dgt 8/22/97
    */
   refresh_sum(i,idx,sum,part,st); /* added for sno */

   s = &(sum[i-1]);
   p = &(part[i-1]);
   if (!s->flags) return 0;
   if (st->string[0]) {
      strcpy(newstring, lower_case(st->string));
      if (!strstr(lower_case(get_subj(idx,i-1,sum)), newstring)) 
         return 0;
   }
   if (st->author[0] &&  strcmp(get_auth(idx,i-1,sum),st->author)) 
		return 0;
   if (st->since  > s->last) return 0; 
   if (st->before < s->last && st->before>0) return 0; 
   if (st->rng_flags && !(s->flags & st->rng_flags)) return 0;
   if (act==A_FORCE) return 1; /* force takes precedence over forgotten */
/* 
if (s->flags & IF_FORGOTTEN)
printf("<%d:%d:%d:%d>",i,spec & (OF_NOFORGET|OF_FORGOTTEN|OF_RETIRED),
                       flags & O_FORGET,
                       s->flags & (IF_FORGOTTEN|IF_RETIRED));
*/
   if (!(spec & (OF_NOFORGET|OF_FORGOTTEN)) && (flags & O_FORGET) 
    && (s->flags & IF_FORGOTTEN))
      return 0;
   if (!(spec & (OF_NOFORGET|OF_RETIRED)) && (s->flags & IF_RETIRED)) 
      return 0;
   if (!(spec & (OF_NOFORGET|OF_EXPIRED)) && (s->flags & IF_EXPIRED)) 
      return 0;

   /* Process NEW limitations */
   if ((spec & OF_NEWRESP) && is_newresp(p,s)) 
      return 1;
   if ((spec & OF_BRANDNEW) && is_brandnew(p,s)) 
      return 1;
   if  (spec & (OF_NEWRESP|OF_BRANDNEW)) 
      return 0;
   if ((spec & OF_UNSEEN) && p->nr) return 0;
   if ((spec & OF_FORGOTTEN) && !(s->flags & IF_FORGOTTEN)) return 0;
   if ((spec & OF_RETIRED) && !(s->flags & IF_RETIRED)) return 0;

   /* if (spec & OF_NEXT) spec |= OF_NONE; */
   return 1;
}

/*****************************************************************************/
/* MARK A RANGE OF ITEMS TO BE ACTED UPON                                    */
/*****************************************************************************/
static void                     /* RETURNS: (nothing) */
markrange(bg,nd,act,sum,st,val) /* ARGUMENTS: */
short      bg;                  /*    Beginning of range */
short      nd;                  /*    End of range */
short      val;                 /*    Action value to set */
char       act[MAX_ITEMS];      /*    Action array to fill in */
sumentry_t sum[MAX_ITEMS];      /*    Summary of item info    */
status_t  *st;                  /*    Conference statistics   */
{
   short j;

   if (bg<st->i_first)
      printf("%s #%d is too small (first %d)\n",topic(1), bg,st->i_first);
   else if (nd>st->i_last)
      printf("%s #%d is too big (last %d)\n",topic(1), nd,st->i_last);
   else {
      for (j=bg; j<=nd; j++) 
         if (sum[j-1].flags) 
            act[j-1]=val;
      if (bg==nd && !sum[bg-1].flags)
         printf("No such %s!\n", topic(0));
   }
}

/*****************************************************************************/
/* CONVERT A TOKEN TO AN INTEGER                                             */
/*****************************************************************************/
short                /* RETURNS: item number     */
get_number(token,st) /* ARGUMENTS:                */
char      *token;    /*    Field to process      */
status_t  *st;       /*    Conference statistics */
{
   short a,b;

   if (match(token,"fi_rst")
    || match(token,"^"))      return st->i_first;
   if (match(token,"l_ast")
    || match(token,"$"))      return st->i_last;
   if (match(token,"th_is")
    || match(token,"cu_rrent")
    || match(token,"."))      return st->i_current;
   if (sscanf(token,"%hd.%hd",&a,&b)==2) {
      if (b>=0 && b<sum[a-1].nr) st->r_first = b;
      return a;
   }
   return atoi(token);
}

void
rangearray(argv,argc,start,fl,act,sum,st) 
char     **argv;
int        argc,start;
short     *fl;                   /*    Flags to use */
char       act[MAX_ITEMS];       /*    Action array to fill in */
sumentry_t sum[MAX_ITEMS];       /*    Item summary info array */
status_t  *st;                   /*    Conference statistics */
{
   short i;

   for (i=start; i<argc; i++) {
      if (match(argv[i],"si_nce") || match(argv[i],"S="))
         st->since  = since(argc,argv,&i);
      else if (match(argv[i],"before") || match(argv[i],"B="))
         st->before = since(argc,argv,&i);
      else if (match(argv[i],"by") || match(argv[i],"A=")) {
         if (i+1 >= argc) 
            printf("Invalid author specified.\n");
         else 
            strcpy(st->author, argv[++i]);
      } else if (!match(argv[i],"F=")) /* skip "F=" */
         rangetoken(argv[i],fl,act,sum,st);
   }
}

/*****************************************************************************/
/* PARSE ONE FIELD OF A RANGE SPECIFICATION                                  */
/*****************************************************************************/
void                             /* RETURNS: (nothing) */
rangetoken(token,flg,act,sum,st) /* ARGUMENTS: */
   char      *token;             /*    Field to process */
   short     *flg;               /*    Flags to use */
   char       act[MAX_ITEMS];    /*    Action array to fill in */
   sumentry_t sum[MAX_ITEMS];    /*    Item summary info array */
   status_t  *st;                /*    Conference statistics */
{
   short fl,a,b,c;
   char  buff[MAX_LINE_LENGTH],*bp,**arr;

   if (debug & DB_RANGE)
      printf("rangetoken: '%s'\n",token);
   if ((bp=expand(token,DM_PARAM)) != NULL) {
      arr = explode(bp," ", 1);
      rangearray(arr,xsizeof(arr),0,flg,act,sum,st);
      xfree_array(arr);
      return;
   }
   fl = *flg;

   if        (match(token,"a_ll")
    ||        match(token,"*")) { 
      markrange(st->i_first, st->i_last,act,sum,st,A_COVER);
      fl |=  OF_RANGE; /* items specified */
   } else if (match(token,"nex_t")) {    /*  fl |=  OF_NEXT; */
      markrange((short)(st->i_current+1),st->i_last,act,sum,st,A_COVER);
      fl |=  OF_RANGE; /* KKK */
   } else if (match(token,"pr_evious")) {    fl |=  /* OF_NEXT | */ OF_REVERSE;
      markrange(st->i_first,(short)(st->i_current-1),act,sum,st,A_COVER);
      fl |=  OF_RANGE; /* KKK */
   } else if (match(token,"n_ew")) {         fl |=  OF_BRANDNEW | OF_NEWRESP;
   } else if (match(token,"nof_orget")) {    fl |=  OF_NOFORGET;
   } else if (match(token,"p_ass")) {        fl |=  OF_PASS;
   } else if (match(token,"d_ate")) {        fl |=  OF_DATE;
   } else if (match(token,"nor_esponse")) {  fl |=  OF_NORESPONSE;
   } else if (match(token,"u_id")) {         fl |=  OF_UID;
   } else if (match(token,"nod_ate")) {      fl &= ~OF_DATE;
   } else if (match(token,"nou_id")) {       fl &= ~OF_UID;
   } else if (match(token,"bra_ndnew")) {    fl |=  OF_BRANDNEW;
   } else if (match(token,"newr_esponse")) { fl |=  OF_NEWRESP;
   } else if (match(token,"r_everse")) {     fl |=  OF_REVERSE;
   } else if (match(token,"s_hort")) {       fl |=  OF_SHORT;
   } else if (match(token,"nop_ass")) {      fl &= ~OF_PASS;
   } else if (match(token,"nu_mbered")) {    fl |=  OF_NUMBERED;
   } else if (match(token,"nonu_mbered")) {  fl &= ~OF_NUMBERED;
   } else if (match(token,"unn_umbered")) {  fl &= ~OF_NUMBERED;
   } else if (match(token,"o_ld")) {         /* KKK */
   } else if (match(token,"exp_ired")) {     fl |=  OF_EXPIRED;
   } else if (match(token,"ret_ired")) {     fl |=  OF_RETIRED;
   } else if (match(token,"for_gotten")) {   fl |=  OF_FORGOTTEN;
   } else if (match(token,"un_seen")) {      fl |=  OF_UNSEEN;
   } else if (match(token,"linked")) {       st->rng_flags |= IF_LINKED;
   } else if (match(token,"frozen")) {       st->rng_flags |= IF_FROZEN;
   } else if (match(token,"force_response")
      ||      match(token,"force_respond")){ /* KKK */
   } else if (match(token,"respond")) {      /* KKK */
   } else if (match(token,"form_feed")       
      ||      match(token,"ff")) {           /* KKK */
   } else if (match(token,"lo_ng")) {        fl &= ~OF_SHORT;
   } else if (token[0]=='"') { /* "string" */
      strcpy(st->string, token+1);
      if (st->string[ strlen(token)-2 ]=='"')
         st->string[ strlen(token)-2 ]='\0';
   } else if (strchr(token,',')) {
      arr = explode(token,",", 1);
      rangearray(arr,xsizeof(arr),0,flg,act,sum,st);
      xfree_array(arr);
      return;
   } else if (token[0]=='-') {
      a = get_number(token+1,st);
      markrange(st->i_first,a,act,sum,st,A_COVER);
      fl |= OF_RANGE; /* range specified */
   } else if (token[strlen(token)-1]=='-') {
      strcpy(buff,token);
      buff[strlen(buff)-1]='\0';
      a = get_number(buff,st);
      markrange(a,st->i_last,act,sum,st,A_COVER);
      fl |= OF_RANGE; 
   } else if ((bp=strchr(token,'-')) != NULL) {
      strncpy(buff,token,bp-token);
      buff[bp-token]='\0';
      a = get_number(buff,st);
      b = get_number(bp+1,st);
      if (b<a) { c=b; b=a; a=c; fl ^= OF_REVERSE; }
      markrange(a,b,act,sum,st,A_COVER);
      fl |= OF_RANGE;
/*
   } else if (sscanf(token,"-%hd",&a)==1) { 
      markrange(st->i_first,a,act,sum,st,A_COVER);
      fl |= OF_RANGE; 
   } else if (sscanf(token,"%hd-%hd",&a,&b)==2) { 
      if (b<a) { c=b; b=a; a=c; fl ^= OF_REVERSE; }
      markrange(a,b,act,sum,st,A_COVER);
      fl |= OF_RANGE;
   } else if (sscanf(token,"%hd%c",&a,&ch)==2 && ch=='-') { 
      markrange(a,st->i_last,act,sum,st,A_COVER);
      fl |= OF_RANGE; 
   } else if (sscanf(token,"%hd",&a)==1) { 
      markrange(a,a,act,sum,st,A_FORCE);
      fl |= OF_RANGE; 
*/
   } else if ((a=get_number(token,st)) != 0) {
      markrange(a,a,act,sum,st,A_FORCE);
      fl |= OF_RANGE; 
   } else {
      strcpy(st->string, token);
/* KKK printf("Bad token type in getrange\n"); */
   }
   *flg = fl;
}

time_t              /* OUT: timestamp                */
since(argc,argv,ip)
   int    argc;     /* IN : number of fields         */
   char **argv;     /* IN : fields holding date spec */
   short *ip;       /* IN/OUT: prev field #          */
{
   short i,j,start=0;
   char buff[MAX_LINE_LENGTH],*ptr;
   int where[MAX_ARGS];
   time_t t;

   i = *ip + 1;
   if (i>=argc) {
      printf("Bad date near \"<newline>\"\n");
      return LONG_MAX; /* process nothing */
   }
   /* if (!strncasecmp(argv[i],"S=",2) || !strncasecmp(argv[i],"B=",2)) start+=2; */
   if (argv[i][start]=='"') {
      if (argv[i][ strlen(argv[i])-1 ]=='"')
          argv[i][ strlen(argv[i])-1 ]='\0';
      do_getdate(&t,argv[i]+start+1);
   } else {
      where[i]=0;
      strcpy(buff,argv[i]+start);
      for (j=i+1; j<argc; j++) {
	 strcat(buff," ");
         where[j]=strlen(buff);
	 strcat(buff,argv[j]);
      }
      ptr=do_getdate(&t,buff);
      for ( ; ptr-buff > where[i] && i<argc; i++);
      i--;
   }
   if (debug & DB_RANGE)
      printf("Since set to %s",ctime(&t));
   *ip = i;
   return t;
}

void
rangeinit(st,act)
status_t *st;
char act[MAX_ITEMS];
{
   short i;

   st->string[0]= st->author[0] = '\0';
   st->since    = st->before = st->opt_flags = 0;
   st->rng_flags = 0;
/* flags |= O_FORGET;  commented out 8/18 since it breaks 'set noforget;r' */
   for (i=0; i<MAX_ITEMS; i++) act[i]=0;
}

/*****************************************************************************/
/* PARSE ONE FIELD OF A RANGE SPECIFICATION                                  */
/* Note that we need sum passed in because linkfrom does ranges in other     */
/* conference                                                                */
/*****************************************************************************/
void                            /* RETURNS: (nothing) */
range(argc,argv,fl,act,sum,st,bef)  /* ARGUMENTS: */
   int        argc;             /*    Number of arguments */
   char     **argv;             /*    Argument list       */
   short     *fl;               /*    Flags to use */
   char       act[MAX_ITEMS];   /*    Action array to fill in */
   sumentry_t sum[MAX_ITEMS];   /*    Item summary info array */
   status_t  *st;               /*    Conference statistics */
   int        bef;
{
   rangearray(argv,argc,bef+1,fl,act,sum,st);
}
