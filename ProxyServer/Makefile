CC = g++
CFLAGS = -g -Wall -Werror
INCLUDES = -I.

# List of object files
OBJECTS = proxy_parse.o proxy.o hashCache.o ip_block.o network.o client_op.o

# Default target
all: proxy

# Build proxy executable
proxy: $(OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDES) -o proxy $(OBJECTS)

# Compile proxy_parse.o
proxy_parse.o: proxy_parse.c proxy_parse.h uthash.h constants.h
	$(CC) $(CFLAGS) $(INCLUDES) -c proxy_parse.c -o proxy_parse.o

# Compile proxy.o
proxy.o: proxy.c client_op.h proxy_parse.h constants.h network.h
	$(CC) $(CFLAGS) $(INCLUDES) -c proxy.c -o proxy.o

# Compile hashCache.o
hashCache.o: hashCache.c hashCache.h uthash.h constants.h
	$(CC) $(CFLAGS) $(INCLUDES) -c hashCache.c -o hashCache.o

# Compile ip_block.o
ip_block.o: ip_block.c ip_block.h constants.h
	$(CC) $(CFLAGS) $(INCLUDES) -c ip_block.c -o ip_block.o

# Compile network.o
network.o: network.c network.h constants.h
	$(CC) $(CFLAGS) $(INCLUDES) -c network.c -o network.o

# Compile client_op.o
client_op.o: client_op.c client_op.h constants.h
	$(CC) $(CFLAGS) $(INCLUDES) -c client_op.c -o client_op.o

# Clean build artifacts
clean:
	rm -f proxy $(OBJECTS)

# Create tarball
tar:
	tar -cvzf proxy_project.tgz *.c *.h Makefile blocked_ips.txt
