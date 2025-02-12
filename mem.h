// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <stddef.h>

void *emalloc(const size_t size);
void *emalloz(const size_t size);
void *erealloc(void *ptr, size_t size);
char *estrdup(const char *str);
char *estrndup(const char *str, size_t len);
