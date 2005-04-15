srcdir		= $(SRCROOT)/samba/source
CFLAGS		= -O -DDEBUG_PASSWORD -DDEVELOPER $(RC_CFLAGS) -g -Wall -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual -Wcast-align
LDFLAGS		=  -bundle_loader $(DSTROOT)/usr/sbin/smbd $(RC_CFLAGS)
LDSHFLAGS	= -bundle -twolevel_namespace -exported_symbols_list darwin_acls.exp
FLAGS		=  $(CFLAGS) -I$(srcdir)/include -I$(srcdir)/ubiqx -I$(srcdir)/smbwrapper -I$(srcdir)/popt -I$(srcdir) -I$(OBJROOT)/include -I$(SRCROOT)/dlcompat

OBJS	= darwin_acls.so

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
