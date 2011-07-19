Project         = db
Extra_CFLAGS	= -no-cpp-precomp
Extra_LDFLAGS	= -force_flat_namespace -bind_at_load

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

CC_Optimize = -Os  

build::
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/dist/configure --disable-java --disable-shared --prefix=/usr/local docdir=/usr/local/BerkeleyDB/docs
	cd $(OBJROOT) && make
	cd $(OBJROOT) && make prefix=$(DSTROOT)/usr/local/BerkeleyDB bindir=$(DSTROOT)/usr/bin install
	mkdir -p $(DSTROOT)/usr/share/man/man1/
	cp $(SRCROOT)/AppleExtras/*.1 $(DSTROOT)/usr/share/man/man1/
	mkdir -p $(DSTROOT)/usr/local/OpenSourceLicenses/
	cp $(SRCROOT)/db/LICENSE $(DSTROOT)/usr/local/OpenSourceLicenses/BerkeleyDB.txt
	mkdir -p $(DSTROOT)/usr/local/OpenSourceVersions/
	cp $(SRCROOT)/AppleExtras/BerkeleyDB.plist $(DSTROOT)/usr/local/OpenSourceVersions/
	chown -R root:wheel $(DSTROOT)/usr/local/BerkeleyDB/

install_headers::
	mkdir -p $(OBJROOT)
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/dist/configure --disable-java --disable-shared --prefix=/usr/local docdir=/usr/local/BerkeleyDB/docs
	mkdir -p $(DSTROOT)/usr/local/BerkeleyDB/include
	$(INSTALL) -c -m 444 $(OBJROOT)/db.h $(DSTROOT)/usr/local/BerkeleyDB/include
	$(INSTALL) -c -m 444 $(OBJROOT)/db_cxx.h $(DSTROOT)/usr/local/BerkeleyDB/include
	chown -R root:wheel $(DSTROOT)/usr/local/BerkeleyDB/
