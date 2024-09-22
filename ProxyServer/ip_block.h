#ifndef IP_BLOCK_H
#define IP_BLOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void add_github_ips_to_file(FILE *file);
int is_ip_blocked(const char *ip);
void read_blocked_ips(char blocked_ips[][50], int *ip_count);
int file_exists(const char *filename);
#endif // IP_BLOCK_H
