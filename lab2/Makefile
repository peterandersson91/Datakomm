CC = gcc
CFLAGS = -Wall
PROGRAMS = client server

ALL: ${PROGRAMS}

client: client.c
	${CC} ${CFLAGS} -o client.o client.c

server: server.c
	${CC} ${CFLAGS} -o server.o server.c

clean:
	rm -f ${PROGRAMS}
