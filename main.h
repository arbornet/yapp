#pragma once

#include <stdio.h>

#include <string_view>

void wputchar(int c);
void wputs(const std::string_view &s);
void wfputs(const std::string_view &s, FILE *fp);
void wfputc(int c, FILE *fp);
