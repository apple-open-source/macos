CFLAGS = -g
TESTS= rldtest satest
LIBSA= ../sym/libsa.a

all: $(TESTS)

sizeof: sizeof.c
	cc -I../libsa sizeof.c -o sizeof

rldtest: rldtest.o
	cc -o rldtest rldtest.o

satest: satest.o $(LIBSA)
	cc -o satest satest.o $(LIBSA) -lsys_s
