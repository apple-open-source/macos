#
# Makefile to build snacc-generated code for pkcs* and cms
#
ASN1_SRC= sm_vdatypes.asn sm_x501ud.asn  sm_x411ub.asn sm_x411mtsas.asn \
			sm_x501if.asn  sm_x520sa.asn sm_x509cmn.asn sm_x509af.asn \
			sm_x509ce.asn  pkcs1oids.asn pkcs9oids.asn sm_cms.asn sm_ess.asn \
			pkcs7.asn pkcs8.asn appleoids.asn

SNACC_SRC= $(SRCROOT)/..

GEN_SRC= $(ASN1_SRC:%.asn=%.cpp)
GEN_HDR= $(ASN1_SRC:%.asn=%.h)

ASN_SRC_DIR= $(SRCROOT)/asn
GEN_SRC_DIR= $(SRCROOT)/c++
GEN_HDR_DIR= $(SRCROOT)/inc

VPATH= $(ASN_SRC_DIR)

#
# get snacc headers from snacc project source, not from /usr/local/include
#
HDR_INCPATH= -I$(SNACC_SRC)/c++-lib/inc

#
# standard useful ASN1
#
ASN_USEFUL= asn-useful.asn1

#
# local build directory, 
#
#LOCAL_BUILD= /Data_and_Apps/dmitchBuild
LOCAL_BUILD= $(SYMROOT)

CFLAGS= -g  $(HDR_INCPATH) -DVDADER_RULES
LDFLAGS= -lstdc++ -lsnacc++ -L$(LOCAL_BUILD)

CC= /usr/bin/cc
SNACC= /usr/local/bin/snacc
SNACC_FLAGS= -C -D -u $(ASN_USEFUL)

first:	$(GEN_SRC) move_genfiles

$(GEN_SRC):	$(ASN1_SRC)
	(cd $(ASN_SRC_DIR); $(SNACC) $(SNACC_FLAGS) $(ASN1_SRC))

move_genfiles:	clean_genfiles
	(cd $(ASN_SRC_DIR); \
	 mv *.cpp $(GEN_SRC_DIR); \
	 mv *.h $(GEN_HDR_DIR))

clean_genfiles:
	(cd $(GEN_SRC_DIR); rm -f *.cpp)
	(cd $(GEN_HDR_DIR); rm -f *.h)

clean:	clean_genfiles

install:

# anything else?


