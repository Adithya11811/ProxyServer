#include "ip_block.h"
#include "constants.h"

int file_exists(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file) {
    fclose(file);
    return 1; // File exists
  }
  return 0; // File doesn't exist
}
// Function to add GitHub's IPs to the file if not present
// void add_github_ips_to_file(FILE *file) {
//   const char *github_ipv4 = "20.207.73.82";
//   const char *github_ipv6 = "64:ff9b::14cf:4952";
//   fprintf(file, "%s\n", github_ipv4);
//   fprintf(file, "%s\n", github_ipv6);
//   fflush(file);
//   printf("Added GitHub's IPv4 and IPv6 addresses to the file.\n");
// }

// Function to read blocked IPs from file
void read_blocked_ips(char blocked_ips[][50], int *ip_count) {
  FILE *file = fopen(BLOCKED_IP_FILE, "r");
  if (!file) {
    perror("Error opening blocked IPs file");
    return;
  }

  char line[50];
  while (fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\n")] = '\0'; // Remove newline character
    strcpy(blocked_ips[*ip_count], line);
    (*ip_count)++;
  }

  fclose(file);
}

int is_ip_blocked(const char *ip) {
  char blocked_ips[100][50]; // Store up to 100 IPs
  int ip_count = 0;

  // Check if blocked IP file exists, otherwise create it
  if (!file_exists(BLOCKED_IP_FILE)) {
    printf(
        "Blocked IP file not found. Creating %s...\n",
        BLOCKED_IP_FILE);
    FILE *file = fopen(BLOCKED_IP_FILE, "w");
    if (!file) {
      perror("Error creating blocked IP file");
      return EXIT_FAILURE;
    }
    //add_github_ips_to_file(file);
    fclose(file);
  }

  // Now, read the blocked IPs into the array
  read_blocked_ips(blocked_ips, &ip_count);

  for (int i = 0; i < ip_count; i++) {
    if (strcmp(blocked_ips[i], ip) == 0) {
      return 1; // IP is blocked
    }
  }
  return 0; // IP is not blocked
}