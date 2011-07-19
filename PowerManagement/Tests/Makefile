
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

ios:
	make all CC='xcrun -sdk iphoneos llvm-gcc-4.2' \
		CFLAGS='-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS4.3.Internal.sdk/'

all: force_rebuild
	make -C $(TOOLSDIR) CC=$(CC) CFLAGS=$(CFLAGS) LD=$(LD) LDFLAGS=$(LDFLAGS) && \
	make  -C $(SOURCESDIR) CC=$(CC) CFLAGS=$(CFLAGS) LD=$(LD) LDFLAGS=$(LDFLAGS) && \
	sudo -p "Please enter password; PM tests must run as root:" $(TOOLSDIR)/pmharness.perl

clean:
	make -C $(TOOLSDIR) clean; \
	make -C $(SOURCESDIR) clean;

force_rebuild:
	
