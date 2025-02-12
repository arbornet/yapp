// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <utility>
#include <string>
#include <string_view>
#include <vector>

#include <stdio.h>
#include <sys/types.h>

#include "trie.h"
#include "yapp.h"

/* Flag type */
typedef unsigned long flag_t; /* expand type when we need > 16 option flags */
typedef unsigned short mask_t;

/* Macros */
typedef struct {
	std::string name;
	std::string value;
	mask_t mask;
} macro_t;

typedef struct {
	char *name;
	int token_type;
} keyword_t;

typedef struct {
	std::string name;
	std::string location;
} assoc_t;

typedef struct {
	uint32_t flags;  /* item flags (see IF_xxx in yapp.h)           */
	uint32_t nr;     /* number of responses (not inc. initial text) */
	time_t last;     /* item file modification time */
	time_t first;    /* item file creation time */
	/* char  *subj;  Subj must be separate so sum.c can block dump sum file
	 */
} sumentry_t;

typedef struct {
	short nr;
	long last;
} partentry_t;

typedef struct {
	const char *name;
	int (*func)(int, char **);
} dispatch_t;

/* Global status structure */
typedef struct {
	unsigned int c_security; /* cf security type */
#ifdef NEWS
	long c_article; /* highest article # seen */
#endif
	short c_status,                 /* cf status flags */
	    c_confitems,                /* # sum file entries */
	    i_first,                    /* first item in conference */
	    i_last,                     /* last item in conference */
	    i_current,                  /* current item */
	    i_next,                     /* next item */
	    i_prev,                     /* prev item */
	    i_newresp,                  /* # of old items with new responses */
	    i_brandnew,                 /* # of brand new items */
	    i_unseen,                   /* # of unseen items */
	    i_numitems,                 /* total # of active items */
	    r_totalnewresp,             /* total # of new responses in cf */
	    r_first,                    /* first resp to process */
	    r_last,                     /* last resp to process */
	    r_current,                  /* current response */
	    r_max,                      /* highest response # of current item */
	    r_lastseen,                 /* highest response # seen */
	    l_current;                  /* current line # of response */
	std::string fullname;		/* fullname in current cf */
	/* Range specifiers */
	std::string string;		/* "string" range           */
	std::string author;		/* author (login) specified */
	short rng_flags;		/* item status flags for range */
	time_t since,			/* since <date> range */
	    before;			/* before <date> range */

	off_t mailsize;			/* last size of mailbox */

	time_t sumtime,  		/* lastmod of sum file */
	    parttime;    		/* lastmod of participation file */
	FILE *outp;      		/* output stream (pager) pointer */
	FILE *inp;       		/* input stream pointer */
	short opt_flags, 		/* option flags */
	    count;       		/* count of something */
} status_t;

/* Response */
typedef struct {
	std::string fullname;		/* Author's full name */
	std::string login;		/* Author's login */
	uid_t uid;           		/* Author's UID */
	short flags;
	std::vector<std::string> text;	/* The actual text lines */
	short numchars;               	/* How many characters in the response? */
	time_t date;                  	/* Timestamp of entry */
	long offset,                  	/* Offset to start of ,R line */
	    textoff,                  	/* Offset to start of actual text */
	    endoff;                   	/* Offset to start of next response */
	short parent;                 	/* This is a response to what # (+1)? */
#ifdef NEWS
	std::string mid;		/* Message ID string (for Usenet) */
	long article;			/* Article number    (for Usenet) */
#endif
} response_t;

typedef struct {
	int type;
	union {
		int i;
		char *s;
	} val;
} entity_t;

typedef struct {
	int type;
	int fd;
	FILE *fp;
} stdin_t;
