/*
  proxy_parse.c -- a HTTP Request Parsing Library.
  COS 461
*/

#include "proxy_parse.h"

#define DEFAULT_NHDRS 8
#define MAX_REQ_LEN 65535
#define MIN_REQ_LEN 4

// static const char *root_abs_path = "/";

/* private function declartions */
int ParsedRequest_printRequestLine(struct ParsedRequest *pr,
                                   char *buf, size_t buflen,
                                   size_t *tmp);
size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr);

/*
 * debug() prints out debugging info if DEBUG is set to 1
 *
 * parameter format: same as printf
 *
 */
void debug(const char *format, ...)
{
     va_list args;
     if (DEBUG)
     {
          va_start(args, format);
          vfprintf(stderr, format, args);
          va_end(args);
     }
}

/*
 *  ParsedHeader Public Methods
 */

/* Set a header with key and value */
int ParsedHeader_set(struct ParsedRequest *pr,
                     const char *key, const char *value)
{
     struct ParsedHeader *ph;
     ParsedHeader_remove(pr, key);

     if (pr->headerslen <= pr->headersused + 1)
     {
          pr->headerslen = pr->headerslen * 2;
          pr->headers =
              (struct ParsedHeader *)realloc(pr->headers,
                                             pr->headerslen * sizeof(struct ParsedHeader));
          if (!pr->headers)
               return -1;
     }

     ph = pr->headers + pr->headersused;
     pr->headersused += 1;

     ph->key = (char *)malloc(strlen(key) + 1);
     memcpy(ph->key, key, strlen(key));
     ph->key[strlen(key)] = '\0';

     ph->value = (char *)malloc(strlen(value) + 1);
     memcpy(ph->value, value, strlen(value));
     ph->value[strlen(value)] = '\0';

     ph->keylen = strlen(key) + 1;
     ph->valuelen = strlen(value) + 1;
     return 0;
}

/* get the parsedHeader with the specified key or NULL */
struct ParsedHeader *ParsedHeader_get(struct ParsedRequest *pr,
                                      const char *key)
{
     size_t i = 0;
     struct ParsedHeader *tmp;
     while (pr->headersused > i)
     {
          tmp = pr->headers + i;
          if (tmp->key && key && strcmp(tmp->key, key) == 0)
          {
               return tmp;
          }
          i++;
     }
     return NULL;
}

/* remove the specified key from parsedHeader */
int ParsedHeader_remove(struct ParsedRequest *pr, const char *key)
{
     struct ParsedHeader *tmp;
     tmp = ParsedHeader_get(pr, key);
     if (tmp == NULL)
          return -1;

     free(tmp->key);
     free(tmp->value);
     tmp->key = NULL;
     return 0;
}

/* modify the header with given key, giving it a new value
 * return 1 on success and 0 if no such header found
 *
int ParsedHeader_modify(struct ParsedRequest *pr, const char * key,
               const char *newValue)
{
     struct ParsedHeader *tmp;
     tmp = ParsedHeader_get(pr, key);
     if(tmp != NULL)
     {
       if(tmp->valuelen < strlen(newValue)+1)
       {
            tmp->valuelen = strlen(newValue)+1;
            tmp->value = (char *) realloc(tmp->value,
                              tmp->valuelen * sizeof(char));
       }
       strcpy(tmp->value, newValue);
       return 1;
     }
     return 0;
}
*/

/*
  ParsedHeader Private Methods
*/

void ParsedHeader_create(struct ParsedRequest *pr)
{
     pr->headers =
         (struct ParsedHeader *)malloc(sizeof(struct ParsedHeader) * DEFAULT_NHDRS);
     pr->headerslen = DEFAULT_NHDRS;
     pr->headersused = 0;
}

size_t ParsedHeader_lineLen(struct ParsedHeader *ph)
{
     if (ph->key != NULL)
     {
          return strlen(ph->key) + strlen(ph->value) + 4;
     }
     return 0;
}

size_t ParsedHeader_headersLen(struct ParsedRequest *pr)
{
     if (!pr || !pr->buf)
          return 0;

     size_t i = 0;
     int len = 0;
     while (pr->headersused > i)
     {
          len += ParsedHeader_lineLen(pr->headers + i);
          i++;
     }
     len += 2;
     return len;
}

int ParsedHeader_printHeaders(struct ParsedRequest *pr, char *buf,
                              size_t len)
{
     char *current = buf;
     struct ParsedHeader *ph;
     size_t i = 0;

     if (len < ParsedHeader_headersLen(pr))
     {
          debug("buffer for printing headers too small\n");
          return -1;
     }

     while (pr->headersused > i)
     {
          ph = pr->headers + i;
          if (ph->key)
          {
               memcpy(current, ph->key, strlen(ph->key));
               memcpy(current + strlen(ph->key), ": ", 2);
               memcpy(current + strlen(ph->key) + 2, ph->value,
                      strlen(ph->value));
               memcpy(current + strlen(ph->key) + 2 + strlen(ph->value),
                      "\r\n", 2);
               current += strlen(ph->key) + strlen(ph->value) + 4;
          }
          i++;
     }
     memcpy(current, "\r\n", 2);
     return 0;
}

void ParsedHeader_destroyOne(struct ParsedHeader *ph)
{
     if (ph->key != NULL)
     {
          free(ph->key);
          ph->key = NULL;
          free(ph->value);
          ph->value = NULL;
          ph->keylen = 0;
          ph->valuelen = 0;
     }
}

void ParsedHeader_destroy(struct ParsedRequest *pr)
{
     size_t i = 0;
     while (pr->headersused > i)
     {
          ParsedHeader_destroyOne(pr->headers + i);
          i++;
     }
     pr->headersused = 0;

     free(pr->headers);
     pr->headerslen = 0;
}

int ParsedHeader_parse(struct ParsedRequest *pr, char *line)
{
     char *key;
     char *value;
     char *index1;
     char *index2;

     index1 = strchr(line, ':'); // Use strchr instead of index for portability
     if (index1 == NULL)
     {
          debug("No colon found\n");
          return -1;
     }

     // Allocate memory for the key
     key = (char *)malloc((index1 - line + 1) * sizeof(char));
     if (key == NULL)
     {
          debug("Memory allocation failed for key\n");
          return -1;
     }

     memcpy(key, line, index1 - line); // Copy key up to the colon
     key[index1 - line] = '\0';        // Null-terminate the key

     // Move past the colon and possible spaces
     index1++;
     while (*index1 == ' ')
          index1++; // Skip spaces

     // Find the end of the value (i.e., the end of the line)
     index2 = strstr(index1, "\r\n");
     if (index2 == NULL) // In case the header doesn't end correctly
     {
          debug("Invalid header format\n");
          free(key);
          return -1;
     }

     // Allocate memory for the value
     value = (char *)malloc((index2 - index1 + 1) * sizeof(char));
     if (value == NULL)
     {
          debug("Memory allocation failed for value\n");
          free(key);
          return -1;
     }

     memcpy(value, index1, (index2 - index1)); // Copy value up to the end of the line
     value[index2 - index1] = '\0';            // Null-terminate the value

     ParsedHeader_set(pr, key, value); // Store the key-value pair in ParsedRequest

     free(key);   // Free memory for key
     free(value); // Free memory for value
     return 0;
}

/*
  ParsedRequest Public Methods
*/
void ParsedRequest_destroy(struct ParsedRequest *pr)
{
     if (pr->buf != NULL)
     {
          free(pr->buf);
          pr->buf = NULL; // Set to NULL to avoid dangling pointers
     }
     if (pr->path != NULL)
     {
          free(pr->path);
          pr->path = NULL;
     }

     // If you have headers, free them as well
     if (pr->headerslen > 0)
     {
          ParsedHeader_destroy(pr);
     }

     free(pr);
}

struct ParsedRequest *ParsedRequest_create()
{
     struct ParsedRequest *pr;
     pr = (struct ParsedRequest *)malloc(sizeof(struct ParsedRequest));
     if (pr != NULL)
     {
          ParsedHeader_create(pr);
          pr->buf = NULL;
          pr->method = NULL;
          pr->protocol = NULL;
          pr->host = NULL;
          pr->path = NULL;
          pr->version = NULL;
          pr->buf = NULL;
          pr->buflen = 0;
     }
     return pr;
}

/*
   Recreate the entire buffer from a parsed request object.
   buf must be allocated
*/
int ParsedRequest_unparse(struct ParsedRequest *pr, char *buf,
                          size_t buflen)
{
     if (!pr || !pr->buf)
          return -1;

     size_t tmp;
     if (ParsedRequest_printRequestLine(pr, buf, buflen, &tmp) < 0)
          return -1;
     if (ParsedHeader_printHeaders(pr, buf + tmp, buflen - tmp) < 0)
          return -1;
     return 0;
}

/*
   Recreate the headers from a parsed request object.
   buf must be allocated
*/
int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf,
                                  size_t buflen)
{
     if (!pr || !pr->buf)
          return -1;

     if (ParsedHeader_printHeaders(pr, buf, buflen) < 0)
          return -1;
     return 0;
}

/* Size of the headers if unparsed into a string */
size_t ParsedRequest_totalLen(struct ParsedRequest *pr)
{
     if (!pr || !pr->buf)
          return 0;
     return ParsedRequest_requestLineLen(pr) + ParsedHeader_headersLen(pr);
}

/*
   Parse request buffer

   Parameters:
   parse: ptr to a newly created ParsedRequest object
   buf: ptr to the buffer containing the request (need not be NUL terminated)
   and the trailing \r\n\r\n
   buflen: length of the buffer including the trailing \r\n\r\n

   Return values:
   -1: failure
   0: success

*/
int ParsedRequest_parse(struct ParsedRequest *parse, const char *buf, int buflen)
{
     char *full_addr;
     char *saveptr;
     char *index;

     if (parse->buf != NULL)
     {
          debug("parse object already assigned to a request\n");
          return -1;
     }

     if (buflen < MIN_REQ_LEN || buflen > MAX_REQ_LEN)
     {
          debug("invalid buflen %d", buflen);
          return -1;
     }

     /* Create NUL terminated tmp buffer */
     char *tmp_buf = (char *)malloc(buflen + 1); /* including NUL */
     memcpy(tmp_buf, buf, buflen);
     tmp_buf[buflen] = '\0';

     index = strstr(tmp_buf, "\r\n\r\n");
     if (index == NULL)
     {
          debug("invalid request line, no end of header\n");
          free(tmp_buf);
          return -1;
     }

     /* Copy request line into parse->buf */
     index = strstr(tmp_buf, "\r\n");
     if (parse->buf == NULL)
     {
          parse->buf = (char *)malloc((index - tmp_buf) + 1);
          parse->buflen = (index - tmp_buf) + 1;
     }
     memcpy(parse->buf, tmp_buf, index - tmp_buf);
     parse->buf[index - tmp_buf] = '\0';

     /* Parse request line */
     parse->method = strtok_r(parse->buf, " ", &saveptr);
     if (parse->method == NULL)
     {
          debug("invalid request line, no whitespace\n");
          free(tmp_buf);
          free(parse->buf);
          parse->buf = NULL;
          return -1;
     }

     /* Handle CONNECT and GET methods */
     if (strcmp(parse->method, "GET") == 0)
     {
          // Handle GET method request
          full_addr = strtok_r(NULL, " ", &saveptr);
          if (full_addr == NULL)
          {
               debug("invalid request line, no full address\n");
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }

          parse->version = strtok_r(NULL, "\r\n", &saveptr);
          if (parse->version == NULL)
          {
               debug("invalid request line, missing version\n");
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }
          if (strncmp(parse->version, "HTTP/", 5) != 0)
          {
               debug("invalid request line, unsupported version %s\n", parse->version);
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }

          parse->protocol = strtok_r(full_addr, "://", &saveptr);
          parse->host = strtok_r(NULL, "/", &saveptr);
          parse->path = strtok_r(NULL, " ", &saveptr);

          if (parse->host == NULL)
          {
               debug("invalid request line, missing host\n");
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }

          if (parse->path == NULL)
          {
               // If no path is specified, default to "/"
               parse->path = strdup("/");
          }
     }
     else if (strcmp(parse->method, "CONNECT") == 0)
     {
          // Handle CONNECT method request
          full_addr = strtok_r(NULL, " ", &saveptr);
          if (full_addr == NULL)
          {
               debug("invalid request line, no full address\n");
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }
          parse->version = strtok_r(NULL, "\r\n", &saveptr);
          if (parse->version == NULL)
          {
               debug("invalid request line, missing version\n");
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }

          if (strncmp(parse->version, "HTTP/", 5) != 0)
          {
               debug("invalid request line, unsupported version %s\n", parse->version);
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }

          /* For CONNECT, full_addr contains the host and port */
          parse->host = strtok_r(full_addr, ":", &saveptr);
          if (parse->host == NULL)
          {
               debug("invalid request line, missing host\n");
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }

          parse->port = strtok_r(NULL, ":", &saveptr);
          if (parse->port == NULL)
          {
               debug("invalid request line, missing port\n");
               free(tmp_buf);
               free(parse->buf);
               parse->buf = NULL;
               return -1;
          }
          return 0; // CONNECT requests don't have headers, so we return early
     }

     else
     {
          debug("invalid request line, method not supported: %s\n", parse->method);
          free(tmp_buf);
          free(parse->buf);
          parse->buf = NULL;
          return -1;
     }

     /* Parse headers for non-CONNECT methods */
     printf("PARSING HEADERS\n");
     int ret = 0;
     char *currentHeader = strstr(tmp_buf, "\r\n") + 2;
     while (currentHeader[0] != '\0' &&
            !(currentHeader[0] == '\r' && currentHeader[1] == '\n'))
     {
          if (ParsedHeader_parse(parse, currentHeader))
          {
               ret = -1;
               break;
          }

          currentHeader = strstr(currentHeader, "\r\n");
          if (currentHeader == NULL || strlen(currentHeader) < 2)
               break;

          currentHeader += 2;
     }
     free(tmp_buf);
     return ret;
}

/*
   ParsedRequest Private Methods
*/
size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr)
{
     if (!pr || !pr->buf)
          return 0;

     size_t len =
         strlen(pr->method) + 1 + strlen(pr->protocol) + 3 +
         strlen(pr->host) + 1 + strlen(pr->version) + 2;
     if (pr->port != NULL)
     {
          len += strlen(pr->port) + 1;
     }
     /* path is at least a slash */
     len += strlen(pr->path);
     return len;
}

int ParsedRequest_printRequestLine(struct ParsedRequest *pr,
                                   char *buf, size_t buflen,
                                   size_t *tmp)
{
     char *current = buf;

     if (buflen < ParsedRequest_requestLineLen(pr))
     {
          debug("not enough memory for first line\n");
          return -1;
     }
     memcpy(current, pr->method, strlen(pr->method));
     current += strlen(pr->method);
     current[0] = ' ';
     current += 1;

     memcpy(current, pr->protocol, strlen(pr->protocol));
     current += strlen(pr->protocol);
     memcpy(current, "://", 3);
     current += 3;
     memcpy(current, pr->host, strlen(pr->host));
     current += strlen(pr->host);
     if (pr->port != NULL)
     {
          current[0] = ':';
          current += 1;
          memcpy(current, pr->port, strlen(pr->port));
          current += strlen(pr->port);
     }
     /* path is at least a slash */
     memcpy(current, pr->path, strlen(pr->path));
     current += strlen(pr->path);

     current[0] = ' ';
     current += 1;

     memcpy(current, pr->version, strlen(pr->version));
     current += strlen(pr->version);
     memcpy(current, "\r\n", 2);
     current += 2;
     *tmp = current - buf;
     return 0;
}
