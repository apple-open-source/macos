CFLAGS = -g -I..

all: boot_subset testServer listServer

boot_subset: boot_subset.o
	cc -o boot_subset boot_subset.o

testServer: testServer.o bootstrapUser.o
	cc -o testServer testServer.o bootstrapUser.o

listServer: listServer.o bootstrapUser.o
	cc -o listServer listServer.o bootstrapUser.o


bootstrapUser.o: ../bootstrapUser.c
	cc -c -g ../bootstrapUser.c

clean:
	rm -f *.o testServer listServer
