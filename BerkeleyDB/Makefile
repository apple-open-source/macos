Project         = db
Extra_CFLAGS	= -no-cpp-precomp
Extra_LDFLAGS	= -force_flat_namespace -bind_at_load

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

CC_Optimize = -Os -mdynamic-no-pic

build::
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/dist/configure --disable-java --disable-shared --with-mutex=DARWIN/_spin_lock_try --prefix=/usr/local docdir=/usr/local/BerkeleyDB/docs
	cd $(OBJROOT) && make
	cd $(OBJROOT) && make prefix=$(DSTROOT)/usr/local/BerkeleyDB install
