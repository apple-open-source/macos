CC := gcc
override CFLAGS :=

MAINPROG_CFLAGS := -arch $(ARCH) -g -mfix-and-continue

MAINPROG_LDFLAGS := -arch $(ARCH)

FIX_CFLAGS := $(MAINPROG_CFLAGS) -arch $(ARCH) -mno-dynamic-no-pic

MAKE_A_BUNDLE_FLAGS := -bundle -flat_namespace -undefined suppress -arch $(ARCH)

all: stage1 stage2

# Build the main application
stage1:
	$(CC) $(MAINPROG_CFLAGS) -c a.c
	cp b-generic.c b.c
	$(CC) $(MAINPROG_CFLAGS) -DVERSION=1 -o b.o -c b.c
	$(CC) $(MAINPROG_LDFLAGS) -o a.out a.o b.o

# First rebuild of b.o
stage2:
	cp b-generic.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=2 -o b2.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o2 b2.o

# Second rebuild of b.o
stage3:
	sleep 10
	cp b-generic.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=3 -o b3.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o3 b3.o

# Third rebuild of b.o
stage4:
	cp b-generic.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=4 -o b4.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o4 b4.o

# Fourth rebuild of b.o
stage5:
	cp b-invalid-1.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=5 -o b5.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o5 b5.o

# Fifth rebuild of b.o
stage6:
	cp b-invalid-2.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=6 -o b6.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o6 b6.o

# Sixth rebuild of b.o
stage7:
	cp b-invalid-3.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=7 -o b7.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o7 b7.o

# Eighth rebuild of b.o
stage9:
	cp b-valid-1.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=9 -o b9.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o9 b9.o

# Ninth rebuild of b.o
stage10:
	cp b-invalid-5.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=10 -o b10.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o10 b10.o

# Tenth rebuild of b.o
stage11:
	cp b-invalid-6.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=11 -o b11.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o11 b11.o

# Eleventh rebuild of b.o
stage12:
	cp b-invalid-7.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=12 -o b12.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o12 b12.o

# rebuild of b.o #12
stage13:
	cp b-invalid-8.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=13 -o b13.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o13 b13.o

# rebuild of b.o #13
stage14:
	cp b-invalid-9.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=14 -o b14.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o14 b14.o

# rebuild of b.o #14
stage15:
	cp b-invalid-10.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=15 -o b15.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o15 b15.o

# rebuild of b.o #15
stage16:
	cp b-invalid-11.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=16 -o b16.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o16 b16.o

# rebuild of b.o #16
stage17:
	cp b-invalid-12.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=17 -o b17.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o17 b17.o

# rebuild of b.o #17
stage18:
	cp b-generic.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=18 -o b18.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o18 b18.o

# rebuild of b.o #19
stage19:
	cp b-valid-2.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=19 -o b19.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o19 b19.o

# rebuild of b.o #20
stage20:
	cp b-valid-3.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=20 -o b20.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o20 b20.o

# rebuild of b.o #21
stage21:
	cp b-valid-4.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=21 -o b21.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o21 b21.o

# rebuild of b.o #22
stage22:
	cp b-valid-5.c b.c
	$(CC) $(FIX_CFLAGS) -DVERSION=22 -o b22.o -c b.c
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o22 b22.o

clean:
	rm -f a.out a.o b.o b-v1.o b-v2.o b-v3.o b.o.[0-9]* b.c
	rm -f b-bundlized.o.[0-9]* b-bundlized.o b-bundlized.o[0-9]*
	rm -f b-v?.c
	rm -f b[0-9].o b[12][0-9].o
