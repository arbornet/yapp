// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include <string_view>

#include "struct.h"

void check_mail(int f);
void get_status(status_t *st, sumentry_t *sum, partentry_t *part, int idx);
int item_sum(int i, sumentry_t *sum, partentry_t *part, int idx, status_t *stt);
void load_sum(sumentry_t *sum, partentry_t *part, status_t *stt, int idx);
void refresh_link(status_t *stt, sumentry_t *sum, partentry_t *part, int idx, int i);
void refresh_stats(sumentry_t *sum, partentry_t *part, status_t *st);
void refresh_sum(int item, int idx, sumentry_t *sum, partentry_t *part, status_t *st);
void save_sum(sumentry_t *sum, int i, int idx, status_t *stt);
uint32_t get_hash(const std::string_view &str);
void dirty_part(int i);
void dirty_sum(int i);
