#pragma once

#include <string>
#include <vector>

#include "struct.h"

const char *get_subj(int idx, int item, sumentry_t *sum);
const char *get_auth(int idx, int item, sumentry_t *sum);
const std::vector<std::string> &get_config(int idx);
void clear_cache(void);
void store_subj(int idx, int item, const std::string_view &str);
void store_auth(int idx, int item, const std::string_view &str);
