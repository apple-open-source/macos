Project  = telnet
ProductType = staticlib
Install_Dir = /usr/local/lib

CFILES = auth.c enc_des.c encrypt.c forward.c genget.c getent.c \
	kerberos.c kerberos5.c krb4encpwd.c \
	misc.c read_password.c rsaencpwd.c sra.c

Extra_CC_Flags = -Wno-unused
Extra_CC_Flags += -D__FBSDID=__RCSID \
	-I. \
	-DHAS_CGETENT -DAUTHENTICATION -DRSA -DKRB5 -DFORWARD -DHAVE_STDLIB_H \
	# -DENCRYPTION -DDES_ENCRYPTION -DKRB4 

Install_Headers_Directory = /usr/local/include/libtelnet
Install_Headers = auth.h auth-proto.h encrypt.h enc-proto.h misc.h misc-proto.h

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

after_install:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/libtelnet.plist $(OSV)
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/LICENSE $(OSL)/libtelnet.txt
