CFLAGS = $(RC_CFLAGS) -Os -dead_strip

installsrc:
	rm -rf $(SRCROOT)/fstyp.tproj
	mkdir -p $(SRCROOT)/fstyp.tproj
	cp Makefile fstyp.c fstyp_hfs.c fstyp_msdos.c fstyp_ufs.c fstyp_udf.c \
		fstyp_ntfs.c $(SRCROOT)/fstyp.tproj
	cp fstyp.8 fstyp_hfs.8 fstyp_msdos.8 fstyp_ufs.8 fstyp_udf.8 \
		fstyp_ntfs.8 $(SRCROOT)/fstyp.tproj

installhdrs:

install:
	install -d -m 755 $(DSTROOT)/sbin
	install -d -m 755 $(DSTROOT)/usr/share/man/man8
	$(CC) $(CFLAGS) -o $(DSTROOT)/sbin/fstyp $(SRCROOT)/fstyp.tproj/fstyp.c
	strip -x $(DSTROOT)/sbin/fstyp
	$(CC) $(CFLAGS) -o $(DSTROOT)/sbin/fstyp_hfs $(SRCROOT)/fstyp.tproj/fstyp_hfs.c
	strip -x $(DSTROOT)/sbin/fstyp_hfs
	$(CC) $(CFLAGS) -o $(DSTROOT)/sbin/fstyp_msdos $(SRCROOT)/fstyp.tproj/fstyp_msdos.c
	strip -x $(DSTROOT)/sbin/fstyp_msdos
	$(CC) $(CFLAGS) -o $(DSTROOT)/sbin/fstyp_ufs $(SRCROOT)/fstyp.tproj/fstyp_ufs.c
	strip -x $(DSTROOT)/sbin/fstyp_ufs
	$(CC) $(CFLAGS) -o $(DSTROOT)/sbin/fstyp_udf $(SRCROOT)/fstyp.tproj/fstyp_udf.c
	strip -x $(DSTROOT)/sbin/fstyp_udf
	$(CC) $(CFLAGS) -o $(DSTROOT)/sbin/fstyp_ntfs $(SRCROOT)/fstyp.tproj/fstyp_ntfs.c
	strip -x $(DSTROOT)/sbin/fstyp_ntfs
	install -c -m 644 $(SRCROOT)/fstyp.tproj/fstyp.8 $(DSTROOT)/usr/share/man/man8
	install -c -m 644 $(SRCROOT)/fstyp.tproj/fstyp_hfs.8 $(DSTROOT)/usr/share/man/man8
	install -c -m 644 $(SRCROOT)/fstyp.tproj/fstyp_msdos.8 $(DSTROOT)/usr/share/man/man8
	install -c -m 644 $(SRCROOT)/fstyp.tproj/fstyp_ufs.8 $(DSTROOT)/usr/share/man/man8
	install -c -m 644 $(SRCROOT)/fstyp.tproj/fstyp_udf.8 $(DSTROOT)/usr/share/man/man8
	install -c -m 644 $(SRCROOT)/fstyp.tproj/fstyp_ntfs.8 $(DSTROOT)/usr/share/man/man8

clean:
	rm -f fstyp fstyp_hfs fstyp_msdos fstyp_ufs fstyp_udf fstyp_ntfs
