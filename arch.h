#pragma once

#include <cstdio>

#include "struct.h"

void get_resp(FILE *fp, response_t *re, int fast, int num);
void get_item(FILE *fp, int n, response_t *re, sumentry_t *sum);

#define GR_ALL 0x0000
#define GR_OFFSET 0x0001
#define GR_HEADER 0x0002
