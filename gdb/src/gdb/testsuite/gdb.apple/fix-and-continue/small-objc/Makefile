CC := gcc
override CFLAGS :=

MAINPROG_CFLAGS := -g -ffix-and-continue

MAINPROG_LDFLAGS := -framework Foundation \
     -L/System/Library/PrivateFrameworks/ZeroLink.framework/Versions/A/Resources

FIX_CFLAGS := $(MAINPROG_CFLAGS) -findirect-data -mno-dynamic-no-pic -freplace-objc-classes

MAKE_A_BUNDLE_FLAGS := -bundle -flat_namespace -undefined suppress

all: stage1 stage2

# It has yet to be decided if all fixing of an object file must have object
# file unique names, or if they can all have the same name.  The settings
# below make it easy to experiment.

B1SUFFIX=1
B2SUFFIX=2
B3SUFFIX=3
B4SUFFIX=4

# Build the main application
stage1:
	gcc -v
	gcc $(MAINPROG_CFLAGS) -c main.m
	cp class-v1.m class.m
	gcc $(MAINPROG_CFLAGS) -c class.m
	gcc $(MAINPROG_LDFLAGS) -o a.out main.o class.o

# First rebuild of class.o
stage2:
	gcc -v
	cp class-v2.m class.m
	gcc $(FIX_CFLAGS) -c class.m
	gcc -c -o reload-stub.o reload-stub.s
	gcc $(MAKE_A_BUNDLE_FLAGS) -o class-bundlized.o$(B2SUFFIX) class.o

# Second rebuild of class.o
stage3:
	gcc -v
	cp class-v3.m class.m
	gcc $(FIX_CFLAGS) -c class.m
	gcc -c -o reload-stub.o reload-stub.s
	gcc $(MAKE_A_BUNDLE_FLAGS) -o class-bundlized.o$(B3SUFFIX) class.o

# Third rebuild of class.o
stage4:
	gcc -v
	cp class-v4.m class.m
	gcc $(FIX_CFLAGS) -c class.m
	gcc -c -o reload-stub.o reload-stub.s
	gcc $(MAKE_A_BUNDLE_FLAGS) -o class-bundlized.o$(B4SUFFIX) class.o

clean:
	rm -f a.out a.o b.o b-v1.o b-v2.o b-v3.o b.o.[0-9]*
	rm -f class.o main.o
	rm -f class-bundlized.o class-bundlized.o.[0-9]* class-bundlized.o[0-9]*
	rm -f b-bundlized.o.[0-9]* b-bundlized.o b-bundlized.o[0-9]*
	rm -f reload-stub.o
