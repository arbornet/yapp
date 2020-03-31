/* $Id: sep.c,v 1.19 1997/02/10 23:24:09 thaler Exp $ */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* This module takes care of most of the fancy output, and allows
 * users and administrators to completely customize the output
 * of many functions.  A "separator" string is passed to confsep
 * or itemsep, which break it up and generate output based on the
 * codes therein.  For more information, do "help separators"
 * from within the program.
 */
#include <stdio.h>
#include <stdlib.h> /* for atoi */
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "yapp.h"
time_t time PROTO((time_t *tloc));
#include "struct.h"
#include "item.h"
#include "range.h"
#include "globals.h"
#include "sep.h"
#include "change.h" /* to get CF_PUBLIC */
#include "lib.h"
#include "sum.h"    /* to get SF_FAST */
#include "macro.h"
#include "xalloc.h"
#include "main.h"
#include "news.h"
#include "stats.h"
#include "driver.h" /* for command */
#include "user.h"   /* for get_user */
#include "conf.h"   /* for get_desc */

static char *lchars="ntvbrface";
static char *rchars="\n\t\v\b\r\f\007\377\033";
static int lastnum=0,show[100],depth = 0,num=0,tabs=1;
static int newline,qfail,once,zero;
static char buff[MAX_LINE_LENGTH];
extern char *cfiles[];

void
init_show()
{
   show[0]=1; /* was: show[depth]=1; */
}

/******************************************************************************/
/* OUTPUT A STRING TO THE DESIRED FORMAT                                      */
/******************************************************************************/
static void                    /* RETURNS: (nothing)           */
string(b,fp)                   /* ARGUMENTS:                   */
char *b;                       /*    String to output          */
FILE *fp;                      /*    File pointer to output to */
{                              /* LOCAL VARIABLES:             */
   char fmt[MAX_LINE_LENGTH];  /*    Format string             */
   char buff[MAX_LINE_LENGTH]; /*    Formatted output          */

   if (!show[depth]) return;
   if (num)
      sprintf(fmt,"%%%ds",num);
   else
      strcpy(fmt,"%s");
   sprintf(buff,fmt,b);
   wfputs(buff,fp);
}

/*
 * This routine does:   fprintf(fp, "%s\n", str);
 * except that it wraps output at WRAPCOL columns
 */
static void
wrapout(fp, str)
   FILE *fp;
   char *str;
{ 
   int   wrapcol;
   char *line, *ptr, *p;

   if (!show[depth]) return;

   p = expand("wrap", DM_VAR);
   if (!p || (wrapcol=atoi(p))<1) {
      string(str, fp);
      return;
   }

   ptr = line = (char *)xalloc(0, wrapcol+2);
 
   for (p=str; *p; p++) {
      if (*p=='\n' || *p=='\r') {
         (*ptr)='\0';
         if (ptr>line) 
            wfputs(line, fp);
         ptr = line;
         wfputc(*p, fp);
         continue;
      }
 
      if (ptr-line == wrapcol) {
         if (*p==' ') {
            (*ptr)='\0';
            wfputs(line, fp);
            ptr=line;
   
            wfputc('\n', fp);
            continue;
         } else {
            /* Find previous space */
            char *s;
            for (s=ptr-1; s>=line && !isspace(*s); s--);
            if (s<line) {
               (*ptr)='\0';
               wfputs(line, fp);
               while (*p && !isspace(*p))
                  wfputc(*p++, fp);
               wfputc('\n', fp);
               ptr=line;
               if (!*p) p--;
               continue;
            } else {
               (*s++)=(*ptr)='\0';
               wfputs(line, fp);
               wfputc('\n', fp);
               strcpy(line, s);
               ptr -= s-line;
            }
         }  
      }  
      *ptr++ = *p;
   }
            
   /* Once we hit end of line, dump the rest but don't append a newline */
   if (ptr>line) {
      (*ptr)='\0';
      wfputs(line, fp);
   }

   xfree_string(line);
}

/******************************************************************************/
/* OUTPUT A NUMBER TO THE DESIRED FORMAT                                      */
/******************************************************************************/
static void  /* RETURNS: (nothing)   */
number(b,fp) /* ARGUMENTS:           */
short b;     /*    Number to output  */
FILE *fp;    /*    Stream to send to */
{
   char fmt[10];

   if (!show[depth]) return;
   if (num)
      sprintf(fmt,"%%%dd",num);
   else
      strcpy(fmt,"%d");
   if (!b && zero)
      sprintf(buff,"%co",("nN")[zero-1]);
   else
      sprintf(buff,fmt,b);
   wfputs(buff,fp);
   lastnum=b;
}

entity_t *
get_entity(spp)
   char **spp;
{
static entity_t ent;
   char  buff[80];
   char *sp, *str, *sub;
   sp = *spp;

   if (*sp=='"') { /* Get string */
      sp++; /* skip start quote */
      for (sub=buff; *sp && *sp!='"'; sp++,sub++)
         *sub = *sp;
      (*sub)='\0';
      if (*sp=='"') /* skip ending quote */
         sp++;
      ent.type = ET_STRING;
      ent.val.s = xstrdup(buff);

   } else if (isdigit(*sp) || *sp=='-') {        /* Get int    */
      sub=buff;
      if (*sp=='-')
         *sub++ = *sp++;
      while (*sp && isdigit(*sp))
         *sub++ = *sp++;
      (*sub)='\0';
      ent.type = ET_INTEGER;
      ent.val.i = atoi(buff);

   } else { /* Get macro */
      for (sub=buff; *sp && isalnum(*sp); sp++,sub++)
         *sub = *sp;
      (*sub)='\0';
      str = expand(buff,DM_VAR);
      if (!str) str = "";
      if (isdigit(*str) || *str=='-') {
         ent.type = ET_INTEGER;
         ent.val.i = atoi(str);
      } else {
         ent.type = ET_STRING;
         ent.val.s = xstrdup(str);
      }
   }

   *spp = sp;
   return &ent;
}

void
dest_entity(ent)
   entity_t *ent;
{
   if (ent->type==ET_STRING)
      xfree_string(ent->val.s);
}

/*
 * Convert an operator string to integer format, which some OR'ed combination
 * of the following flags:
 */
#define OP_GT   0x001
#define OP_EQ   0x010
#define OP_LT   0x100
#define OP_IN  0x1000
#define OP_NOT 0x2000
int            /* RETURNS: Integer-format operator */
opstr2int(str) /* ARGUMENTS: */
   char *str;  /*    Operator string, e.g. "<=", "==", etc */
{
   char *p;
   int not=0;
   int ret=0;

   for (p=str; *p; p++) {
      switch(*p) {
      case '~': ret|=OP_IN; break;
      case '=': ret|=OP_EQ; break;
      case '>': ret|=OP_GT; break;
      case '<': ret|=OP_LT; break;
      case '!': not=1-not;  break;
      default : return 0;
      }
   }

   if (not) {
      if (!(ret & OP_IN))
         ret = 0x111 - ret;
      else
         ret |= OP_NOT;
   }
   return ret;
}

int                        /* RETURNS: 1 if expression is true, 0 if false */
opcompare(left, op, right) /* ARGUMENTS: */
   entity_t *left;         /*    left operand */
   int       op;           /*    operator: OR'ing of operand flags */
   entity_t *right;        /*    right operand */
{
   char *lstr=NULL, *rstr=NULL;
   int   lint=0, rint=0, typ;
   char  buff[80], buff2[80];

   /* Promote int to string if needed */
   if ((op && op!=0x111) && left->type==ET_STRING && right->type==ET_INTEGER) {
      lstr = left->val.s;
      rstr = buff;
      sprintf(buff, "%d", right->val.i);
      typ = ET_STRING;
   } else if ((op && op!=0x111) && left->type==ET_INTEGER && right->type==ET_STRING) {
      rstr = right->val.s;
      lstr = buff;
      sprintf(buff, "%d", left->val.i);
      typ = ET_STRING;
   } else if (left->type==ET_STRING) { /* both strings */
      typ = ET_STRING;
      lstr = left->val.s;
      rstr = right->val.s;
   } else { /* both int's */
      if (op & OP_IN) { /* must promote both to strings */
         typ = ET_STRING;
         lstr = buff;
         sprintf(buff, "%d", left->val.i);
         rstr = buff2;
         sprintf(buff2, "%d", right->val.i);
      } else {
         typ = ET_INTEGER;
         lint = left->val.i;
         rint = right->val.i;
      }
   }

   if ( typ==ET_INTEGER) {
      if (op==0x000) /* single operand */
         return lint;
      if (op==0x111) /* single operand */
         return !lint;

      /* Two operands */
      return ((lint> rint && (op & OP_GT))
           || (lint==rint && (op & OP_EQ))
           || (lint< rint && (op & OP_LT)));
   } else { /* ET_STRING */
      if (op==0x000) /* single operand */
         return (lstr != NULL);
      if (op==0x111) /* single operand */
         return (lstr == NULL);

      /* Check for containment in list... */
      if (op & OP_IN) {
         char *little = (char *)xalloc(0, strlen(rstr)+3);
         char *big    = (char *)xalloc(0, strlen(lstr)+3);
         char *p;
         sprintf(little, " %s ", rstr);
         sprintf(big, " %s ", lstr);
         p = strstr(big, little);
         xfree_string(little);
         xfree_string(big);
         return (op & OP_NOT)? (p==NULL) : (p!=NULL);
      }

      /* Two operands */
      return ((strcmp(lstr, rstr)> 0 && (op & OP_GT))
           || (strcmp(lstr, rstr)==0 && (op & OP_EQ))
           || (strcmp(lstr, rstr)< 0 && (op & OP_LT)));
   }
}

/******************************************************************************/
/* PROCESS CONDITIONS FOR BOTH ITEM/CONF SEPS                                 */
/******************************************************************************/
static char   /* RETURNS: 1 on true, 0 on false */
misccond(spp) /* ARGUMENTS: */
char **spp;   /*    Separator string */
{
   char *sp,ret=0,not=0;

   sp = *spp;

   if (*sp=='!' || *sp=='~') { not = !not; sp++; }
   
   switch(*(sp++)) {
   case 'P': ret=(lastnum!=1); break;
   case 'S': ret= (st_glob.c_status & CS_FW); break;
   case 'l': ret= (st_glob.c_status & CS_NORESPONSE); break;
   case '(': {
      char *sub, *buff;
      entity_t left, right;
      int op;

      /* Skip whitespace */
      while (isspace(*sp)) sp++;

      /* Get left operand */
      memcpy(&left, get_entity(&sp), sizeof(left));

      /* Skip whitespace */
      while (isspace(*sp)) sp++;

      /* Get operator */
      buff = (char *)xalloc(0, strlen(sp)+1);
      for (sub=buff; *sp && strchr("<>=!~", *sp); sp++,sub++)
         *sub = *sp;
      (*sub)='\0';
      op = opstr2int(buff);
      xfree_string(buff);

      /* Skip whitespace */
      while (isspace(*sp)) sp++;

      /* Get right operand */
      memcpy(&right, get_entity(&sp), sizeof(right));

      /* Skip whitespace */
      while (isspace(*sp)) sp++;

      /* Get right paren */
      if (*sp==')') sp++;
    
      ret = opcompare(&left, op, &right);
      dest_entity(&left);
      dest_entity(&right);
      break;
   }
   default: ret=0; break; /* don't show */
   }

   *spp = sp;
   if (!show[depth]) return 0;
   return (ret!=0)^not;
}

static sumentry_t oldsum[MAX_ITEMS];
static short      oldconfidx;

/* Display new responses since last call */
void
announce_new_responses(fp)
   FILE *fp; /* IN: Stream to send to */
{
   int new=0, i;

   refresh_sum(0,confidx,sum,part,&st_glob);
   if (confidx == oldconfidx) {
      for (i=st_glob.i_first; i<st_glob.i_last; i++) {
         if (sum[i].nr > oldsum[i].nr && sum[i].last > part[i].last) {
            if (!new) {
               string("New responses were just posted to item(s):\n", fp);
               new=1;
            }
            string(" ", fp);
            number(i+1, fp);
            oldsum[i].nr = sum[i].nr;
         }
      }
      if (new)
         string("\n", fp);
   } else {
      oldconfidx = confidx;
      memcpy(oldsum, sum, MAX_ITEMS*sizeof(sumentry_t));
   }
}

/* Mark given response as not new, so our own responses don't trigger
 * the message above
 */
void
skip_new_response(c,i,nr)
   short c;  /* IN: conference index */
   short i;  /* IN: item number */ 
   short nr; /* IN: response count */
{  
   if (c == oldconfidx) {
      oldsum[i].nr = nr;
   } else {  
      oldconfidx = c;
      refresh_sum(0,c,sum,part,&st_glob);
      memcpy(oldsum, sum, MAX_ITEMS*sizeof(sumentry_t));
   }
}

/******************************************************************************/
/* PROCESS SEPS FOR BOTH ITEM/CONF SEPS                                       */
/******************************************************************************/
void            /* RETURNS: (nothing) */
miscsep(spp,fp) /* ARGUMENTS: */
char **spp;     /*    Separator string */
FILE *fp;       /*    Stream to send to */
{
   char *sp;
   short i;
   char *sub;

   sp = *spp;
   switch(*(sp++)) {

   /* Customization separators */
   case '%': string("%",fp); break;
   case 'E': if (!depth || show[depth-1]) show[depth]= !show[depth]; break;
   case 'c': newline=0; break;
/* case 'R': refresh_sum(0,confidx,sum,part,&st_glob); break; */
   case 'R': announce_new_responses(fp); break;
   case 'S': if (lastnum!=1) string("s",fp); break;
   case 'T': tabs=num; break;
   case 'X': for (i=0; i<tabs; i++) 
                if (show[depth]) wfputc(' ',fp); 
             break;
   case ')': 
/*
for (i=0; i<depth; i++) printf("   ");
printf("---\n");
*/
             depth--; break;
   case 'D': if (show[depth]) wfputs(get_date(time((time_t*)0),num),fp); break;
   case '`': /* Execute a command */
             { int tmp = once; /* save once */
               int lvl = stdin_stack_top;
                sub = xstrdup(mystrtok(sp, "`"));
                sp += strlen(sub);
                if (*sp=='`') sp++;
                once = 0;
                command(sub,1);
                once = tmp;
                xfree_string(sub);
                while (stdin_stack_top > lvl)
                   pop_stdin();
             }
             break;

   default:  break; /* do nothing */
   }

   *spp = sp;
}

/******************************************************************************/
/* PROCESS CONDITIONS FOR ITEM SEPS ONLY                                      */
/******************************************************************************/
char             /* RETURNS: 1 true, 0 false */
itemcond(spp,fl) /* ARGUMENTS:               */
char **spp;      /*    Separator string      */
long  fl;        /*    Sep flags             */
{
   char *sp,ret=0,not=0;
   response_t *cre;

   sp = *spp;
   cre = &(re[st_glob.r_current]);

   if (*sp=='!' || *sp=='~') { not = !not; sp++; }
   for (num=0; isdigit(*sp); sp++) num= num*10+(*sp-'0'); 
   
   switch(*(sp++)) {
   case 'B': ret=((once & IS_START)>0); once&= ~IS_START; break;
   case 'D': ret=((once & IS_DATE)>0); break;
#ifdef NEWS
   case 'E': ret=(( cre->flags & RF_EXPIRED)>0); break;
#endif
   case 'F': ret=((fl & OF_NUMBERED) || (flags & O_NUMBERED)); break;
/* case 'I': (see 'O') */
   case 'L': ret=(st_glob.l_current>=0 && cre->text 
              &&  st_glob.l_current<xsizeof(cre->text)
              &&  cre->text[st_glob.l_current]); break;
   case 'N': ret=(st_glob.r_current>0); 
/*           ret= (!(!part[st_glob.i_current-1].nr && sum[st_glob.i_current-1].nr) 
                  && (part[st_glob.i_current-1].nr  < sum[st_glob.i_current-1].nr));
*/
             break;
   case 'I': /* 
			 	 ret=(!part[st_glob.i_current-1].nr && sum[st_glob.i_current-1].nr);
             break; fall through into O 
				 */
   case 'O': ret=((once & IS_ITEM)>0); once &= ~IS_ITEM;   break;
   case 'p': ret=(cre->parent>0);      once &= ~IS_PARENT; break;
	case 'r': ret=(st_glob.r_current>=0); break;
   case 'R': ret=((once & (IS_ITEM|IS_RESP))>0);  
             once&= ~(IS_ITEM|IS_RESP);  
/*
             ret= ((!part[st_glob.i_current-1].nr && sum[st_glob.i_current-1].nr) 
                 || (part[st_glob.i_current-1].nr  < sum[st_glob.i_current-1].nr));
*/
             break;
   case 'T': ret=((fl & OF_FORMFEED)>0); break;
   case 'U': ret=((once & IS_UID)>0); break;
   case 'V': ret=(( cre->flags & RF_CENSORED)>0); break;
   case 'W': ret=(( cre->flags & RF_SCRIBBLED)>0); break;
   case 'X': ret=((sum[st_glob.i_current-1].flags & IF_RETIRED)>0); 
             once &= ~IS_RETIRED;
             break;
   case 'x': ret= (once & num); /* once &= ~num; */ break;
   case 'Y': ret=((sum[st_glob.i_current-1].flags & IF_FORGOTTEN)>0); 
             once &= ~IS_FORGOTTEN;
             break;
   case 'Z': ret=((sum[st_glob.i_current-1].flags & IF_FROZEN)>0); 
             once &= ~IS_FROZEN;
             break;
   default: return misccond(spp);
   }

   *spp = sp;
   if (!show[depth]) return 0;
   return (ret!=0)^not;
}

/******************************************************************************/
/* PROCESS SEPS FOR ITEM SEPS ONLY                                            */
/* This works only for the current conference                                 */
/******************************************************************************/
void                /* RETURNS: (nothing) */
itemsep2(spp,fl,fp) /* ARGUMENTS: */
char **spp;         /*    Separator string */
long *fl;           /*    Flags (see sep.h) */
FILE *fp;           /*    Stream to output to */
{
   char *sp;
   char *sub,neg=0;
   response_t *cre;
   int cap=0;

   cre = &(re[st_glob.r_current]);
   sp = *spp;
   buff[0]=0;
   num=0;

   /* Get number */
   zero=0;
   if (*sp == '^') { cap=1; sp++; }
   if (*sp == 'z') { zero=1; sp++; }
   else if (*sp == 'Z') { zero=2; sp++; }
   if (*sp == '-') { neg=1; sp++; }
   while (isdigit(*sp)) { num= num*10+(*sp-'0'); sp++; }
   if (neg) num = -num;

   switch(*(sp++)) {

   /* Item Function Codes */
   case 'a': string(cre->fullname,fp);    break;
   case 'C': if (confidx>=0) string(compress(conflist[confidx].name),fp); break;
   case 'h': string(get_subj(confidx,st_glob.i_current-1,sum),fp);       break;
   case 'i': number(st_glob.i_current,fp); break;
   case 'l': string(cre->login,fp);       break;
   case 'e': {
      int  tuid=0;
      char tlogin[MAX_LINE_LENGTH];
      char temail[MAX_LINE_LENGTH];
      char tfullname[MAX_LINE_LENGTH];
      char thome[MAX_LINE_LENGTH];
      if (cre->login) {
         char *at;
         strcpy(tlogin, cre->login);
         at=strchr(tlogin, '@');
         if (at && !strcmp(at+1, hostname))
            *at = '\0';
         tuid = cre->uid;
         if (get_user(&tuid, tlogin, tfullname, thome, temail))
            string(temail,fp);       
         else
            string(cre->login,fp);
      } else
         strcpy(temail, email); 
      break;
   }
   case 'L': wrapout(fp,cre->text[st_glob.l_current]); break;
#ifdef NEWS
   case 'm': string(message_id(compress(conflist[confidx].name),
	       st_glob.i_current,st_glob.r_current,re),fp); break;
#endif
   case 'n': number(sum[st_glob.i_current-1].nr - 1,fp);     break;
   case 'N': number(st_glob.l_current+1,fp); break;
   case 'r': number(st_glob.r_current,fp); break;
   case 's': number((cre->flags & (RF_SCRIBBLED|RF_EXPIRED))? 0 : xsizeof(cre->text),fp); 
             break;
   case 'k': number((cre->numchars+1023)/1024,fp); break;
   case 'q': number(cre->numchars,fp); break;
   case 'K': /* KKK */ break;
   case 'Q': /* KKK */ break;
   case 'u': number((short)cre->uid,fp); 
             /* *fl &= ~OF_UID; */
             once &= ~IS_UID; break;
   case 'd': if (show[depth]) wfputs(get_date(cre->date,num?num:1),fp); 
             /* *fl &= ~OF_DATE; */
             once &= ~IS_DATE; break;
   case 't': if (show[depth]) wfputs(get_date(cre->date,num),fp); 
             /* *fl &= ~OF_DATE; */
             once &= ~IS_DATE; break;
   case 'p': number((short)cre->parent-1,fp); once&=~IS_PARENT; break;
   case '<': { int tmp = once; /* save once */
                sub = mystrtok(sp, ">");
                sp += strlen(sub);
                if (*sp=='>') sp++;
                once = 0;
                /* itemsep(capexpand(sub,DM_VAR,cap),1); */
                fitemsep(fp, capexpand(sub,DM_VAR,cap),1);
                once = tmp;
             }
             break;
   case '{': { int tmp = once; /* save once */
                sub = mystrtok(sp, "}");
                sp += strlen(sub);
                if (*sp=='}') sp++;
                once = 0;
                /* itemsep(capexpand(sub,DM_VAR,cap),1); */
                fitemsep(fp, capexpand(sub,DM_VAR,cap),1);
                once = tmp;
             }
             break;
   case '(': show[depth+1]=itemcond(&sp,*fl); 
             depth++;
             break; /* ) */
   
   default:  *spp=sp-1; miscsep(spp,fp); return;
   }

   *spp = sp;
}

/******************************************************************************/
/* PROCESS CONDITIONS FOR CONF SEPS ONLY                                      */
/******************************************************************************/
char              /* RETURNS: 1 true, 0 false */
confcond(spp,idx,st) /* ARGUMENTS:          */
char **spp;       /*    Separator string */
short idx;        /*    Conference index */
status_t *st;
{
   char buff[MAX_LINE_LENGTH];
   struct stat stt;
   char *sp,ret=0,not=0;

   sp = *spp;

   if (*sp=='!' || *sp=='~') { not = !not; sp++; }
   for (num=0; isdigit(*sp); sp++) num= num*10+(*sp-'0'); 
/*
int i;
for (i=0; i<depth; i++) printf("   ");
printf("%1d: %c ",i,*sp);
*/
   switch(*(sp++)) {
   case 'y': lastnum=st->i_unseen; ret=lastnum; break;
   case 'n': lastnum=st->i_brandnew+st->i_newresp; ret= lastnum; break;
   case 'b': lastnum=st->i_brandnew; ret= lastnum; break;
   case 'r': lastnum=st->i_newresp; ret= lastnum; break;
   case 'm': ret= (status & S_MAIL);  break;
   case 'x': ret= (once & num); /* once &= ~num; */ break;
   case 'N': if (num>=0 && num<CF_PUBLIC && idx>=0) {
                sprintf(buff,"%s/%s",conflist[idx].location,compress(cfiles[num]));
                if (stat(buff,&stt) || stt.st_size<=0) ret=0;
                else if (st->c_status & CS_JUSTJOINED) ret=1;
                else ret=(stt.st_mtime > st->parttime);
             }
             break;
   case 'F': if (num>=0 && num<CF_PUBLIC && idx>=0) {
                sprintf(buff,"%s/%s",conflist[idx].location,compress(cfiles[num]));
                ret=!stat(buff,&stt);
             }
             break;
   case 'O': ret= (st->c_status & CS_OTHERCONF)?1:0; break;
   case 'C': ret= (idx>=0); break;
   case 'i': ret= (st->i_first<=st->i_last); break;
   case 's': ret= (st->c_status & CS_FW); break;
   case 'f': if (num>=0 && idx>=0) {
                sprintf(buff,"%s/sum",conflist[idx].location);
                ret = !stat(buff,&stt);
             }
             break;
   case 'j': ret= (st->c_status & CS_JUSTJOINED)?1:0; break;
/* case 'l': ret= (st->c_status & CS_NORESPONSE); break; */
   case 'B': ret= (idx == confidx); break;
   case 'k': ret= (once & IS_CFIDX); /* once &= ~IS_CFIDX; */ break;

   default: return misccond(spp);
   }
/*
printf("%d\n",ret);
*/
   *spp = sp;
   if (!show[depth]) return 0;
   return (ret!=0)^not;
}

/******************************************************************************/
/* PROCESS SEPS FOR CONF SEPS ONLY                                            */
/******************************************************************************/
void
confsep2(spp,idx,st,part,fp) /* ARGUMENTS: */
char **spp;               /*    Separator string */
partentry_t *part;        /*    User participation info */
short idx;                /*    Conference index */
status_t *st;
FILE *fp;                 /*    Stream to output to */
{
   char  *sp,*sub,*sh,*sh2,neg=0;
   time_t t;
   char **config;
   int cap=0;

   sp = *spp;
   num=0;

   /* Get number */
   zero=0;
   if (*sp == '^') { cap=1; sp++; }
   if (*sp == 'z') { zero=1; sp++; }
   else if (*sp == 'Z') { zero=2; sp++; }
   if (*sp == '-') { neg=1; sp++; }
   while (isdigit(*sp)) { num= num*10+(*sp-'0'); sp++; }
   if (neg) num = -num;

   switch(*(sp++)) {

   /* Conference separators */
#ifdef NEWS
   case 'A': number(st->c_article, fp); break;
#endif
   case 'y': number(st->i_unseen,fp); break;
   case 'n': number(st->i_brandnew+st->i_newresp,fp); break;
   case 'b': number(st->i_brandnew,fp); break;
   case 'C': string(st->string,fp); break;
   case 'r': number(st->i_newresp,fp); break;
   case 'N': number(st->r_totalnewresp,fp); break;
   case 'k': number(st->count,fp); break;
   case 'u': string(fullname_in_conference(st),fp); break;
   case 'v': string(login,fp); break;
   case 'w': string(work,fp); break;
   case 'f': number(st->i_first,fp); break;
   case 'L': if (idx>=0) string(get_desc(compress(conflist[idx].name)),fp); break;
   case 'l': number(st->i_last,fp); break;
   case 'Q': if (idx<0) { string("Not in a conference!",fp); qfail=1; } break;
   case 'i': number(st->i_numitems,fp); break;
   case 't': number((short)st->c_security & CT_VISIBLE,fp); break;
   case 's': if (idx>=0) string(compress(conflist[idx].name),fp); break;
   case 'p': if ((config = get_config(idx)) != NULL)
				    string(config[CF_PARTFILE],fp); 
				 break;
   case 'd': if (idx>=0) string(conflist[idx].location,fp); break;
   case 'q': if (idx>=0) {
                sh=conflist[idx].location;
                for (sh2=sh+strlen(sh)-1; sh2>=sh && *sh2!='/'; sh2--);
                string(sh2+1,fp);
             }
             break;
   case 'o': if (show[depth]) wfputs(get_date(st->parttime,num),fp); break;
   case 'm': /* NEW: lastmod of sum file, if any */
/*
             if (idx<0) t=0;
             else {
                sprintf(buff,"%s/sum",conflist[idx].location);
                t= (stat(buff,&stt))? 0 : stt.st_mtime;
             }
*/
             t = st->sumtime;
             if (show[depth]) wfputs(get_date(t,num),fp); 
             break;
   case 'g': if (num>=0 && num<CF_PUBLIC && show[depth] && idx>=0) 
                more(conflist[idx].location,compress(cfiles[num]));
             break; 
   case '<': { int tmp = once; /* save once */
                sub = mystrtok(sp, ">");
                sp += strlen(sub);
                if (*sp=='>') sp++;
                once = 0;
                confsep(capexpand(sub, DM_VAR,cap),idx,st,part,1);
                once = tmp;
             }
             break;
   case '{': { int tmp = once; /* save once */
                sub = mystrtok(sp, "}");
                sp += strlen(sub);
                if (*sp=='}') sp++;
                once = 0;
                confsep(capexpand(sub, DM_VAR,cap),idx,st,part,1);
                once = tmp; 
             }
             break;
   case '(': /* Get number */
             /* for (num=0; isdigit(*sp); sp++) num= num*10+(*sp-'0'); */
             show[depth+1]=confcond(&sp,idx,st); /* for ultrix */
             depth++;
             break; /* ) */

   default:  *spp=sp-1; miscsep(spp,fp); return;
   }

   *spp = sp;
}

/******************************************************************************/
/* SET "ONCE-ONLY" FLAGS VALUE                                                */
/******************************************************************************/
void       /* RETURNS: (nothing) */
sepinit(x) /* ARGUMENTS:         */
short x;   /*    Flags to set    */
{
   once |= x;
}

void
fitemsep(fp,sep,fl)
   FILE *fp;
   char *sep;   /*    Separator variable */
   int   fl;    /*    Force %c? */
{
   char       *sp,*tp;
   response_t *cre;
   char       *str;
   char        buff[MAX_LINE_LENGTH];
   int         start_depth = depth, start_show = show[depth];
   int  start_newline = newline;
   
   if (!sep) return;
   str = sep;

   /* Force %c */
   if (fl) {
      sprintf(buff,"%s%%c",str);
      str = buff;
   }

   /* get status without trashing subj's in memory */
   cre = &(re[st_glob.r_current]);

   init_show();
   newline=1;
   sp=str;

   for(;;) {
      switch (*sp) {
      case '$': {  char *sub;
                   int   cap=0;
                   if (sp[1]=='^') {
                      cap=1;
                      sp++;
                   }
                   if (sp[1]!='{') {
                      if (show[depth]) wfputc(*sp++,fp);
                      else             sp++;
                   } else {
                      sp+=2;
                      sub = mystrtok(sp, "}");
                      sp += strlen(sub);
                      if (*sp=='}') sp++;
                      if (show[depth]) {
                         if ((sub = capexpand(sub, DM_VAR,cap)) != NULL)
                            wfputs(sub, fp);
                      }
                   }
                   break;
      }
      case '%':   sp++; 
                  itemsep2(&sp,&st_glob.opt_flags,fp);
                  break;
      case '\0':  if ((once & IS_UID)  && ((st_glob.opt_flags & OF_UID ) || (flags & O_UID )))
                     fprintf(fp," uid %d",cre->uid);
                  if ((once & IS_DATE) && ((st_glob.opt_flags & OF_DATE) || (flags & O_DATE))) 
                     fprintf(fp," on %.24s",get_date(cre->date,0));
                  if ((once & IS_RETIRED) 
                  && (sum[st_glob.i_current-1].flags & IF_RETIRED))
                     fprintf(fp,"\n   <%s is retired>", topic(0));
                  if ((once & IS_FORGOTTEN) 
                  && (sum[st_glob.i_current-1].flags & IF_FORGOTTEN))
                     fprintf(fp,"\n   <%s is forgotten>",topic(0));
                  if ((once & IS_FROZEN) 
                  && (sum[st_glob.i_current-1].flags & IF_FROZEN))
                     fprintf(fp,"\n   <%s is frozen>",topic(0));
#if 0
                  if ((once & IS_PARTY) 
                  && (sum[st_glob.i_current-1].flags & IF_PARTY))
                     fprintf(fp,"\n   <synchronous (party) %s>",topic(0));
#endif
                  if ((once & IS_LINKED) 
                  && (sum[st_glob.i_current-1].flags & IF_LINKED))
                     fprintf(fp,"\n   <linked %s>",topic(0));
                  if ((once & IS_PARENT) 
                  && (cre->parent>0))
                     fprintf(fp,"   <response to #%d>",cre->parent-1);

                  if (once & IS_CENSORED) {
                     if (cre->flags & RF_EXPIRED)
                        wfputs("   <expired>",fp);
                     else if (cre->flags & RF_SCRIBBLED) {
                        if (cre->numchars>8 && cre->text 
			 && (flags & O_SCRIBBLER)) {
                           char buff[MAX_LINE_LENGTH];
                           short i;

                           for (i=0; i<8 && ((char*)cre->text)[i]!=' '; i++)
                              buff[i] = ((char*)cre->text)[i];
                           buff[i] = '\0';
                        
                           fprintf(fp,"   <censored & scribbled by %s>",buff);
                        } else {
                           int tmp = once; /* save once */
                           once = 0;
                           itemsep(expand("scribbled",DM_VAR),1);
                           once = tmp;
                           /* wfputs(expand("scribbled", DM_VAR),fp); */
                        }
                     } else if (cre->flags & RF_CENSORED) {
                        int tmp = once; /* save once */
                        once = 0;
                        itemsep(expand("censored",DM_VAR),1);
                        once = tmp;
                        /* wfputs(expand("censored", DM_VAR),fp); */
                     }
		  }

                  if (newline) wfputc('\n',fp);
                  once=0;
                  if (fp) 
                     fflush(fp); /* flush when done with wfput stuff */
                  depth = start_depth; /* restore original depth */
                  show[depth] = start_show;
                  newline = start_newline;
                  return;
      case '\\':  /* Translate lchar into rchar */
                  sp++; 
                  tp=strchr(lchars,*sp); 
                  if (tp) {    /* if *sp is 0 byte, will insert a 0 byte */
                     if (show[depth]) wfputc(rchars[tp-lchars],fp);
                     sp++;
                     break;
                  } /* else fall through into default */
      default:    if (show[depth]) wfputc(*sp++,fp);
                  else             sp++;
      }
   }
}

/******************************************************************************/
/* PROCESS ITEMSEP STRING                                                     */
/* Output to pipe, if one is open, else to stdout                             */
/******************************************************************************/
void         /* RETURNS: (nothing) */
itemsep(sep,fl) /* ARGUMENTS: */
char *sep;   /*    Separator variable */
int          fl;         /*    Force %c? */
{
   FILE       *fp;
   if (status & S_EXECUTE)    fp = 0;
   else if (status & S_PAGER) fp = st_glob.outp;
   else                       fp = stdout;
   fitemsep(fp, sep, fl);
}

/******************************************************************************/
/* PROCESS CONFSEP STRING                                                     */
/******************************************************************************/
void                     /* RETURNS: (nothing)                    */
confsep(sep,idx,st,part,fl) /* ARGUMENTS:                            */
char        *sep;        /*    Sep string to process              */
partentry_t *part;       /*    User participation info            */
short        idx;        /*    Index of which cf we're processing */
status_t *st;
int          fl;         /*    Force %c? */
{
   char *sp,*tp,*str;
   FILE *fp;
   char buff[MAX_LINE_LENGTH];
   int  start_depth = depth; /* save original depth */
   int  start_show = show[depth]; 
   int  start_newline = newline;

/*
   str = expand(sep,DM_VAR);
   if (!str) str=sep;
*/
   if (!sep) return;
   str = sep;
   
   /* Compatibility: force "...prompt" to end in \c */
   if (fl) {
      sprintf(buff,"%s%%c",str);
      str = buff;
   }

   if (status & S_EXECUTE)        fp = 0;
   else if (status & S_PAGER) fp = st_glob.outp;
   else                           fp = stdout;

   init_show();
   newline=1; qfail=0;
   sp=str;

   while (!qfail) {
      switch (*sp) {
      case '$': {  char *sub;
                   int   cap=0;
                   if (sp[1]=='^') {
                      cap=1;
                      sp++;
                   }
                   if (sp[1]!='{') {
                      if (show[depth]) wfputc(*sp++,fp);
                      else             sp++;
                   } else {
                      sp+=2;
                      sub = mystrtok(sp, "}");
                      sp += strlen(sub);
                      if (*sp=='}') sp++;
                      if (show[depth]) {
                         if ((sub = capexpand(sub, DM_VAR, cap)) != NULL)
                            wfputs(sub, fp);
                      }
                   }
                   break;
      }
      case '%':   sp++; 
                  confsep2(&sp,idx,st,part,fp);
                  break;
      case '\0':  if (newline) wfputc('\n',fp);
                  once=0;
                  if (fp) 
                     fflush(fp); /* flush when done with wfput stuff */
                  depth = start_depth; /* restore original depth */
                  show[depth] = start_show;
                  newline = start_newline;
                  return;
      case '\\':  /* Translate lchar into rchar */
                  sp++; 
                  tp=strchr(lchars,*sp); 
                  if (tp) {    /* if *sp is 0 byte, will insert a 0 byte */
                     if (show[depth]) wfputc(rchars[tp-lchars],fp);
                     sp++;
                     break;
                  } /* else fall through into default */
      default:    if (show[depth]) wfputc(*sp++,fp);
                  else             sp++;
                  break;
      }
   }
   if (newline) wfputc('\n',fp);
   if (fp) fflush(fp);
   depth = start_depth; /* restore original depth */
   show[depth] = start_show;
   newline = start_newline;
}

char *
get_sep(pEptr)
   char **pEptr;
{
  static char buff[MAX_LINE_LENGTH];
  char oldeval[MAX_LINE_LENGTH];
  int  tmp_status;

  tmp_status = status;
  strcpy(oldeval, evalbuf);
  switch(mode) {
  case M_RFP:
  case M_TEXT:
  case M_EDB:
     evalbuf[0] = '\0';
     status |=  S_EXECUTE;
     itemsep2(pEptr, st_glob.opt_flags, NULL);
     status &= ~S_EXECUTE;
     break;
                 
  case M_OK:
  case M_JOQ:
  default:
     evalbuf[0] = '\0';
     status |=  S_EXECUTE;
     confsep2(pEptr, confidx, &st_glob, part, NULL); 
     status &= ~S_EXECUTE;
     break;
  }
  strcpy(buff, evalbuf);
  strcpy(evalbuf, oldeval);
  status = tmp_status;

  return buff;
}
