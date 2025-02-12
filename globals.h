// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once


#include <string>
#include <vector>

#include "yapp.h"
#include "struct.h"

/* GLOBAL VARS */

/* Status info */
extern flag_t flags;
extern unsigned char mode;
extern flag_t status;
extern flag_t debug;
extern stdin_t saved_stdin[STDIN_STACK_SIZE];
extern stdin_t orig_stdin[STDIN_STACK_SIZE];
extern int stdin_stack_top;
/* Conference info */
extern int current; /* current index to cflist */
extern int confidx; /* current index to conflist */
extern int defidx;
extern int joinidx;     /* current index to conflist */
extern std::vector<std::string> cflist;	/* User's cflist */
extern std::string cfliststr;		/* cflist in a string */
extern std::vector<std::string> fw;	/* List of FW's for current conf */
/* System info */
extern std::string bbsdir;
extern std::string helpdir;
extern std::vector<assoc_t> conflist;	// System table of conferences
extern std::vector<assoc_t> desclist;	// System table of conference descriptions
extern std::string hostname;
/* Info on the user */
extern uid_t uid;
extern std::string login;
extern std::string fullname;		/* Full name from passwd file */
extern std::string email;		/* User's email address */
extern std::string home;                /* User's home directory */
extern std::string work;		/* User's work directory */
extern std::string partdir;		/* Location of user's partfiles */
/* Item info */
extern status_t st_glob, st_new;
extern response_t re[MAX_RESPONSES];
extern sumentry_t sum[MAX_ITEMS];
extern partentry_t part[MAX_ITEMS];

extern char evalbuf[MAX_LINE_LENGTH];
