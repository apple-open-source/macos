include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

VersionFlags := -compatibility_version 1 -current_version 1
CC_Shlib      = $(CC) $(CC_Archs) -dynamiclib $(VersionFlags) -all_load

build:: $(ShadowTestFile)
	$(MAKE) -C $(BuildDirectory)/poll PREFIX=$(DSTROOT)/usr CFLAGS="$(CC_Archs)" install
	cd $(BuildDirectory)/dlcompat && ./configure --prefix=/usr
	cd $(BuildDirectory)/dlcompat && $(MAKE) OPT="$(CC_Archs)"
	cd $(BuildDirectory)/dlcompat && $(MAKE) DESTDIR=$(DSTROOT) install
	$(MKDIR) -p $(DSTROOT)/usr/local/lib/system
	$(CP) $(BuildDirectory)/dlcompat/libdl.a $(DSTROOT)/usr/local/lib/system/libdl.a
	$(CP) $(BuildDirectory)/dlcompat/libdl.a $(DSTROOT)/usr/local/lib/system/libdl_debug.a
	$(CP) $(BuildDirectory)/dlcompat/libdl.a $(DSTROOT)/usr/local/lib/system/libdl_profile.a
	$(CP) $(BuildDirectory)/poll/libpoll.a $(DSTROOT)/usr/local/lib/system/libpoll.a
	$(CP) $(BuildDirectory)/poll/libpoll.a $(DSTROOT)/usr/local/lib/system/libpoll_debug.a
	$(CP) $(BuildDirectory)/poll/libpoll.a $(DSTROOT)/usr/local/lib/system/libpoll_profile.a
	$(RM) $(DSTROOT)$(USRLIBDIR)/lib*
	$(MKDIR) $(DSTROOT)/usr/share
	$(MV) $(DSTROOT)/usr/man $(DSTROOT)/usr/share
	$(INSTALL) -d $(DSTROOT)/usr/include/sys
	$(INSTALL) -m 444 $(BuildDirectory)/sys_poll.h $(DSTROOT)/usr/include/sys/poll.h
