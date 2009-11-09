CFLAGS = -g -Os -Wall -Wextra -Wshadow -Wmissing-prototypes -Wmissing-declarations -Werror

all: client server

hello_logger.h hello_loggerServer.c hello_loggerUser.c hello_loggerServer.h: hello_logger.defs
	mig -sheader hello_loggerServer.h hello_logger.defs

client.o: hello_logger.h

server.o: hello_loggerServer.h

client: hello_loggerUser.o client.o

server: hello_loggerServer.o server.o

clean:
	-rm -f *.o client server hello_logger.h hello_loggerServer.c hello_loggerUser.c hello_loggerServer.h
