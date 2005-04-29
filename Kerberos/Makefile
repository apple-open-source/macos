BUILDIT = ./Common/Scripts/buildit.pl
BUILDROOT = /tmp

KERBEROS_SUBMISSION = $(BUILDROOT)/KerberosSubmission

SUBMISSION_NAME = Kerberos
SUBMISSION_SOURCES = $(KERBEROS_SUBMISSION)/$(SUBMISSION_NAME)
SUBMISSION = $(KERBEROS_SUBMISSION)/$(SUBMISSION_NAME).tgz

LIBRARIES_JAMFILE = ./Common/Scripts/KfMLibraries.jam
LIBRARIES_PROJECT = KerberosLibraries
LIBRARIES_BUILD = $(BUILDROOT)/KerberosLibraries

CLIENTS_JAMFILE = ./Common/Scripts/KfMClients.jam
CLIENTS_PROJECT = KerberosClients
CLIENTS_BUILD = $(BUILDROOT)/KerberosClients

INSTALLER_DST_PATH = $(BUILDROOT)/$(SUBMISSION_NAME).dst
MAKEINSTALLER = $(SUBMISSION_SOURCES)/Common/Scripts/MakeKerberosInstaller.sh

# Default values:  (normally set by buildit at Apple)
INSTALL_MODE_FLAG = u+w,go-w,a+rX
FRAMEWORK_SEARCH_PATHS = $(INSTALLER_DST_PATH)/System/Library/Frameworks
RC_ProjectName = Kerberos
BUILT_PRODUCTS_DIR = $(SYMROOT)/BuiltProducts

JAM = /Developer/Private/jam
JAMFILE = ./Common/Scripts/KfM.jam

PKINIT_FILES = \
	KerberosFramework/Kerberos5/Sources/include/pkinit_apple_utils.h \
	KerberosFramework/Kerberos5/Sources/include/pkinit_asn1.h \
	KerberosFramework/Kerberos5/Sources/include/pkinit_cert_store.h \
	KerberosFramework/Kerberos5/Sources/include/pkinit_client.h \
	KerberosFramework/Kerberos5/Sources/include/pkinit_cms.h \
	KerberosFramework/Kerberos5/Sources/lib/krb5/krb/pkinit_apple_asn1.c \
	KerberosFramework/Kerberos5/Sources/lib/krb5/krb/pkinit_apple_cert_store.c \
	KerberosFramework/Kerberos5/Sources/lib/krb5/krb/pkinit_apple_client.c \
	KerberosFramework/Kerberos5/Sources/lib/krb5/krb/pkinit_apple_cms.c \
	KerberosFramework/Kerberos5/Sources/lib/krb5/krb/pkinit_apple_utils.c \
        KerberosFramework/Kerberos5/Sources/kdc/pkinit_apple_server.c

# So we don't have to manually set and export everything
.EXPORT_ALL_VARIABLES:

#
# Targets to create the submission:
#

sanitycheck:
	echo "Checking for paths containing ${USER}'s home directory in files not starting with ${USER}."
	:; ! find . -type f \! -name ${USER}.\* -print0 | xargs -0 grep ${HOME}
	echo "done."

# Hack to place pkinit sources in tree
pkinit_sources:
	for FILE in $(PKINIT_FILES) ; do \
		if [ ! -f "$$FILE" ] ; then echo "Creating '$$FILE'" ; touch "$$FILE" ; fi ; \
	done

# Create the submission directory
makedirs:
	echo "Removing old directories..."
	if [ -d "$(KERBEROS_SUBMISSION)" ]; then chmod -R u+w "$(KERBEROS_SUBMISSION)" && rm -rf "$(KERBEROS_SUBMISSION)"; fi
	if [ -d "$(LIBRARIES_BUILD)"     ]; then chmod -R u+w "$(LIBRARIES_BUILD)"     && rm -rf "$(LIBRARIES_BUILD)"; fi
	if [ -d "$(CLIENTS_BUILD)"       ]; then chmod -R u+w "$(CLIENTS_BUILD)"       && rm -rf "$(CLIENTS_BUILD)"; fi
	if [ -d "$(INSTALLER_DST_PATH)"  ]; then chmod -R u+w "$(INSTALLER_DST_PATH)"  && rm -rf "$(INSTALLER_DST_PATH)"; fi
	echo "Creating build directories... "
	mkdir -p "$(KERBEROS_SUBMISSION)"
	mkdir -p "$(LIBRARIES_BUILD)"
	mkdir -p "$(CLIENTS_BUILD)"
	mkdir -p "$(INSTALLER_DST_PATH)"

# Install the sources into the submission folder, strip CVS directories and executable bits and tar up the submission
make_submission: makedirs
	echo "Creating submission... "
	make "SRCROOT=$(SUBMISSION_SOURCES)" installsrc
	find "$(SUBMISSION_SOURCES)" -type d -name CVS -print0 | xargs -0 rm -r
	gnutar -czf "$(SUBMISSION)" -C "$(KERBEROS_SUBMISSION)" "$(SUBMISSION_NAME)"
	rm -rf "$(SUBMISSION_SOURCES)"

# Unpack and build the submission, just like apple will (we hope)
build_submission: make_submission
	echo "Unpacking submission... "
	gnutar -xzpf "$(SUBMISSION)" -C "$(KERBEROS_SUBMISSION)"
	echo "Running test build of target '$(LIBRARIES_PROJECT)'... "
	(cd "$(SUBMISSION_SOURCES)" && BUILDIT_DIR=$(LIBRARIES_BUILD) && export BUILDIT_DIR && perl "$(BUILDIT)" . -release $(USER) -project $(LIBRARIES_PROJECT) -merge "$(INSTALLER_DST_PATH)")
	echo "Running test build of target '$(CLIENTS_PROJECT)'... "
	(cd "$(SUBMISSION_SOURCES)" && BUILDIT_DIR=$(CLIENTS_BUILD)   && export BUILDIT_DIR && perl "$(BUILDIT)" . -release $(USER) -project $(CLIENTS_PROJECT)   -merge "$(INSTALLER_DST_PATH)")

# Create the installer from the built submission
makeinstaller: build_submission
	sh $(MAKEINSTALLER) "$(SUBMISSION_SOURCES)" "$(INSTALLER_DST_PATH)" "$(KERBEROS_SUBMISSION)"

kfm: sanitycheck pkinit_sources makeinstaller


#
# Apple build system targets:
#

# install sources from working directory into SRCROOT
installsrc:
	mkdir -p "$(SRCROOT)"
	gnutar -cf - . | gnutar -xpf - -C "$(SRCROOT)"
	find "$(SRCROOT)" -type f -print0 | xargs -0 chmod 644        
	chmod a+x "$(SRCROOT)/KerberosFramework/Kerberos5/Sources/config/mkinstalldirs"
	chmod a+x "$(SRCROOT)/KerberosFramework/KerberosErrors/Scripts/compile_et"

installhdrs:
	echo "WARNING: installhdrs target disabled to avoid running krb5 build system twice."

all:
	$(JAM) "-sJAMFILE=$(JAMFILE)" build

install:
	echo "RC_CFLAGS is set to '$(RC_CFLAGS)'"
	echo "RC_ARCHS is set to '$(RC_ARCHS)'"
	$(JAM) "-sJAMFILE=$(JAMFILE)" install

clean:
	$(JAM) "-sJAMFILE=$(JAMFILE)" clean


