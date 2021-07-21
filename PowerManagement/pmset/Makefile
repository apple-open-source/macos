
OBJ_FILES = pmset.o powermanagementUser.o

CFLAGS = -g -DSTANDALONE -Wall -arch i386
FRAMEWORKS = -framework IOKit -framework CoreFoundation -framework SystemConfiguration

pmset: $(OBJ_FILES)
	cc -o pmset $(CFLAGS) $(OBJ_FILES) $(FRAMEWORKS)

powermanagement.h powermanagementUser.c: powermanagement.defs
	mig powermanagement.defs

powermnagementUser.o: powermanagement.h powermanagementUser