
OSCOMPILER  = llvm-gcc-4.2
EMCOMPILER  = xcrun -sdk iphoneos llvm-gcc-4.2
TOOLSDIR    = Tools
SOURCESDIR  = Sources

lion:
	make all CC='$(OSCOMPILER)' \
		 CFLAGS='"-arch x86_64"' \
	         LD='$(OSCOMPILER)' \
	         USETARGET='os' \
		     LDFLAGS='"-framework IOKit -framework CoreFoundation -framework SystemConfiguration"'

iosfour:
	make all CC='xcrun -sdk iphoneos llvm-gcc-4.2' \
		CFLAGS='-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS4.3.Internal.sdk/'

iosfive:
	make all CC='xcrun -sdk iphoneos $(EMCOMPILER)' \
		     CFLAGS='-arch armv7 -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.Internal.sdk/' \
		     LD='xcrun -sdk iphoneos $(EMCOMPILER)' \
		     LDFLAGS='-arch armv7 -framework IOKit -framework CoreFoundation -framework SystemConfiguration -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.Internal.sdk/' \
		     USETARGET='embedded'

all: force_rebuild
	make -C $(TOOLSDIR) CC=$(CC) CFLAGS=$(CFLAGS) LD=$(LD) LDFLAGS=$(LDFLAGS) && \
	make  -C $(SOURCESDIR) CC=$(CC) CFLAGS=$(CFLAGS) LD=$(LD) LDFLAGS=$(LDFLAGS) && \
	sudo -p "Please enter password; PM tests must run as root:" $(TOOLSDIR)/pmharness.perl

clean:
	make -C $(TOOLSDIR) clean; \
	make -C $(SOURCESDIR) clean;

force_rebuild:
	