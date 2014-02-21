#Makefile
CC = gcc
INCLUDE = /usr/lib
LIBS = 
OBJS = 

all: client server

client: 
	$(CC) -o client client.c $(CFLAGS) $(LIBS)
server:
	$(CC) -o server server.c $(CFLAGS) $(LIBS)

clean:
	rm -f client server
