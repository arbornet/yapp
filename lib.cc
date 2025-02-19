// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "lib.h"

#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <readline/readline.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <iterator>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include "driver.h"
#include "files.h"
#include "globals.h"
#include "macro.h"
#include "main.h"
#include "mem.h"
#include "str.h"
#include "struct.h"
#include "system.h"
#include "yapp.h"

std::string_view
strim(std::string_view s, const std::string_view ws)
{
    return str::trim(s, ws);
}

std::string_view
strimw(std::string_view s)
{
    return str::trim(s, " \t\r\n\f\v");
}

char *
trim(char *str)
{
    if (str == NULL)
        return NULL;
    while (*str == ' ') str++;
    char *end = str + strlen(str);
    while (end != str && *--end == ' ') *end = '\0';
    return str;
}

// Returns true IFF s matches m.
bool
match(const std::string_view &s, const std::string_view &m)
{
    return str::match(s, m);
}

/******************************************************************************/
/* CHECK FOR A PARTIAL STRING MATCH (required_optional)                       */
/******************************************************************************/
bool
match(const char *ent, // String entered by user
    const char *und    // String with underlines to match
)
{
    if (ent == NULL)
        ent = "";
    if (und == NULL)
        und = "";
    const std::string_view entv{ent, strlen(ent)};
    const std::string_view undv{und, strlen(und)};
    return match(entv, undv);
}

/******************************************************************************/
/* APPEND A LINE TO A FILE                                                    */
/******************************************************************************
Arguments:   filename to put stuff in, string to put there
Returns:     true on success, false on error
Calls:
Description: Appends a block of text to a file with a single operation.
             Locks & unlocks file to be safe.
*******************************************************************************/
/* ARGUMENTS:                       */
/* Filename to append to            */
/* Block of text to write           */
bool
write_file(const std::string &filename, const std::string_view &str)
{
    FILE *fp;
    long mod = O_A;
    if (st_glob.c_security & CT_BASIC)
        mod |= O_PRIVATE;
    if ((fp = mopen(filename, mod)) == NULL)
        return 0;
    auto n = fwrite(str.data(), str.size(), 1, fp);
    mclose(fp);
    return n == str.size();
}

//*****************************************************************************
// DUMP A FILE TO THE OUTPUT
//*****************************************************************************
// Function:    char cat(char *dir, char *filename)
// Called by:
// Arguments:   filename to display
// Directory containing file
// Filename to display
// Returns:     1 on success, 0 on failure
// Calls:
// Description: Copies a file to the screen (not grab_file)
//*****************************************************************************
bool
cat(const std::string_view &dir, const std::string_view &filename)
{
    std::string path(dir);
    if (!filename.empty()) {
        path.append("/");
        path.append(filename);
    }
    if (debug & DB_LIB)
        std::println("cat: {}", path);
    FILE *fp = mopen(path, O_R | O_SILENT);
    if (fp == NULL)
        return 0;
    int c;
    while ((c = fgetc(fp)) != EOF && !(status & S_INT)) wputchar(c);
    mclose(fp);
    return true;
}

extern std::string pipebuf;
//*****************************************************************************
// DUMP A FILE TO THE OUTPUT THROUGH A PAGER
//*****************************************************************************
// ARGUMENTS:
// Directory containing file
// Filename to display
bool
more(const std::string_view &dir, const std::string_view &filename)
{
    /* Need to check if pager exists */
    if (pipebuf.empty())
        pipebuf = expand("pager", DM_VAR);
    if (!(flags & O_BUFFER) || pipebuf.empty())
        return cat(dir, filename);
    std::string path(dir);
    if (!filename.empty()) {
        path.append("/");
        path.append(filename);
    }
    if (debug & DB_LIB)
        std::cout << "CAT: " << path << std::endl;
    FILE *fp = mopen(path, O_R | O_SILENT);
    if (fp == NULL)
        return false;
    open_pipe();
    int c;
    while ((c = fgetc(fp)) != EOF)
        if (fputc(c, st_glob.outp) == EOF)
            break;
    spclose(st_glob.outp);
    mclose(fp);
    status &= ~S_INT;
    return true;
}

namespace {
static bool
readaline(FILE *fp, std::string &str)
{
    char sbuf[MAX_LINE_LENGTH], *s;
    if (0 && fp == stdin) {
        s = readline(NULL);
    } else {
        s = fgets(sbuf, sizeof(sbuf), fp);
    }
    if (s == nullptr) {
        str.clear();
        return false;
    }
    str = s;
    if (0 && fp == stdin)
        free(s);
    return true;
}

void
prepare_input_state(FILE *fp)
{
    // If reading from command input, reset stuff
    if (fp == st_glob.inp) {
        if ((status & S_PAGER) != 0)
            spclose(st_glob.outp);
        // Make SIGINT abort readaline()
        ints_on();
    }
}

void
restore_input_state(FILE *fp, bool ok)
{
    // If command input, reset stuff
    // Stop SIGINT from aborting fgets()/readline()
    if (fp == st_glob.inp) {
        ints_off();
        // If reading from tty, just reset the EOF
        // XXX this should only happen for KEYBOARD input, not xfile
        if (!ok && isatty(fileno(st_glob.inp))) {
            clearerr(fp); // mystdin
            if (!(flags & O_QUIET))
                std::println("");
        }
    }
}

}  // anonymous namespace

// GET INPUT INTO ARBITRARILY SIZED BUFFER
// Handle continued lines (those that end with '\').
//
// ARGUMENTS:
// Input stream
// Min stdin level, 0 if not reading from stdin
std::optional<std::string>
xgets(FILE *fp, int lvl)
{
    if (fp == nullptr)
        return {};
    // Loop over input, accummulating \-continued lines
    std::string out;
    for (;;) {
        prepare_input_state(fp);
        /* Get a line, (may be aborted by SIGINT) */
        std::string str;
        auto ok = readaline(fp, str);
        restore_input_state(fp, ok);

        // If SIGINT seen when getting a command, return empty command,
        if ((status & S_INT) != 0 && fp == st_glob.inp)
            return "";
        if (!ok) {
            // for systems where interrupts abort fgets
            // Reading commands from a file
            if (stdin_stack_top > 0 + (orig_stdin[0].type == STD_SKIP)) {
                pop_stdin();
                if (stdin_stack_top >= lvl)
                    return xgets(st_glob.inp, lvl);
            }
            return {};
        }
        out.append(str);
        // If it ends with \\\n, delete both
        if (out.ends_with("\\\n")) {
            out.pop_back();
            out.pop_back();
        }
        // If newline read, trash it and mark as done
        if (out.ends_with('\n'))  {
            out.pop_back();
            break;
        }
    }
    // Strip non-printing characters if needed
    if (fp == st_glob.inp && (flags & O_STRIP) != 0) {
        size_t strip = 0;
        std::erase_if(out,
            [&](auto c) {
                auto keep = isprint(c) || isspace(c);
                if (!keep) {
                    if (strip++ == 0)
                        std::print("Stripping bad input:");
                    if (iscntrl(c))
                        std::print(" ^{:c}", c + 64);
                    else
                        std::print(" 0x{:02x}", c);
                }
                return !keep;
            });
        if (strip)
            std::println("");
    }
    return out;
}

// Similar to xgets, above, but returns a boolean
// indicating whether or not a line was read.
// Does not care whether it's reading from stdin
// or not.
bool
ngets(std::string &str, FILE *fp)
{
    auto xstr = xgets(fp, -1);
    if (!xstr) {
        str.clear();
        return false;
    }
    str = *xstr;
    return true;
}

//*****************************************************************************
// READ A FILE INTO AN ARRAY OF STRINGS (1 ELT PER LINE)
//*****************************************************************************
// Arguments:   Filename to grab
// Returns:     vector of strings with lines of file
// Description: This function will read in an entire file of text into
//              memory, dynamically allocating enough space to hold it.
//*****************************************************************************
// ARGUMENTS:
// Directory containing file
// Filename to read into memory
// Flags (see lib.h)
std::vector<std::string>
grab_file(
    const std::string_view &dir, const std::string_view &filename, int flags)
{
    std::vector<std::string> lines;

    std::string path(dir);
    if (!filename.empty()) {
        path.append("/");
        path.append(filename);
    }

    FILE *fp = mopen(path, (flags & GF_SILENT) ? O_R | O_SILENT : O_R);
    if (fp == nullptr)
        return lines;

    if (flags & GF_WORD) {
        /* count each word as a line */
        char word[MAX_LINE_LENGTH];
        while (fscanf(fp, "%s", word) == 1) {
            /* what type of file is this? */
            if (word[0] == '#' && (flags & GF_IGNCMT)) {
                fgets(word, MAX_LINE_LENGTH, fp);
                continue;
            }
            lines.push_back(word);
        }
    } else {
        std::optional<std::string> str;
        while ((str = xgets(fp, 0))) {
            const auto &line = *str;
            if (line[0] != '#' || (flags & GF_IGNCMT) == 0)
                lines.push_back(line);
        }
    }
    mclose(fp);

    return lines;
}

//*****************************************************************************
// GRAB SOME MORE OF A FILE UNTIL WE FIND SOME STRING
//*****************************************************************************
// ARGUMENTS:
// Input file pointer
// String start to stop after
// Flags (see lib.h)
// Actual length of stop string
std::vector<std::string>
grab_more(FILE *fp, const char *end, size_t *endlen)
{
    std::vector<std::string> lines;
    if (endlen != NULL)
        *endlen = 0;
    std::optional<std::string> large;
    while ((large = xgets(fp, 0))) {
        auto &line = *large;
        if (end != nullptr && line[0] == end[0] && line[1] == end[0])
            line.erase(0, 2);
        if ((end != nullptr && line.starts_with(end)) ||
            line.starts_with(",R")) {
            if (endlen)
                *endlen = line.size();
            break;
        }
        lines.push_back(line);
    }
    return lines;
}

//*****************************************************************************
// GET INPUT UNTIL USER SAYS YES OR NO
//*****************************************************************************
// ARGUMENTS:
// Prompt
// Answer to return on error
bool
get_yes(const std::string_view &prompt, bool dflt)
{
    std::string s;

    for (;;) {
        if ((flags & O_QUIET) == 0)
            wputs(prompt);
        if (!ngets(s, st_glob.inp)) /* st_glob.inp */
            return dflt;

        /* Skip leading whitespace */
        str::trim(s);

        if (match(s, "n_on") || match(s, "nop_e"))
            return 0;
        if (match(s, "y_es") || match(s, "ok"))
            return 1;
        std::println("\"{}\" is invalid.  Try yes or no.", s);
    }
}

/******************************************************************************/
/* READ IN ASSOCIATIVE LIST                                                   */
/* ! and # begin comments and are not part of the list                        */
/* =filename chains to another file                                           */
/******************************************************************************/
/* ARGUMENTS:                     */
/* Directory containing file   */
/* Filename to read from       */
std::vector<assoc_t>
grab_list(
    const std::string_view &dir, const std::string_view &filename, int flags)
{
    std::vector<assoc_t> v;
    std::string line;

    /* Compose filename */
    const char *sep = "";
    if (!dir.empty() && !filename.empty())
        sep = "/";
    const auto path = str::concat({dir, sep, filename});

    /* Open the file to read */
    FILE *fp = mopen(path, O_R | O_SILENT);
    if (fp == NULL) {
        if ((flags & GF_SILENT) == 0)
            error("grabbing list ", path);
        return v;
    }

    if ((flags & GF_NOHEADER) == 0) {
        /*
         * Get the first line (skipping any comments).
         * This is the default.
         */
        while (ngets(line, fp)) {
            if (!line.empty() && line[0] != '#' && line[0] != '!')
                break;
        }

        /* If empty, return null array */
        if (line.empty()) {
            std::println(stderr, "Error: {} is empty.", path);
            mclose(fp);
            return v;
        }

        /* Start the list, and save default in location 0 */
        v.push_back(assoc_t("", line));

        if (debug & DB_LIB)
            std::println("Default: '{}'", line);
        if (line.find(':') != std::string::npos)
            std::println(stderr, "Warning: {} may be missing default.", path);
    }

    /* Read until EOF */
    while (ngets(line, fp)) {
        if (debug & DB_LIB)
            std::println("Line: '{}'", line);

        /* Skip comment and blank lines */
        if (line.empty() || line[0] == '#')
            continue;

        /* Have a line, split into name and location */
        char *l = line.data();
        char *p = strchr(l, ':');
        if (p != NULL) {
            *p++ = '\0';
            if (debug & DB_LIB)
                std::println("Name: '{}' Dir: '{}'", line, p);
            v.push_back(assoc_t(l, p));
        } else if (line[0] == '=' && line[1] != '\0') {
            /* Chain to another file */
            mclose(fp);
            const char *next = line.c_str() + 1;

            std::string path;
            if (*next == '%')
                path = str::join("/", {bbsdir, next + 1 + (next[1] == '/')});
            else if (!dir.empty())
                path = str::join("/",
                    {dir, std::string_view(line).substr(1 + (line[0] == '/'))});
            else
                path = next;

            fp = mopen(path, O_R | O_SILENT);
            if (fp == NULL) {
                error("grabbing list ", path);
                break;
            }

            ngets(line, fp); /* read magic line */
            if (debug & DB_LIB)
                std::println("grab_list: magic {}", line);

        } else {
            std::println(stderr, "Bad line read: {}", line);
        }
    }
    mclose(fp);

    return v;
}

/******************************************************************************/
/* FIND INDEX OF NAME IN AN ASSOCIATIVE LIST                                  */
/******************************************************************************/
/* ARGUMENTS:                                   */
/* String to match                              */
/* List of elements to search                   */
/* Number of elements in the list               */
std::size_t
get_idx(const std::string_view &elt, const std::vector<assoc_t> &list)
{
    if (list.empty())
        return nidx;
    if (!list[0].name.empty() && match(elt, list[0].name))
        return 0; /* in case it's a list without default */
    for (auto i = 1uz; i < list.size(); i++)
        if (match(elt, list[i].name))
            return i;
    return nidx;
}

// ARGUMENTS:
// List of elements to search
// String to match
const assoc_t *
assoc_list_find(const std::vector<assoc_t> &list, const std::string &key)
{
    if (list.empty())
        return nullptr;

    // Special case for a list without a default.
    auto ap = list.begin();
    if (!ap->name.empty() && match(key, ap->name))
        return &*ap;
    while (++ap != list.end()) {
        if (match(key, ap->name))
            return &*ap;
    }

    return nullptr;
}

/******************************************************************************/
/* CONVERT TIMESTAMP INTO A STRING IN VARIOUS FORMATS                         */
/******************************************************************************/
// ARGUMENTS:
// Timestamp
// Output style
std::string
get_date(time_t t, int style)
{
    static const char *fmt[] = {
#ifdef NOEDATE
        /* 0 */ "%a %b %d %H:%M:%S %Y", /* dates must be in 05
                                         * format */
        /* 1 */ "%a, %b %d, %Y (%H:%M)",
#else
        /* 0 */ "%a %b %e %H:%M:%S %Y", /* dates need not have
                                         * leading 0 */
        /* 1 */ "%a, %b %e, %Y (%H:%M)",
#endif
        /* 2 */ "%a",
        /* 3 */ "%b",
        /* 4 */ "%e",
        /* 5 */ "%y",
        /* 6 */ "%Y",
        /* 7 */ "%H",
        /* 8 */ "%M",
        /* 9 */ "%S",
        /* 10 */ "%I",
        /* 11 */ "%p",
        /* 12 */ "%p",
#ifdef NOEDATE
        /* 13 */ "(%H:%M:%S) %B %d, %Y",
#else
        /* 13 */ "(%H:%M:%S) %B %e, %Y",
#endif
        /* 14 */ "%Y%m%d%H%M%S",
        /* 15 */ "%a, %d %b %Y %H:%M:%S",
        /* 16 HEX */ "",
        /* 17 */
        "%m"
        /* 18 DEC */ "",
    };
    struct tm *tms = localtime(&t);
    if (style < 0 || style == 16 || style > 18) /* sty=0; */
        return std::format("{:X}", t);
    if (style == 18)
        return std::format("{}", t);

    char timestr[128];
    strftime(timestr, sizeof(timestr), fmt[style], tms);

    return timestr;
}
/******************************************************************************/
/* GENERATE STRING WITHOUT ANY "'_s IN IT                                     */
/******************************************************************************/
std::string
noquote(const std::string_view &str)
{
    if (str.empty())
        return "";
    auto s = str.begin();
    auto e = str.end();
    if (*s == '"' || *s == '\'') {
        if (str.back() == *s)
            e--;
        s++;
    }
    return std::string(s, e);
}

/******************************************************************************/
/* GENERATE STRING WITHOUT ANY _'s IN IT                                      */
/******************************************************************************/
std::string
compress(const std::string_view &s)
{
    return str::strip(s, "_");
}

void
error(const std::string_view &str1, const std::string_view &str2)
{
    if (errno)
        std::println(stderr, "Got error {} ({}) in {}{}", errno,
            strerror(errno), str1, str2);

    const auto errorlog = str::concat({bbsdir, "/errorlog"});
    FILE *fp = fopen(errorlog.c_str(), "a");
    if (fp == NULL)
        return;
    char timestamp[32];
    time_t now;
    time(&now);
    ctime_r(&now, timestamp);
    if (errno)
        std::println(fp, "{:<8} {} Got error {} ({}) in {}{}", login,
            timestamp + 4, errno, strerror(errno), str1, str2);
    else
        std::println(
            fp, "{:<8} {} WARNING: {}{}", login, timestamp + 4, str1, str2);
    fclose(fp);
}

std::string &
lower_case(std::string &str)
{
    return str::lowercase(str);
}

void
lower_case(std::span<char> &str)
{
    for (auto &c : str) c = tolower(c);
}

char *
lower_case(char *str)
{
    std::span s{str, std::strlen(str)};
    lower_case(s);
    return str;
}

void
mkdir_all(const std::string &fullpath, int mode)
{
    struct stat sb; /* Struct containing directory status */

    /* Make sure directory doesn't exist before creating it */
    if (stat(fullpath.c_str(), &sb) == 0)
        return;

    std::string dir(fullpath);
    const char *path = dir.c_str();
    for (char *p = dir.data(); *p != '\0'; p++) {
        /* Make sure each piece of path  exists */
        if (*p == '/' && p > path) {
            *p = '\0';
            mkdir(path, mode);
            *p = '/';
        }
    }

    /* Create the entire directory before exit */
    if (mkdir(path, mode) != 0) {
        error("Creating directory ", fullpath);
    }
}
