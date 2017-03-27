//
//  rk_01_recoverykey.m
//

#define __KEYCHAINCORE__ 1

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include "SecRecoveryKey.h"
#include "shared_regressions.h"

int rk_01_recoverykey(int argc, char *const *argv)
{
    NSArray *testData = @[
                          @{
                              @"recoverykey" : @"AAAA-AAAA-AAAA-AAAA-AAAA-AAAA-AAGW",
                              @"publicKey" : @"UUjq5Wv572RSsKahddvUPQAEIeErSHMK9J+NKb6sVdo=",
                              @"privateKey" : @"UUjq5Wv572RSsKahddvUPQAEIeErSHMK9J+NKb6sVdpi00pR5UGzfoARLnpxCFmqCh1XCRtjCptztGfN1XW11w==",
                              @"password" : @"Ze14tkzC8keZEnoIv+LoWvicxOTSSqUwhE8xyChmZAs=",
                              },
                          @{ // same again to make sure it works
                              @"recoverykey" : @"AAAA-AAAA-AAAA-AAAA-AAAA-AAAA-AAGW",
                              @"publicKey" : @"UUjq5Wv572RSsKahddvUPQAEIeErSHMK9J+NKb6sVdo=",
                              @"privateKey" : @"UUjq5Wv572RSsKahddvUPQAEIeErSHMK9J+NKb6sVdpi00pR5UGzfoARLnpxCFmqCh1XCRtjCptztGfN1XW11w==",
                              @"password" : @"Ze14tkzC8keZEnoIv+LoWvicxOTSSqUwhE8xyChmZAs=",
                              },
                          @{
                              @"recoverykey" : @"BBBB-BBBB-BBBB-BBBB-BBBB-BBBB-BBAY",
                              @"publicKey" : @"fomczHhXphIMaCbuQlKPefXO8YEIH2M9TFslcBjvJXY=",
                              @"privateKey" : @"fomczHhXphIMaCbuQlKPefXO8YEIH2M9TFslcBjvJXa/W5BWvgJmZO9xShq1sePpLDfGf5lOkwhwzFzFypiXgw==",
                              @"password" : @"P7nC1leKBTJ3aMsXZImVsR2kIlqlsvoSEI8yFKv6xdw=",
                              },
                          ];


    plan_tests(7 * (int)[testData count]);

    [testData enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        NSString *recoveryKey = obj[@"recoverykey"];
        NSString *knownPublicKey = obj[@"publicKey"];
        NSString *knownPrivateKey = obj[@"privateKey"];
        NSString *knownPassword = obj[@"password"];

        SecRecoveryKey *rk = SecRKCreateRecoveryKey(recoveryKey);
        ok(rk, "got recovery key");

        NSData *publicKey = SecRKCopyBackupPublicKey(rk);
        ok(publicKey, "got publicKey");

        ok([publicKey isEqualToData:[[NSData alloc] initWithBase64EncodedString:knownPublicKey options:0]],
           "public key same: %@", [publicKey base64EncodedStringWithOptions:0]);

        NSData *privateKey = SecRKCopyBackupFullKey(rk);
        ok(privateKey, "got privateKey");

        ok([privateKey isEqualToData:[[NSData alloc] initWithBase64EncodedString:knownPrivateKey options:0]],
           "privateKey key same: %@", [privateKey base64EncodedStringWithOptions:0]);

        NSString *recoveryPassword = SecRKCopyAccountRecoveryPassword(rk);
        ok(recoveryPassword, "got account recovery password");

        ok([recoveryPassword isEqualToString:knownPassword], "password same: %@", recoveryPassword);
    }];

    return 0;
}
