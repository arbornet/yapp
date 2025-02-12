// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <string_view>

#include "yapp.h"

// Command-line options
#ifdef INCLUDE_EXTRA_COMMANDS
static const char *options = "qulosnij:x:";
#else
static const char *options = "qulosniwj:x:";
#endif

// Command line option flag
typedef struct {
	const std::string_view name;	// name used with define, change
	flag_t mask;			// bitmask
        bool dflt;
} option_t;

constexpr option_t option[] = {/* User-definable flags          */
    /*  name,       mask,        default, */
    { "quiet", O_QUIET, false, },
    { "buffer", O_BUFFER, true, },
    { "default", O_DEFAULT, true, },
    { "observe", O_OBSERVE, false, },
    { "st_rip", O_STRIP, false, },
    { "so_urce", O_SOURCE, true, },
    { "inc_orporate", O_INCORPORATE, false, },
#ifdef INCLUDE_EXTRA_COMMANDS
    { "www", O_CGIBIN, false, },
#endif
    { "f_orget", O_FORGET, true, },
    { "sta_y", O_STAY, false, },
    { "dot", O_DOT, true, },
    { "ed_always", O_EDALWAYS, false, },
    { "me_too", O_METOO, true, },
    { "nu_mbered", O_NUMBERED, false, },
    { "d_ate", O_DATE, false, },
    { "u_id", O_UID, false, },
    { "ma_iltext", O_MAILTEXT, false, },
    { "autosave", O_AUTOSAVE, false, },
    { "verbose", O_VERBOSE, false, },
    { "scr_ibbler", O_SCRIBBLER, false, },
    { "sig_nature", O_SIGNATURE, false, },
    { "readonly", O_READONLY, false },
    { "sen_sitive", O_SENSITIVE, false },
    { "unseen", O_UNSEEN, true },
    { "autojoin", O_AUTOJOIN, false },
    { "label", O_LABEL, true },
};

constexpr option_t debug_opt[] = {/* User-definable flags          */
    /*  name,          mask,        default, */
    { "mem_ory", DB_MEMORY, false, },
    { "conf_erences", DB_CONF, false, },
    { "mac_ro", DB_MACRO, false, },
    { "range", DB_RANGE, false, },
    { "driv_er", DB_DRIVER, false, },
    { "file_s", DB_FILES, false, },
    { "part", DB_PART, false, },
    { "arch", DB_ARCH, false, },
    { "lib", DB_LIB, false, },
    { "sum", DB_SUM, false, },
    { "item", DB_ITEM, false, },
    { "user", DB_USER, false, },
    { "pipe", DB_PIPE, false, },
    { "ioredir", DB_IOREDIR, false, },
};
