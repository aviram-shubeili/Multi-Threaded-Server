#
# To compile, type "make" or make "all"
# To remove files, type "make clean"
#
OBJS = server.o request.o segel.o client.o
TARGET = server


CC = gcc
CFLAGS = -g -Wall

LIBS = -lpthread 

.SUFFIXES: .c .o 

all: server client output.cgi output1.cgi output2.cgi output3.cgi output4.cgi output5.cgi output6.cgi
	-mkdir -p public
	-cp output.cgi output1.cgi output2.cgi output3.cgi output4.cgi output5.cgi output6.cgi favicon.ico home.html public

server: server.o request.o segel.o
	$(CC) $(CFLAGS) -o server server.o request.o segel.o $(LIBS)

client: client.o segel.o
	$(CC) $(CFLAGS) -o client client.o segel.o

output.cgi: output.c
	$(CC) $(CFLAGS) -o output.cgi output.c

output1.cgi: output1.c
	$(CC) $(CFLAGS) -o output1.cgi output1.c

output2.cgi: output2.c
	$(CC) $(CFLAGS) -o output2.cgi output2.c

output3.cgi: output3.c
	$(CC) $(CFLAGS) -o output3.cgi output3.c

output4.cgi: output4.c
	$(CC) $(CFLAGS) -o output4.cgi output4.c

output5.cgi: output5.c
	$(CC) $(CFLAGS) -o output5.cgi output5.c

output6.cgi: output6.c
	$(CC) $(CFLAGS) -o output6.cgi output6.c

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) server client output.cgi
	-rm -rf public
