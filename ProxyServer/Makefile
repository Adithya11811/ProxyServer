CC=gcc
CFLAGS= -g -Wall 

all: proxy

proxy: proxy_server.o cache.o http.o utils.o proxy_parse.o
	$(CC) $(CFLAGS) -o proxy proxy_server.o cache.o http.o utils.o proxy_parse.o -lpthread

proxy_server.o: proxy_server.c proxy_server.h
	$(CC) $(CFLAGS) -c proxy_server.c

cache.o: cache.c cache.h
	$(CC) $(CFLAGS) -c cache.c

http.o: http.c http.h
	$(CC) $(CFLAGS) -c http.c

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c

proxy_parse.o: proxy_parse.c proxy_parse.h
	$(CC) $(CFLAGS) -c proxy_parse.c

clean:
	rm -f proxy *.o

tar:
	tar -cvzf ass1.tgz proxy_server.c cache.c http.c utils.c README Makefile proxy_parse.c proxy_parse.h proxy_server.h cache.h http.h utils.h
