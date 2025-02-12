// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>

#include "struct.h"

/* EMAIL: */
int incorporate(long i, sumentry_t *sum, partentry_t *part, status_t *stt, int idx);
int make_emhead(response_t *re, int par);
int make_emtail(void);

#ifdef NEWS
std::string dot2slash(const std::string_view &str);
int make_rnhead(response_t *re, int par);
const std::string &message_id(const std::string_view &conf, int itm, int rsp, response_t *re);
void get_article(response_t *re);
void refresh_news(sumentry_t *sum, partentry_t *part, status_t *stt, int idx);
#endif
