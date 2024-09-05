CC = g++
CFLAGS = -g -Wall -Werror

# List of object files
OBJECTS = proxy_parse.o proxy.o hashCache.o

# Default target
all: proxy

# Build proxy executable
proxy: $(OBJECTS)
	$(CC) $(CFLAGS) -o proxy $(OBJECTS)

# Compile proxy_parse.o
proxy_parse.o: proxy_parse.c proxy_parse.h uthash.h
	$(CC) $(CFLAGS) -o proxy_parse.o -c proxy_parse.c

# Compile proxy.o
proxy.o: proxy.c proxy_parse.h uthash.h
	$(CC) $(CFLAGS) -o proxy.o -c proxy.c

# Compile hashCache.o
hashCache.o: hashCache.c hashCache.h uthash.h
	$(CC) $(CFLAGS) -o hashCache.o -c hashCache.c

# Clean build artifacts
clean:
	rm -f proxy *.o

# Create tarball
tar:
	tar -cvzf ass1.tgz proxy README Makefile proxy_parse.c proxy_parse.h hashCache.c hashCache.h uthash.h
