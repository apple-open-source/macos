#define __KEYCHAINCORE__ 1

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include "SecEMCSPriv.h"
#include "Security_regressions.h"

static void tests(void)
{

    @autoreleasepool {
        NSDictionary *idmsData = SecEMCSCreateNewiDMSKey(NULL, NULL, @"1234", NULL, NULL);
        ok(idmsData);

        NSData *emcsKey = SecEMCSCreateDerivedEMCSKey(idmsData, @"1234", NULL);
        ok(emcsKey, "emcs key");
        if (!emcsKey) @throw @"emacsKey missing";

        /*
         * change password
         */

        NSDictionary *newIdmsData = SecEMCSCreateNewiDMSKey(NULL, emcsKey, @"4321", NULL, NULL);

        NSData *newEmcsKey = SecEMCSCreateDerivedEMCSKey(newIdmsData, @"4321", NULL);
        ok(newEmcsKey, "new emcs key");

        ok([newEmcsKey isEqualToData:emcsKey], "key same");
    }

    @autoreleasepool {

        NSDictionary *fakeIdmsData = @{
                 @"iter" : @1000,
                 @"salt" : [NSData dataWithBytes:"\x7b\x30\x67\x4c\x01\x34\xae\xda\xaf\x4a\x34\xda\x68\x5b\x0b\x75" length:16],
                 @"wkey" : [NSData dataWithBytes:"\xa1\x15\xee\x24\xdf\x39\xd6\x96\xb9\x57\x65\xa0\xec\x7d\x80\x4c\xd1\xb3\xc0\x31\x38\xc0\x3a\x38" length: 24],
        };

        NSData *data = SecEMCSCreateDerivedEMCSKey(fakeIdmsData, @"1234", NULL);
        ok(data, "KDF1");

        ok([data isEqualToData:[NSData dataWithBytes:"\xa4\x42\x8b\xb0\xb8\x20\xdb\xfa\x58\x84\xab\xe3\x52\x93\xeb\x10" length:16]], "same");

        data = SecEMCSCreateDerivedEMCSKey(fakeIdmsData, @"4321", NULL);
        ok(!data, "KFD2");
    }
}

int si_90_emcs(int argc, char *const *argv)
{
    plan_tests(7);

    tests();

    return 0;
}
