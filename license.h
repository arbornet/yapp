#pragma once

#include <string>
#include <string_view>

void read_config(void);
std::string get_conf_param(const std::string_view &name, const std::string_view &def);
void free_config(void);
int get_hits_today(void);
