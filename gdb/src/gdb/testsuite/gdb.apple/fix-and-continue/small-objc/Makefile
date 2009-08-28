CC := gcc
override CFLAGS :=

MAINPROG_CFLAGS := -arch $(ARCH) -g -mfix-and-continue

MAINPROG_LDFLAGS := -arch $(ARCH) -framework Foundation \
     -L/System/Library/PrivateFrameworks/ZeroLink.framework/Versions/A/Resources

FIX_CFLAGS := -arch $(ARCH) $(MAINPROG_CFLAGS) -mno-dynamic-no-pic -freplace-objc-classes

MAKE_A_BUNDLE_FLAGS := -arch $(ARCH) -bundle -flat_namespace -undefined suppress

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
	$(CC) -v
	$(CC) $(MAINPROG_CFLAGS) -c main.m
	cp class-v1.m class.m
	$(CC) $(MAINPROG_CFLAGS) -c class.m
	$(CC) $(MAINPROG_LDFLAGS) -o a.out main.o class.o

# First rebuild of class.o
stage2:
	$(CC) -v
	cp class-v2.m class.m
	$(CC) $(FIX_CFLAGS) -o class-v2.o -c class.m
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o class-bundlized.o$(B2SUFFIX) class-v2.o

# Second rebuild of class.o
stage3:
	$(CC) -v
	cp class-v3.m class.m
	$(CC) $(FIX_CFLAGS) -o class-v3.o -c class.m
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o class-bundlized.o$(B3SUFFIX) class-v3.o

# Third rebuild of class.o
stage4:
	$(CC) -v
	cp class-v4.m class.m
	$(CC) $(FIX_CFLAGS) -o class-v4.o -c class.m
	$(CC) $(MAKE_A_BUNDLE_FLAGS) -o class-bundlized.o$(B4SUFFIX) class-v4.o

clean:
	rm -f a.out a.o b.o b-v1.o b-v2.o b-v3.o b.o.[0-9]*
	rm -f class-v2.o class-v3.o class-v4.o
	rm -f class.o main.o
	rm -f class-bundlized.o class-bundlized.o.[0-9]* class-bundlized.o[0-9]*
	rm -f b-bundlized.o.[0-9]* b-bundlized.o b-bundlized.o[0-9]*
	rm -f reload-stub.o
