#
# Ok this is taken from an automaked file and tweaked out
#
include ../../platform-settings

CFLAGS=$(CCFLAGS)

base_OBJECTS = \
	base_connect.o \
	base_dynamic.o \
	base_exec.o \
	base_stdout.o \
	base_accept.o \
	base_file.o \
	base_format.o \
	base_stderr.o \
	base_to.o

single: static

static: $(base_OBJECTS)

all: $(base_OBJECTS)

clean:
	rm -f $(base_OBJECTS)
