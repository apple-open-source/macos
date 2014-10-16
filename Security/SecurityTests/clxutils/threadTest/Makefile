# name of executable to build
EXECUTABLE=threadTest
# C++ source (with .cpp extension)
#CPSOURCE= threadTest.cpp cgConstruct.cpp cgVerify.cpp sslPing.cpp getFields.cpp signVerify.cpp timeThread.cpp symTest.cpp attach.cpp rsaSign.cpp desTest.cpp
CPSOURCE= threadTest.cpp cgConstructThr.cpp cgVerifyThr.cpp sslPing.cpp getFields.cpp signVerify.cpp timeThread.cpp symTestThr.cpp attach.cpp getCachedFields.cpp sslThrash.cpp cspRand.cpp derDecode.cpp secTrustEval.cpp kcStatus.cpp digestClient.cpp mdsLookupThr.cpp cssmErrStr.cpp trustSettings.cpp dbOpenClose.cpp copyRoots.cpp
# C source (.c extension)
CSOURCE= ioSockThr.c testutil.c

#
# Note final load with cc, not ld
#
# project-specific libraries, e.g., -lstdc++
#
# can't link against BSAFE as of 3/15/2001 - it's exported from Security
#PROJ_LIBS= -lc_static
PROJ_LIBS=
#
# Optional lib search paths
#
PROJ_LIBPATH= -L$(LOCAL_BUILD)
#
# choose one for cc
#
VERBOSE=
#VERBOSE=-v

#
# non-standard frameworks (e.g., -framework foo)
#
PROJ_FRAMEWORKS= -framework CoreFoundation

#
# project-specific includes, with leading -I
#
PROJ_INCLUDES= 

#
# Optional C flags (warnings, optimizations, etc.)
#
PROJ_CFLAGS= -fvisibility=hidden

#
# Optional link flags (using cc, not ld)
#
#PROJ_LDFLAGS= -lMallocDebug
PROJ_LDFLAGS=

#
# Optional dependencies
#
CERT_FILE_NAME= mypage.apple_v3.100.cer
TARG_CERT_FILE= $(LOCAL_BUILD)/$(CERT_FILE_NAME)
PROJ_DEPENDS= $(TARG_CERT_FILE)

#
# Other files to remove at 'make clean' time
#
OTHER_TO_CLEAN= 


include ../Makefile.cdsa


$(TARG_CERT_FILE):	$(CERT_FILE_NAME)
	cp -p $(CERT_FILE_NAME) $(LOCAL_BUILD)

