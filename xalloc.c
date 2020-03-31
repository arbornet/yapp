/* $Id: xalloc.c,v 1.6 1997/08/28 00:07:55 thaler Exp $ */

/* XALLOC.C */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif
#include <stdlib.h> /* for malloc */
/* to get flags var for debugging only... */
#include "yapp.h"
#include "struct.h"
#include "xalloc.h"
#include "globals.h"
#include "main.h" /* for wputs */

#define NUMXST 3
static long xst[NUMXST][2]={{ 0,0 }, {0,0}, {0,0}};
#define XS_ACALLS 0
#define XS_RCALLS 1
#define XS_FCALLS 2

/* Information about each array */
typedef struct mem_tt {
   char          **ptr;
   struct mem_tt *next;
   int            num; /* # of elements */
   int            eltsize; /* size of an element */
} mem_t;
static mem_t *first_arr=0,*first_mem=0;

/******************************************************************************/
/* DUMP ALL ARRAYS CURRENTLY ALLOCATED                                        */
/******************************************************************************/
void    /* RETURNS: (nothing) */
xdump() /* ARGUMENTS: (none)  */
{
   mem_t *this;
   short i;

   printf("ARRAYS:\n");
   for (this=first_arr; this; this = this->next) {
      printf("xdump: ptr %x size %d*%d\n",this->ptr,this->num, this->eltsize); 
      for (i=0; i<this->num; i++) 
         if (this->ptr[i]) 
            printf("%4hd: %s\n", i, this->ptr[i]);
   }
   printf("\nOTHER:\n");
   for (this=first_mem; this; this = this->next) 
      printf("xdump: ptr %x size %d*%d str !%s!\n",this->ptr,this->num,
       this->eltsize, this->ptr); 
}

/* Get structure with data about array, or NULL if not found */
static mem_t *
find_array(arr, prevp)
   void *arr;
   mem_t **prevp;
{
   mem_t *this, *prev=NULL;
   for (this=first_arr; this && this->ptr != arr; prev=this,this = this->next);
   if (prevp)
      (*prevp) = prev;
   return this;
}

/* Get structure with data about string, or NULL if not found */
static mem_t *
find_string(arr, prevp)
   void *arr;
   mem_t **prevp;
{
   mem_t *this;

   if (prevp) {
      mem_t *prev=NULL;

      for (this=first_mem; this && this->ptr != arr; prev=this,this=this->next);
      (*prevp) = prev;
   } else {
      for (this=first_mem; this && this->ptr != arr; this=this->next);
   }
   return this;
}

/******************************************************************************/
/* GET # OF ELEMENTS IN AN ARRAY                                              */
/******************************************************************************/
short        /* RETURNS: size of array */
xsizeof(arr) /* ARGUMENTS: */
   void *arr;  /*    Array */
{
   mem_t *this;

   if (!arr) return 0;

   /* Return # of elements in an array */
   this = find_array(arr, NULL);
   if (this)
      return this->num;

   /* Return element size of a normal buffer */
   this = find_string(arr, NULL);
   if (this)
      return this->eltsize;

   (void)wputs("Tried to get sizeof unxalloced ptr\n");
   return 0; /* not found */
}

/******************************************************************************/
/* ALLOCATE A NEW    ARRAY                                                    */
/******************************************************************************/
char **             /* RETURNS: New array */
xalloc(num,eltsize) /* ARGUMENTS: */
   int   num;       /*    Size of array to get */
   int   eltsize;   /*    Size of each element */
{
   mem_t *this;
   char **mem;

   mem=(char **)calloc((unsigned)num?num:1,(unsigned)eltsize);
   this          = (mem_t *)malloc(sizeof(mem_t));
   this->ptr     = mem;
   this->num     = num;
   this->eltsize = eltsize;
   if (num) {
      this->next    = first_arr;
      first_arr     = this;
   } else {
      this->next    = first_mem;
      first_mem     = this;
   }

   if (debug & DB_MEMORY) 
      printf("xalloc: got %x size %d*%d\n", mem,num, eltsize);
   xst[XS_ACALLS][!num]++;
   return mem;
}

/******************************************************************************/
/* CHANGE SIZE OF A STRING                                                    */
/******************************************************************************/
char *            /* RETURNS: new array */
xrealloc_string(arr,num) /* ARGUMENTS: */
   void  *arr;       /*    Old array */
   int    num;       /*    New size (# elts for array, # bytes for string) */
{
   mem_t *this;
   char  *mem;

   this = find_string(arr, NULL);
   if (!this) {
      (void)wputs("Tried to xrealloc unxalloced string\n");
      return 0; /* not found */
   }

   if (debug & DB_MEMORY) 
      printf("xrealloc: from %x size %d*%d\n", arr, this->num, this->eltsize);

   mem=(char*)realloc((char *)arr, (unsigned)num);
   this->eltsize = num;
   this->ptr = (char**)mem;

   if (debug & DB_MEMORY) 
      printf("xrealloc:   to %x size %d*%d\n", mem,this->num, this->eltsize);

   xst[XS_RCALLS][1]++;

   return mem;
}

/******************************************************************************/
/* CHANGE SIZE OF AN ARRAY                                                    */
/******************************************************************************/
char **           /* RETURNS: new array */
xrealloc_array(arr,num) /* ARGUMENTS: */
   void  *arr;       /*    Old array */
   register int num;       /*    New size (# elts for array, # bytes for string) */
{
   mem_t *this;
   char **mem; 

   this = find_array(arr, NULL);
   if (!this) {
      (void)wputs("Tried to xrealloc unxalloced array\n");
      return 0; /* not found */
   }

   /* If making array smaller, free up elts */
#if 1 /* hopefully this is faster */
{
   register char **ptr, **done = &((char**)arr)[num];
   for (ptr=&((char **)arr)[this->num-1]; ptr>=done; ptr--) {
      if (*ptr) {
         xfree_string(*ptr);
      }
   }
}
#else
{
   register char *ptr;
   register int i;
   for (i=this->num-1; i>=num; i--) {
      if ((ptr = ((char**)arr)[i]) != NULL) {
         xfree_string(ptr);
         abort();
      }
   }
}
#endif

   if (debug & DB_MEMORY) 
      printf("xrealloc: from %x size %d*%d\n", arr, this->num, this->eltsize);

   if (num)
      mem=(char **)realloc((char *)arr,(unsigned)num * this->eltsize);
   else /* necessary on HP-UX to prevent mem=NULL or can't xfree */
      mem=(char **)realloc((char *)arr,1);
   this->num = num;
   this->ptr = mem;
   if (debug & DB_MEMORY) 
      printf("xrealloc:   to %x size %d*%d\n", mem,num, this->eltsize);

   xst[XS_RCALLS][0]++;

   return mem;
}

/******************************************************************************/
/* VERIFY THAT ALL ARRAYS HAVE BEEN FREED                                     */
/******************************************************************************/
void
xcheck()
{
   if (!first_arr && !first_mem) {
      if (debug & DB_MEMORY)
         puts("xcheck: Everything freed.\n");
   } else {
      printf("xcheck: Error, failed to xfree the following:\n");
      xdump();
   }
/*
   if (first_arr)
      printf("xcheck: Error, failed to xfree array %x!\n",first_arr->ptr);
   else if (first_mem)
      printf("xcheck: Error, failed to xfree other %x!\n",first_mem->ptr);
   else if (debug & DB_MEMORY)
      puts("xcheck: Everything freed.\n");
*/
}

/******************************************************************************/
/* FREE A BLOCK OF MEMORY                                                     */
/******************************************************************************/
void              /* RETURNS: (nothing) */
xfree_string(mem) /* ARGUMENTS:         */
   void *mem;     /*    Array to free   */
{
   mem_t *this,*prev=0;

   if (!mem) return;

   /* Find info associated with the block of memory */
   this = find_string(mem,&prev);
   if (!this) {
      (void)wputs("Tried to free unxalloced string\n");
      return; /* not found */
   }

   if (debug & DB_MEMORY) {
      printf("xfree: free %x size %d*%d\n", mem,this->num, this->eltsize); 
      printf("Str:!%s!\n",(char*)mem);
   }

   /* Remove from linked list */
   if (!prev) 
      first_mem  = this->next;
   else       
      prev->next = this->next;

   /* Count for statistics */
   xst[XS_FCALLS][1]++;

   /* Free the space */
   free((char *)mem);
   free((char *)this);
}
 
/******************************************************************************/
/* FREE AN ARRAY                                                              */
/******************************************************************************/
void             /* RETURNS: (nothing) */
xfree_array(arr) /* ARGUMENTS:         */
   void *arr;    /*    Array to free   */
{
   mem_t *this,*prev=0;
   short i;

   if (!arr) return;

   /* Find info associated with the block of memory */
   this = find_array(arr, &prev);
   if (!this) {
      (void)wputs("Tried to free unxalloced array\n");
      return; /* not found */
   }

   if (debug & DB_MEMORY) 
      printf("xfree: free %x size %d*%d\n", arr,this->num, this->eltsize); 

   /* Free the elements of the array */
   for (i=0; i<this->num; i++) {
      if (((char**)arr)[i]) 
         xfree_string(((char**)arr)[i]);
   }

   /* Remove from linked list */
   if (!prev) {
      first_arr  = this->next;
   } else       
      prev->next = this->next;

   /* Count for statistics */
   xst[XS_FCALLS][0]++;

   /* Free up space */
   free((char *)arr);
   free((char *)this);
}

/******************************************************************************/
/* STANDARD strdup() CODE FOR THOSE OS'S WHICH DON'T HAVE IT                  */
/******************************************************************************/
char *      /* RETURNS: New string    */
xstrdup(str) /* ARGUMENTS:             */
char *str;  /*    String to duplicate */
{
   char *ptr;
   ptr = (char*)xalloc(0,strlen(str)+1);
   strcpy(ptr,str);
   return ptr;
}

/******************************************************************************/
/* DUMP ALL ARRAYS CURRENTLY ALLOCATED                                        */
/******************************************************************************/
void    /* RETURNS: (nothing) */
xstat() /* ARGUMENTS: (none)  */
{
   mem_t *this;
   short i;
   long n,s,b,nt=0,st=0,bt=0;

   printf("Category: Number   Size  Bytes Acalls Rcalls Fcalls\n");
   n=s=b=0;
   for (this=first_arr; this; this = this->next) {
      n++;
      s += this->num;
      b += (this->num * this->eltsize);
   }
   printf("Arrays  : %6d %6d %6d",n,s,b);
   for (i=0; i<NUMXST; i++)
      printf(" %6d",xst[i][0]);
   nt+=n; st+=s; bt+=b;

   n=s=b=0;
   for (this=first_mem; this; this = this->next) {
      n++;
      s++;
      b+= this->eltsize;
   }
   printf("\nStrings : %6d %6d %6d",n,s,b);
   for (i=0; i<NUMXST; i++)
      printf(" %6d",xst[i][1]);
   nt+=n; st+=s; bt+=b;

   printf("\nTotals  : %6d %6d %6d",nt,st,bt);
   for (i=0; i<NUMXST; i++)
      printf(" %6d",xst[i][0]+xst[i][1]);
   printf("\n");
}
