#ifndef CONSTANTS_H
#define CONSTANTS_H

#define REDIRECT_IP "/https://guthib.com/"

#define GOOGLE_REDIRECT                                                        \
  "HTTP/1.1 302 Found\r\nLocation: https://guthib.com/\r\n\r\n"

#define BLOCKED_IP_FILE "blocked_ips.txt"

#define MAX_SIZE 1048576 // 1MB max size

#define MAX_ELEMENT_SIZE 102400 // 100KB max element size
const int INITIAL_BUF_SIZE = 5000;
const int MAX_IP_COUNT = 100;

#endif // CONSTANTS_H
