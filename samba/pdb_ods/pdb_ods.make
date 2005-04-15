srcdir		= $(SRCROOT)/samba/source
CFLAGS		= -O -DDEBUG_PASSWORD -DDEVELOPER $(RC_CFLAGS) -g -Wall -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual -Wcast-align
LDFLAGS		= -L$(OBJROOT) -framework Security -framework DirectoryService -framework CoreFoundation -bundle_loader $(DSTROOT)/usr/sbin/smbd $(RC_CFLAGS)  
LDSHFLAGS	= -bundle -twolevel_namespace -exported_symbols_list pdb_ods.exp -all_load -multiply_defined suppress
FLAGS		=  $(CFLAGS) -I$(srcdir)/include -I$(srcdir)/ubiqx -I$(srcdir)/smbwrapper -I$(srcdir)/popt -I$(srcdir) -I$(OBJROOT)/include -I$(SRCROOT)/dlcompat

OBJS	= pdb_ods.so

# Default target

default: $(OBJS)

# Pattern rules

%.so: %.o
	$(CC) $(LDSHFLAGS) $(LDFLAGS) -o $(OBJROOT)/$@ $<

%.o: %.c
	$(CC) $(FLAGS) -c $<

# Misc targets

clean:
	rm -rf .libs
	rm -f core *~ *% *.bak \
	$(OBJS)
