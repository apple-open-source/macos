CC := gcc-4.0
override CFLAGS :=

MAINPROG_CFLAGS := -g -mfix-and-continue

MAINPROG_LDFLAGS := -L/System/Library/PrivateFrameworks/ZeroLink.framework/Versions/A/Resources

FIX_CFLAGS := $(MAINPROG_CFLAGS) -mno-dynamic-no-pic

MAKE_A_BUNDLE_FLAGS := -bundle -flat_namespace -undefined suppress

all: stage1 stage2

# Build the main application
stage1:
	gcc-4.0 $(MAINPROG_CFLAGS) -c a.c
	cp b-generic.c b.c
	gcc-4.0 $(MAINPROG_CFLAGS) -DVERSION=1 -o b.o -c b.c
	gcc-4.0 $(MAINPROG_LDFLAGS) -o a.out a.o b.o

# First rebuild of b.o
stage2:
	cp b-generic.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=2 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o2 b.o

# Second rebuild of b.o
stage3:
	cp b-generic.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=3 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o3 b.o

# Third rebuild of b.o
stage4:
	cp b-generic.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=4 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o4 b.o

# Fourth rebuild of b.o
stage5:
	cp b-invalid-1.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=5 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o5 b.o

# Fifth rebuild of b.o
stage6:
	cp b-invalid-2.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=6 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o6 b.o

# Sixth rebuild of b.o
stage7:
	cp b-invalid-3.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=7 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o7 b.o

# Eighth rebuild of b.o
stage9:
	cp b-valid-1.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=9 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o9 b.o

# Ninth rebuild of b.o
stage10:
	cp b-invalid-5.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=10 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o10 b.o

# Tenth rebuild of b.o
stage11:
	cp b-invalid-6.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=11 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o11 b.o

# Eleventh rebuild of b.o
stage12:
	cp b-invalid-7.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=12 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o12 b.o

# rebuild of b.o #12
stage13:
	cp b-invalid-8.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=13 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o13 b.o

# rebuild of b.o #13
stage14:
	cp b-invalid-9.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=14 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o14 b.o

# rebuild of b.o #14
stage15:
	cp b-invalid-10.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=15 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o15 b.o

# rebuild of b.o #15
stage16:
	cp b-invalid-11.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=16 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o16 b.o

# rebuild of b.o #16
stage17:
	cp b-invalid-12.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=17 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o17 b.o

# rebuild of b.o #17
stage18:
	cp b-generic.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=18 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o18 b.o

# rebuild of b.o #19
stage19:
	cp b-valid-2.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=19 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o19 b.o

# rebuild of b.o #20
stage20:
	cp b-valid-3.c b.c
	gcc-4.0 $(FIX_CFLAGS) -DVERSION=20 -o b.o -c b.c
	gcc-4.0 $(MAKE_A_BUNDLE_FLAGS) -o b-bundlized.o20 b.o

clean:
	rm -f a.out a.o b.o b-v1.o b-v2.o b-v3.o b.o.[0-9]* b.c
	rm -f b-bundlized.o.[0-9]* b-bundlized.o b-bundlized.o[0-9]*
	rm -f b-v?.c
