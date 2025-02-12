// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 * mopen() flags: note that on some operating systems (e.g. Solaris),
 * the combination of O_R and O_LOCK is illegal, so don't do it!
 */

#pragma once

#include <fcntl.h>
#include <stdio.h>

#include <string>
#include <string_view>

#define O_PASSTHRU 0x00000FFF       /* actual flags to open() */
#define O_PRIVATE 0x00001000        /* mode 0600 (vs. 0644)         */
#define O_NOCREATE 0x00002000       /* don't create (must exist)    */
#define O_LOCK 0x00004000           /* need to lock it?             */
#define O_PIPE 0x00008000           /* is this a pipe?              */
#define O_SILENT 0x00010000         /* is this a pipe?              */
#define O_NOBLOCK 0x00020000        /* don't block, fail on no lock */
#define O_R (O_RDONLY | O_NOCREATE) /* "r"  */
#define O_W (O_WRONLY | O_CREAT | O_TRUNC | O_LOCK)    /* "w"  */
#define O_A (O_WRONLY | O_CREAT | O_APPEND | O_LOCK)   /* "a"  */
#define O_RPLUS (O_RDWR | O_NOCREATE | O_LOCK)         /* "r+" */
#define O_WPLUS (O_RDWR | O_CREAT | O_TRUNC | O_LOCK)  /* "w+" */
#define O_APLUS (O_RDWR | O_CREAT | O_APPEND | O_LOCK) /* "a+" */

FILE *mopen(const std::string &file, long fl);
int mclose(FILE *fp);
void mcheck(void);
void mdump(void);
void madd(int fd, const std::string_view &file, int flg, int pid);
int get_pid(FILE *fp);
