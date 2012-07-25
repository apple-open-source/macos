//
//  capabilities.h
//  CCRegress
//

#include <Availability.h>

#ifndef __CAPABILITIES_H__
#define __CAPABILITIES_H__

#define entryPoint(testname,supportname) \
int testname(int argc, char *const *argv) { \
char prString[80];\
sprintf(prString, "No %s Support in this release\n", supportname);\
plan_tests(1); \
diag(prString); \
ok(1, prString); \
return 0; \
}


#define _SNOWLEOPARD_ 1060
#define _LION_ 1070
#define _ZIN_ 1080
#define _CAB_ 1090
#define _NMOS_ 1091



#if __MAC_OS_X_VERSION_MAX_ALLOWED < _NMOS_
#define CC_RMD128_DIGEST_LENGTH 16
#define CC_RMD160_DIGEST_LENGTH 20
#define CC_RMD256_DIGEST_LENGTH 32
#define CC_RMD320_DIGEST_LENGTH 40
#endif


#define CRYPTORWITHMODE 1
#define CCDIGEST 1
#define CCRANDOM 1
#define CCKEYDERIVATION 1
#define CCCMAC 1
#define CCRSA 1
#define CCEC 1
#define CCDH 1
#define CCBIGNUM 1
#define CCRESET 1
#define CCSYMREGRESSION 1
#define CCSYMOFFSET 1
#define CCSYMZEROLEN 1
#define CCSYMCBC 1
#define CCSYMOFB 1
#define CCSYMCFB 1
#define CCSYMGCM 1
#define CCSYMXTS 1
#define CCSYMRC2 1
#define CCPADCTS 1
#define CCHMACCLONE 1
#define CCSELFTEST 0
#define CCSYMWRAP 1
#define CNENCODER 0
#define CCBIGDIGEST 0
#define CCSYMCTR 1

#endif /* __CAPABILITIES_H__ */
