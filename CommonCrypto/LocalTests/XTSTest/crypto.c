#include <stdio.h>
#include <CommonCrypto/CommonCryptor.h>
#include </usr/local/include/CommonCrypto/CommonCryptorSPI.h>

static int keyLength = 16;
static int dataLength = 512;


int main (int argc, const char * argv[]) {
    runAllVectors();
    return 0;
}
