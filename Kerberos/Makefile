OS = MACOS
JAM = /Developer/Private/jam
JAMFILE = ./Common/Scripts/KfM.jam
BUILDIT = ./Common/Scripts/buildit.pl
SUBMISSIONDIR = /tmp/KerberosSubmission

# Default values:  (overridden by buildit at Apple)
SRCROOT = $(SUBMISSIONDIR)/Sources
INSTALL_MODE_FLAG = u+w,go-w,a+rX

SUBMISSIONNAME = Kerberos
SUBMISSIONSOURCES = $(SUBMISSIONDIR)/$(SUBMISSIONNAME)
SUBMISSION = $(SUBMISSIONDIR)/KerberosSubmission.tgz

MAKEINSTALLER = $(SUBMISSIONSOURCES)/KerberosInstaller/Scripts/MakeKerberosInstaller.sh

include /Developer/Makefiles/pb_makefiles/platform.make

install:
	$(JAM) "-sJAMFILE=$(JAMFILE)" "-sSRCROOT=$(SRCROOT)" "-sDSTROOT=$(DSTROOT)" "-sSYMROOT=$(SYMROOT)" "-sOBJROOT=$(OBJROOT)" "-sINSTALL_MODE_FLAG=$(INSTALL_MODE_FLAG)" install

installsrc:
	$(JAM) "-sJAMFILE=$(JAMFILE)" "-sSRCROOT=$(SRCROOT)" "-sDSTROOT=$(DSTROOT)" "-sSYMROOT=$(SYMROOT)" "-sOBJROOT=$(OBJROOT)" "-sINSTALL_MODE_FLAG=$(INSTALL_MODE_FLAG)" installsrc

installsrc_norsrc:
	$(JAM) "-sJAMFILE=$(JAMFILE)" "-sSRCROOT=$(SRCROOT)" "-sDSTROOT=$(DSTROOT)" "-sSYMROOT=$(SYMROOT)" "-sOBJROOT=$(OBJROOT)" "-sINSTALL_MODE_FLAG=$(INSTALL_MODE_FLAG)" "-sNORSRC=YES" installsrc  

clean:
	$(JAM) "-sJAMFILE=$(JAMFILE)" "-sSRCROOT=$(SRCROOT)" "-sDSTROOT=$(DSTROOT)" "-sSYMROOT=$(SYMROOT)" "-sOBJROOT=$(OBJROOT)" "-sINSTALL_MODE_FLAG=$(INSTALL_MODE_FLAG)" clean

installhdrs:
    echo "WARNING: installhdrs target disabled to avoid running krb5 build system twice."
#	$(JAM) "-sJAMFILE=$(JAMFILE)" "-sSRCROOT=$(SRCROOT)" "-sDSTROOT=$(DSTROOT)" "-sSYMROOT=$(SYMROOT)" "-sOBJROOT=$(OBJROOT)" "-sINSTALL_MODE_FLAG=$(INSTALL_MODE_FLAG)" installhdrs

# Create the submission directory
makedirs:
	if [ -d "$(SUBMISSIONDIR)" ]; then rm -r "$(SUBMISSIONDIR)"; fi
	mkdir "$(SUBMISSIONDIR)"

# Install the sources into the submission folder with disassembled resource forks and tar up the submission
makesubmission: makedirs installsrc_norsrc
	mv "$(SRCROOT)" "$(SUBMISSIONSOURCES)"
	find "$(SUBMISSIONSOURCES)" -type d -name CVS -print0 | xargs -0 rm -r
	cd "$(SUBMISSIONDIR)" && gnutar -czp -f "$(SUBMISSION)" "$(SUBMISSIONNAME)"
	rm -r "$(SUBMISSIONSOURCES)"

# Unpack and build the submission, just like apple would
buildsubmission: makesubmission
	cd "$(SUBMISSIONDIR)" && gnutar -xzp -f "$(SUBMISSION)"
	cd "$(SUBMISSIONSOURCES)" && perl "$(BUILDIT)" . -release $(USER)

# Create the installer from the built submission
# The source tree we use must have the disassembled resource forks (needed for CFM glue)
makeinstaller: buildsubmission
	sh $(MAKEINSTALLER) "$(SUBMISSIONSOURCES)" "/tmp/$(SUBMISSIONNAME).roots/$(SUBMISSIONNAME).dst" "$(SUBMISSIONDIR)"

kfm: makeinstaller
	
