#
# Top-level Makefile for clxutils. Allows build or clean
# of all directories in one swoop.  
#
SHELL := /bin/zsh

SUBDIRS= clAppUtils anchorTest caVerify certChain certInCrl certTime certcrl \
	certLabelTest certSerialEncodeTest certsFromDb cgConstruct \
	cgVerify  clTool cmstool \
	dotMacArchive dotMacTool extendAttrTest extendAttrTool \
	extenGrab extenTest extenTestTp extractCertFields findCert \
	idTool kcImport kcExport kcTime kcTool keyFromCert \
	newCmsTool ocspdTool ocspTool pemtool  \
	parseCert parseCrl pemtool secTime secTrustTime \
	signerAndSubj signerAndSubjSsl \
	signerAndSubjTp smimePolicy sslAlert sslAuth sslBench sslCipher \
	sslEcdsa sslHandshakeTime sslProt \
	sslServer sslSession sslSubjName sslViewer \
	sysIdTool threadTest trustAnchors trustApps unBER \
	vfyCert vfyCertChain
# krbtool removed pending new kerb changes
#	 krbtool

# note the p12 tests removed - I really want p12Reencode back in...p12 we can do without
#	extractCertFields idTool kcImport kcExport kcTime kcTool keyFromCert pemtool p12 p12Reencode \

INSTALLDIR=$(DSTROOT)/usr/local/bin

# default SRCROOT for clean target
SRCROOT ?= $(shell pwd)

#
# The "if [[ -a $$i ]]" allows use of this Makefile on a fragment of 
# clxutils, e.g., the SslExamples package built by buildSsl.
#
first:
	@foreach i in $(SUBDIRS); \
		if [[ -a $$i ]] then; \
			echo "=== Making $$i ==="; \
			cd $$i || exit 1; \
			make -r || exit 1; \
			cd ..; \
		fi; \
	end

build: first

sphinx:
	make "SPHINX=-DSPHINX"

# architecture options

64bit:
	make "DEBUG_CFLAGS=-arch x86_64" "CMDLINE_LDFLAGS=-arch x86_64" 
	
64bitFat:
	make "DEBUG_CFLAGS=-arch x86_64 -arch i386" "CMDLINE_LDFLAGS=-arch x86_64 -arch i386" 

fat:
	make "DEBUG_CFLAGS=-arch i386" "CMDLINE_LDFLAGS=-arch i386" 

install:
	@foreach i in $(SUBDIRS); \
		if [[ -a $$i ]] then; \
			echo "=== Installing $$i ==="; \
			cd $$i; \
			make install "DSTROOT=$(DSTROOT)" "SYMROOT=$(SYMROOT)" \
				"OBJROOT=$(OBJROOT)" || exit 1; \
			cd ..; \
		fi; \
	end
	install -d -m 0755 $(INSTALLDIR)
	install -p -m 0755 cltpdvt $(INSTALLDIR)
	echo "** BUILD SUCCEEDED **"

# this module doesn't need to install any headers
installhdrs:

installsrc:
	cp -R . $(SRCROOT)

clean:
	@foreach i in $(SUBDIRS); \
		if [[ -a $$i ]] then; \
			echo "=== Cleaning $$i ==="; \
			cd $$i || exit 1; \
			make clean "DSTROOT=$(DSTROOT)" "SYMROOT=$(SYMROOT)" \
				"OBJROOT=$(OBJROOT)" "SRCROOT=$(SRCROOT)/$$i" \
				|| exit 1; \
			cd ..; \
		fi; \
	end
