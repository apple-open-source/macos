
#include "testbyteBuffer.h"
#include "testmore.h"
#include "capabilities.h"

#if (CCRSA == 0)
entryPoint(CommonRSA,"RSA Cryptography")
#else

#include <CommonCrypto/CommonRSACryptor.h>
static int kTestTestCount = 18;

static bool
RSAVerifyTest(byteBuffer modulus, byteBuffer exponent, byteBuffer message, byteBuffer signature, CCDigestAlgorithm digestSelector, int verbose, CCAsymetricPadding padding)
{
    CCRSACryptorRef CAVPubKey;
	CCCryptorStatus retval;
    byteBuffer digestValue = mallocByteBuffer(1024);
    byteBuffer decodedSignature = mallocByteBuffer(1024);

    retval = CCRSACryptorCreateFromData(ccRSAKeyPublic, modulus->bytes, modulus->len, exponent->bytes, exponent->len,
                                        NULL, 0, NULL, 0, &CAVPubKey);

    ok(retval == 0, "Build a CAVS test public key using CCRSACryptorCreateFromData");

    CCDigest(digestSelector, message->bytes, message->len, digestValue->bytes);
    digestValue->len = CCDigestGetOutputSize(digestSelector);

    if(verbose) {
        retval = CCRSACryptorCrypt(CAVPubKey, signature->bytes, 128, decodedSignature->bytes, &decodedSignature->len);
        printf("retval = %d\n", retval);
        printf("Decoded Signature %s\n", bytesToHexString(decodedSignature));
        printf("Digest of message %s\n\n\n", bytesToHexString(digestValue));
    }
    // ccOAEPPadding  ccPKCS1Padding ccX931Padding
    retval = CCRSACryptorVerify(CAVPubKey, padding, digestValue->bytes, digestValue->len, digestSelector, 0, signature->bytes, signature->len);
    printf("CCRSACryptorVerify returned %d\n", retval);
    free(digestValue);
    free(decodedSignature);
    return retval == kCCSuccess;
}


static void 
RSAX931BuildTest(uint32_t e, 
                 char *xp1str, char *xp2str, char *xpstr, 
                 char *xq1str, char *xq2str, char *xqstr,
                 char *pstr, char *qstr, char *mstr, char *dstr,
                 CCRSACryptorRef *retpublicKey, CCRSACryptorRef *retprivateKey)
{
    byteBuffer xp1, xp2, xp, xq1, xq2, xq, p, q, m, d;
    int verbose = 1;
    CCRSACryptorRef publicKey, privateKey;

    xp1 = hexStringToBytes(xp1str);
    xp2 = hexStringToBytes(xp2str);
    xp = hexStringToBytes(xpstr);
    xq1 = hexStringToBytes(xq1str);
    xq2 = hexStringToBytes(xq2str);
    xq = hexStringToBytes(xqstr);
    p = hexStringToBytes(pstr);
    q = hexStringToBytes(qstr);
    m = hexStringToBytes(mstr);
    d = hexStringToBytes(dstr);
    
    uint8_t modulus[1024], exponent[1024], pval[1024], qval[1024];
    size_t modulusLength, exponentLength, pLength, qLength;
    
    modulusLength = exponentLength = pLength = qLength = 1024;

    CCRSACryptorCreatePairFromData(e, 
                                   xp1->bytes, xp1->len, xp2->bytes, xp2->len, xp->bytes, xp->len, 
                                   xq1->bytes, xq1->len, xq2->bytes, xq2->len, xq->bytes, xq->len,
                                   &publicKey, &privateKey,
                                   pval, &pLength, qval, &qLength, modulus, &modulusLength, exponent, &exponentLength);
    /*
    retval = CCRSAGetKeyComponents(privateKey, modulus, &modulusLength, exponent, &exponentLength,
                                   pval, &pLength, qval, &qLength);
    ok(retval == 0, "got private key components");
     */
    
    byteBuffer retP = bytesToBytes(pval, pLength);
    byteBuffer retQ = bytesToBytes(qval, qLength);
    byteBuffer retD = bytesToBytes(exponent, exponentLength);
    byteBuffer retM = bytesToBytes(modulus, modulusLength);
    
    if(bytesAreEqual(retP, q) && bytesAreEqual(retQ, p)) {
        byteBuffer tmp = p;
        p = q;
        q = tmp;
        printf("Swapped P and Q\n");
    }
    
    ok(bytesAreEqual(retP, p), "p is built correctly");
    ok(bytesAreEqual(retQ, q), "q is built correctly");
    ok(bytesAreEqual(retD, d), "n is built correctly");
    ok(bytesAreEqual(retM, m), "d is built correctly");
    
    if(verbose) {
        if(!bytesAreEqual(retP, p)) printf("P\nreturned: %s\nexpected: %s\n\n", bytesToHexString(retP), bytesToHexString(p));
        else printf("P is correct\n");
        if(!bytesAreEqual(retQ, q)) printf("Q\nreturned: %s\nexpected: %s\n\n", bytesToHexString(retQ), bytesToHexString(q));
        else printf("Q is correct\n");
        if(!bytesAreEqual(retD, d)) printf("D\nreturned: %s\nexpected: %s\n\n", bytesToHexString(retD), bytesToHexString(d));
        else printf("D is correct\n");
        if(!bytesAreEqual(retM, m)) printf("M\nreturned: %s\nexpected: %s\n\n", bytesToHexString(retM), bytesToHexString(m));
        else printf("M is correct\n");
    }
    *retpublicKey = publicKey;
    *retprivateKey = privateKey;
}

int CommonRSA (int argc, char *const *argv) {
	CCCryptorStatus retval;
    size_t keysize;
    CCRSACryptorRef publicKey, privateKey;
    byteBuffer keydata, dekeydata, hash;
    char encryptedKey[8192];
    size_t encryptedKeyLen = 8192;
    char decryptedKey[8192];
    size_t decryptedKeyLen = 8192;
    char signature[8192];
    size_t signatureLen = 8192;
    // char importexport[8192];
    // size_t importexportLen = 8192;
    char inputpadded[128], outputpadded[128];
    int accum = 0;
    int debug = 0;
    // int verbose = 1;

    
	plan_tests(kTestTestCount);
    
    keysize = 1024;
    
    if(debug) printf("Keygen\n");
    retval = CCRSACryptorGeneratePair(keysize, 65537, &publicKey, &privateKey);
    
    ok(retval == 0, "Generate an RSA Key Pair");
    accum += retval;
    
    if(debug) printf("Encrypt/Decrypt\n");
    keydata = hexStringToBytes("000102030405060708090a0b0c0d0e0f");
        
    retval = CCRSACryptorEncrypt(publicKey, ccPKCS1Padding, keydata->bytes, keydata->len, encryptedKey, &encryptedKeyLen,
                        "murf", 4, kCCDigestSHA1);
    
    ok(retval == 0, "Wrap Key Data with RSA Encryption - ccPKCS1Padding");
    accum += retval;
    
    retval = CCRSACryptorDecrypt(privateKey, ccPKCS1Padding, encryptedKey, encryptedKeyLen,
                        decryptedKey, &decryptedKeyLen,"murf", 4, kCCDigestSHA1);
    
    ok(retval == 0, "Unwrap Key Data with RSA Encryption - ccPKCS1Padding");
    accum += retval;

	dekeydata = bytesToBytes(decryptedKey, decryptedKeyLen);
    
	ok(bytesAreEqual(dekeydata, keydata), "Round Trip ccPKCS1Padding");
    accum += !bytesAreEqual(dekeydata, keydata);

    if(debug) printf("Encrypt/Decrypt 2\n");
    
    encryptedKeyLen = 8192;
    decryptedKeyLen = 8192;
    
    retval = CCRSACryptorEncrypt(publicKey, ccOAEPPadding, keydata->bytes, keydata->len, encryptedKey, &encryptedKeyLen,
                                 "murf", 4, kCCDigestSHA1);

    ok(retval == 0, "Wrap Key Data with RSA Encryption - ccOAEPPadding");
    accum += retval;
    
    
    retval = CCRSACryptorDecrypt(privateKey, ccOAEPPadding, encryptedKey, encryptedKeyLen,
                                 decryptedKey, &decryptedKeyLen,"murf", 4, kCCDigestSHA1);
    
    ok(retval == 0, "Unwrap Key Data with RSA Encryption - ccOAEPPadding");
    accum += retval;
    
	dekeydata = bytesToBytes(decryptedKey, decryptedKeyLen);
    
	ok(bytesAreEqual(dekeydata, keydata), "Round Trip ccOAEPPadding");
        
    if(debug) printf("Sign/Verify\n");
    hash = hexStringToBytes("000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f");

    retval = CCRSACryptorSign(privateKey, ccPKCS1Padding, 
                     hash->bytes, CCDigestGetOutputSize(kCCDigestSHA256),
                     kCCDigestSHA256, 16,
                     signature, &signatureLen);

    ok(retval == 0, "RSA Signing");
    accum += retval;
    
    retval = CCRSACryptorVerify(publicKey, ccPKCS1Padding,
                       hash->bytes, CCDigestGetOutputSize(kCCDigestSHA256), 
                       kCCDigestSHA256, 16,
                       signature, signatureLen);
    ok(retval == 0, "RSA Verifying");
    accum += retval;
    signatureLen = 8192;
    retval = CCRSACryptorSign(privateKey, ccOAEPPadding, 
                              hash->bytes, CCDigestGetOutputSize(kCCDigestSHA1),
                              kCCDigestSHA1, 16,
                              signature, &signatureLen);
    
    ok(retval == 0, "RSA Signing OAEP");
    accum += retval;
    
    retval = CCRSACryptorVerify(publicKey, ccOAEPPadding,
                                hash->bytes, CCDigestGetOutputSize(kCCDigestSHA1), 
                                kCCDigestSHA1, 16,
                                signature, signatureLen);
    ok(retval == 0, "RSA Verifying OAEP");
    accum += retval;

#ifdef NEVER
    memset(signature, 5, 8192);

    retval = CCRSACryptorSign(privateKey, ccX931Padding, 
                              hash->bytes, CCDigestGetOutputSize(kCCDigestSHA1),
                              kCCDigestSHA1, 16,
                              signature, &signatureLen);
    
    //diag("Signature retval = %d\n", retval);
    
    ok(retval == 0, "RSA Signing");
    accum += retval;
    
    retval = CCRSACryptorVerify(publicKey, ccX931Padding,
                                hash->bytes, CCDigestGetOutputSize(kCCDigestSHA1), 
                                kCCDigestSHA1, 16,
                                signature, signatureLen);
    ok(retval == 0, "RSA Verifying");
    accum += retval;

	retval = CCRSACryptorExport(publicKey, importexport, &importexportLen);
    
    ok(retval == 0, "RSA Export Public Key");
    accum += retval;

    retval = CCRSACryptorImport(importexport, importexportLen, &publicKey2);
    
    ok(retval == 0, "RSA Import Public Key");
    accum += retval;
                      
	importexportLen = 8192;
	retval = CCRSACryptorExport(privateKey, importexport, &importexportLen);
    
    ok(retval == 0, "RSA Export Private Key");
    accum += retval;
    
    retval = CCRSACryptorImport(importexport, importexportLen, &privateKey2);
    
    ok(retval == 0, "RSA Import Private Key");
    accum += retval;
#endif

    if(debug) printf("Starting to build key from data\n");
    uint32_t e = 3;
    char *xp1, *xp2, *xp, *xq1, *xq2, *xq, *p, *q, *m, *d;
    xp1 = "1eaa9ade4a0da46dd40824d814";
    xp2 = "17379044dc2c6105423da807f8";
    xp = "fd3f368d01a95944bc1578f8ae58a9b6c17f529da1599a8bcd361df6efede4176924944e30cbe5c2ddea5648019d2086b95c68588380b8725003b047db88f92a";
    xq1 = "1da08feb13d9fba526190d3756";
    xq2 = "10d93d84466d213a3e776c61f6";
    xq = "f67b5f051126a8956171561b62f572090cde4b09b13f73ee28a90bea2bfb4001fe7b16bd51266524684520e77941dddc56b892ae4bd09dd44acc08bf45dd0a58";
    
    
    p = "fd3f368d01a95944bc1578f8ae58a9b6c17f529da1599a8bcd361df6efede4176924944e30d114d4c767d573d1149e005267e6fe36c51d86968cf6f65afcb973";
    q = "f67b5f051126a8956171561b62f572090cde4b09b13f73ee28a90bea2bfb4001fe7b16bd5129f06dc6e1f8b4f739c7eb1eb8dcacca3b41cd484fc0c693367037";
    m = "f3d4c9ca2dca5d4b893919ae7bee0d174d1e7bd2190287f79a7db6f21366108e8b0aa37cc972989ff3730d629620076555884da0e895d4e426449c60e36fad1d0208dd4ade1c45fc90da5e76c9c89fd95d13ce76a97530ee83ea3cfbe96cf28f85c4756797cd0123683194b7b2fcd185c3ea984cb0ef90580f95d57a44b027b5";
    d = "28a376f707a1ba3741898447bf525783e22fbf4daed5c153ef14f3d3033bad6d172c7094cc3dc41aa8932ce5c3b0013b8e4162457c18f8d0b10b6f657b3d478482626149773760b0688ded3b1ebf16044273b2cd3924b068c2572dd9cceb4d13afb0cc64ae4da9facefbf66d271d11ef0dcc4e1af2a7dd80b2c984f4e3bf7fad";

    
    RSAX931BuildTest(e, xp1, xp2, xp, xq1, xq2, xq, p, q, m, d, &publicKey,  &privateKey);

    e = 0x010001;
    xp1 = "155e67ddb99eefb13e4b77a7f0";
    xp2 = "17044df236c14e8ec333e92506";
    xp = "d4f2b30f4f062ad2d05fc742e91bc20ca3ee8a2d126aff592c7de19edb3b884550ddd6f99b0a6b2b785617b46c0995bc112176dbae9a5b7f0bec678e84d6f44c";
    p = "d4f2b30f4f062ad2d05fc742e91bc20ca3ee8a2d126aff592c7de19edb3b884550ddd6f99b13e5dd56ffb2ac1867030f385597e712f65ac8dd1de502857c1a41";
    xq1 = "1e2923b103c935e3788ebd10e4";
    xq2 = "11a2ccec655a8b362b5ec5fcc4";
    xq = "f7c6a68cff2467f300b82591e5123b1d1256546d999a37f4b18fe4896464df6987e7cc80efee3ce4e2f5c7a3cc085bbe33e4d375ed59cbc591f2b3302bd823bc";
    q = "f7c6a68cff2467f300b82591e5123b1d1256546d999a37f4b18fe4896464df6987e7cc80efeeb4c59165f7d1aec9be2b34889dbe221147e7ceefb5c9bd5cb945";
    m = "ce1b6904ec27f4a8f420414860704f4797a202ed16a9a35f63a16511a31675ccb046b02b192ef121b328385922f5faa032113332d42f84c70d4323133e216b0f339ebaf672f6214d0d7c13bea301174485ec44f44fae0e8a7f8d3c81ced5df77723331816158c3added7dc55f1436a7e5f14730be22cf3bebab1b62915c80c85";
    d = "18d16522721b5793169e61ae08eacd291641ac6f8718933313c8a5e66b487393dbb00f5b89334556e4ff5555aa678b2fca07972e2a2db4a3d15d81b639f7852ffe71657918d0280ff1be2f8f5d90b3e68195ab35e5069a3053540958bc6d58489fecf8baab0981f4af7b4db43550bcf01114e5ecdcb18f228db1c617b5d09781";
    RSAX931BuildTest(e, xp1, xp2, xp, xq1, xq2, xq, p, q, m, d, &publicKey,  &privateKey);

#ifdef NEVER
    e = 3;
    xp1 = "1c36bd0874761109bb0575ee16";
    xp2 = "1777c33935db08546dd66b6d96";
    xp = "d040fa5fe5e32eab84bac6cab4c512dae938cbbe4a29f972b78b149b0b5f6a639e29c0830fa13ca140ac83dda18a1ea7b25122d3c39a10effe7afad4a8b4e77ba42c7912399fcd4f1592a3059188bff536788fe6807e0df8e3d1e7350cf5dd69";
    p = "d040fa5fe5e32eab84bac6cab4c512dae938cbbe4a29f972b78b149b0b5f6a639e29c0830fa13ca140ac83dda18a1ea7b25122d3c39a10effe7afad4a8b4e77ba42c791239acf889977037a0efe181d54b93279b7e46a2fdcf674039fb11e89b";
    xq1 = "14c70e475b12870bc6efd3b944";
    xq2 = "1432548a4959eed65b858cd316";
    xq = "e4d222daf062a01a3a9ddfc82a229613403b772ff05fa9fab1fc77de51744af98b65d47bdb2e8f5091af66002550b1d3ca446738450f8f670045f8465a952a8942079c1e048228c86291bb0ae7665146782021262c49143b5ea37ce400240372";
    q = "e4d222daf062a01a3a9ddfc82a229613403b772ff05fa9fab1fc77de51744af98b65d47bdb2e8f5091af66002550b1d3ca446738450f8f670045f8465a952a8942079c1e04937b7eb94b8d322faefd691b6fa2b0ef4a2333ed791afe8ac3ac41";
    m = "ba24d0a5878c01f6ad9140b6271b42309887a6815d5ef1bc3415a381b7b511a42b8d2b8d9df59faa0b69456ff908e24b4ccb835420404ce449c9ce4ca65dc4ae4eb6bb8403b809d530ef4b37e5b211c13a03e2a69afb8c748b90c97d52023ae9a24c1f1f4b3b87685eaa649f54e41b6439e29700543f0747f09658ed392f96ee568a50ad7b5441c88ad37c581526ff296b1c6cc87e352d4f921960b6b630f8f546f1077a7586b839ee07717de84e0a19cd52eceb358ff2c69387b13a83e5335b";
    d = "1f0622c641420053c7983573b12f35b2c4169bc03a3a7d9f5e039b404948d846074231ecefa8eff1ac918b92a9817b0c8ccc95e35ab562260c4c4d0cc664f61d0d1e7496009eac4e32d28c8950f302f589ab507119d49768c1ed76ea3855b47bfcded5a6137e49706fe2f50213aa1313ad67b8adaef390a46bd7ccbdfa0f5042dcd4749d181613a3c9694314626207c7a7c125ca139742296de412449dd1267d6574d30c5e8bb60844e1f21c76ca41cf3bb805c521553218ce71390055029a6b";
    RSAX931BuildTest(e, xp1, xp2, xp, xq1, xq2, xq, p, q, m, d, &publicKey,  &privateKey);

    e = 3;
    xp1 = "1408766e2cb2d47ebfee7ea614";
    xp2 = "16292b77507cffd2f798b7c9f2";
    xp = "f74435451a7ddaa163c8c8ad03dfde97fe066360dfee52e3a9d8f41310fdb484e92e302de0b88c6c698a0b4af99ae001758441bbeb74be9d8047d104a9edb60e9e127c5d0cfd5d170ab84b314f71cbeea22006a2916a1dbc66c5be0357def520fd38445d0815f5ac3099afeb6f2d48666d22da9e3c961949459ce399829719c1";
    p = "f74435451a7ddaa163c8c8ad03dfde97fe066360dfee52e3a9d8f41310fdb484e92e302de0b88c6c698a0b4af99ae001758441bbeb74be9d8047d104a9edb60e9e127c5d0cfd5d170ab84b314f71cbeea22006a2916a1dbc66c5be0357def520fd38445d081fbe68dd24e14f0711cc0351fec8641d8ea7d22c4709f233e6349b";
    xq1 = "161d77eb77c6f257d8f8a3b0ca";
    xq2 = "152f11dfc70b78f0fc6c9137b8";
    xq = "c4d3feeb0e561be3727fd83dedaeaaecba01c798e917dd8bb11a03ce07fcf08f6f006ac6137d021912dffffc1aee981c395366fef05718e38aef69f0abf64f8b2cb9750826b8ec854dab1e1280c403169e3497ee9af08bd6d2b53a0d9c49e034220506f7719041f0cced1cc846b853a090ac42af0f699c2c3174606e02800952";
    q = "c4d3feeb0e561be3727fd83dedaeaaecba01c798e917dd8bb11a03ce07fcf08f6f006ac6137d021912dffffc1aee981c395366fef05718e38aef69f0abf64f8b2cb9750826b8ec854dab1e1280c403169e3497ee9af08bd6d2b53a0d9c49e034220506f771942204f0890fb5e617c580aa98a7482b5457215badc119f23b21c3";
    m = "be1cfc39868d8e9a8239f504482be60c01071cbdab4355b03c10edafe85d9ca10689d86036b6d35829a364a8a2b69f28743e50e5e27ac6b6fe8962809e1c2e0765b2d7508d61bfa538085dfb685595c6965bc5e0855a6dd8807a83e2ee7fc50b5b48f2d232195b672f2c325eb6649dee9758ce76f690107f3b0d10afef427777fb0bac0a41e23717fc54d9194a344d1823bdc18fa364e5373da39a3e41bcc4d88a688a711b56c6387b669d37c4fd7878559b93473869ae8190c46605f03cf25038bf771246fb81a27bc9d44ba67bfce94a3051856511661dbe0803d220809695ad707022c4acd24d40e011eb3752e39568f66cdd2d90369a67295e19dadb0d11";
    d = "1faf7f5eebc2426f15b45380b6b1fbacaad684ca4735e39d5f58279d5164ef702bc1a410091e788eb19b3b717073c53168b50d7ba5bf211e7fc1906ac504b25690f323e2c23af546340164ff3c0e43a1190f4ba56b8f124ec0146b507d154b81e48c28785daee49132875dba73bb6fa7c3e42269291802bfdf2cd81d528b13e90a7de94f042d0ac33102095d0ec64b433c9e43c3a4651e215072c5ba3175aff6085efd3f868589487fd4c2fd72be000f1bcb51c20f6fa3d56b97872d6f0ed21e67a896478336340105e6672bf90bb250ac4f487e0973ca17161781f58763f58ac25ddb77b7297da53dddb02661b18dad920fd4dd7b7233f125336dd79e1ef3c9";
    RSAX931BuildTest(e, xp1, xp2, xp, xq1, xq2, xq, p, q, m, d, &publicKey,  &privateKey);

    e = 3;
    xp1 = "164511563871556a9babc022c8";
    xp2 = "1ae2a7a04f23efe080f48a24b0";
    xp = "db5c4ccf412b17041b6e20b7e0cb45d807ef4da8282428e05e26782fef3251ea2f613d00a134842c6070aa6ebd2c38bb2a28c0f457601b159ae1f5af94dc8c9812f9b4e031ed1f08c64fdb6ffca71c0d3fc93c63596100b2dbce1d6cbf34fae84bccb859397f700114b4bba2e56678360f79c9df784e5f21e995f84fb8622543a48351520012ff80144653efc08ed49e62e17050fa4fc1c98cdd8e40c68f9512e3c687b4cfcc55eb8caeaa3fd44ab8ad00a8389c288eac128c4ee82832e3d0bb";
    p = "db5c4ccf412b17041b6e20b7e0cb45d807ef4da8282428e05e26782fef3251ea2f613d00a134842c6070aa6ebd2c38bb2a28c0f457601b159ae1f5af94dc8c9812f9b4e031ed1f08c64fdb6ffca71c0d3fc93c63596100b2dbce1d6cbf34fae84bccb859397f700114b4bba2e56678360f79c9df784e5f21e995f84fb8622543a48351520012ff80144653efc08ed49e62e17050fa4fc1c98cdd8e40c68f9512e3c687b4d000a836a83d21ea810c683a30e79e5fc8626e78961f076aef2f89ab";
    xq1 = "18ab1ad30607288890b387858a";
    xq2 = "19975a38d9368fa99deda7e986";
    xq = "bd7cc6c56616fb5b41f35d8de2a5c61d1894895dfa46aa95c2de4ea5dfe370eb4543d6670898431d29a9efbbb034347cfaeb8a4c55bcb52dca553dd93ae81fa9ad2bc2b5e6a42c3d3b237648a3907d8a11e6db8b008016064f94168f50fddd791c3d72f729c21e811e68db7ae5400a0f02906462241a33e8faa1c20f48aa12253a80ce75f87a81b37a80079a9ecc42d378ee0e19e913769b738628a14b772673b0fcbf777c55be99f974e1eff5bd8c9d190abff776f246e6614b2f8d81ed812c";
    q = "bd7cc6c56616fb5b41f35d8de2a5c61d1894895dfa46aa95c2de4ea5dfe370eb4543d6670898431d29a9efbbb034347cfaeb8a4c55bcb52dca553dd93ae81fa9ad2bc2b5e6a42c3d3b237648a3907d8a11e6db8b008016064f94168f50fddd791c3d72f729c21e811e68db7ae5400a0f02906462241a33e8faa1c20f48aa12253a80ce75f87a81b37a80079a9ecc42d378ee0e19e913769b738628a14b772673b0fcbf777c640b3b2f869336b823710bb296f32aaba903f90af79239c3d97279";
    m = "a25e0fbcc06a40ac879bba988e78b9df8f88b800077d580b615e3f2f663c9ce631eb0229ee7a4d5166122378bd055f686dd382e63c1564c96127ec191c88d1ba02fcf90f1efcfe29bdfab0fd6413dcb4027512d15c2e337f7111e7acc7679cd1b96581461466ca63af5fbfc0579d322ca02413b75a6dce25c529d6475fafbc5d07504a29039c0f567cbb9dff2938687a6e6d4633f9ae46383536060dc7efb90ff99a6e97449e8f8ad24853f70726953b3f1dc82222f8407f98250f2060777cbd05d0b2ed6abb99d86ac30974df41da16bc1e3abd610df6bcff49a2be932baeedf163911eec026dcbd5937734b47ceb48db97c27bd2a35338f90332b75374ae4404913ae82caf14bba7410c638676a544046aed0b6605562186a4ba6b3695ab25f900899bd03a8f3e68d548b4eadbd9a348a142618954b1b9d73245926d6c57e26454db887c6272280c2d0efff1b856762da7c8be77a0006da3ea589b21ee5efec36574c041d8e506af55de52083225242642cafdcdadfa9663e4424a2bb937d3";
    d = "1b0fad4a2011b5721699f46ec269744fed417400013f8eac903a5fdd3bb4c4d10851d5b1a7bf0ce2e6585b3eca2b8fe6bcf895d10a0390cc3adbfcaeda16cd9f007f7ed7da7f7fb19fa9c82a3b58a4c8ab138322e4b25dea92d85147769144cd9ee6403658bbcc65f28ff54ab944ddb21ab0adf3e467a25ba0dc4e613a9d4a0f81380c5c2b44ad3914c9efaa86debc1467bce108a99d0bb408de5657a1529ed7feef126e8b6fc297230c0dfe813118df352fa15b05d40abfeeb0d7dababe94c9e77e9a8ecb3eebe9823aec87d9f8225aef4465f3dfc5db367a60cf517603a7596a1fbf9e8b08f115b73ecf81b684bfad73c093df30ebc07e434caa87c09d55ab0b674b3858afa1939ba249c7265fd747731f2384d75b5fe6b9e06bbd3110787618290fb73cd42aca08f3f2ee855e393a5e6e835aa77cafc7d329c1dde7655abeeb8d74a015f8d2d36a3bc8939864dfd60da40c63435f76ac1b411af42d5145e95d1b0798a8e8b2ee23edb188228061fa60760993399b16b0cb2246c63ec809f3";
    RSAX931BuildTest(e, xp1, xp2, xp, xq1, xq2, xq, p, q, m, d, &publicKey,  &privateKey);

    
    e = 3;
    xp1 = "1a02a180a22a37d3ab4d5523fe";
    xp2 = "1179fc502dbe82ff9946c00392";
    xp = "d94a30017127e43b0005e99016c2f4efb8e0c91e61805b52478e35fddf3918e7a3a6e68013e5be75fa246981f222f5862ae79fdc67b3f7e849343ef1d0fb13301e314f267f862d33a66bae633a813b8b91518c95bb3dca18c2b6f02c30b0777cd253329cbcf4779d8d437fdff4c60f27738658f163081d08397e1353073f8df24675588ad215e4dc3615a59d2ad9b9815aeecb9a69fa37e036f36f115e909dbb02fd8a96cad3be182947e944e3a281c3cdf1ad35d4fd62c9417dcb0b3c8beffe8e558e6bab154b78ef43117c2808af1255f7c56dadf8e4ebe384f1eca918cae473e32caf7dc2d5250f6fe5ef00f68a997968dce7fbd2066da370a75aad1f7895";
    p = "d94a30017127e43b0005e99016c2f4efb8e0c91e61805b52478e35fddf3918e7a3a6e68013e5be75fa246981f222f5862ae79fdc67b3f7e849343ef1d0fb13301e314f267f862d33a66bae633a813b8b91518c95bb3dca18c2b6f02c30b0777cd253329cbcf4779d8d437fdff4c60f27738658f163081d08397e1353073f8df24675588ad215e4dc3615a59d2ad9b9815aeecb9a69fa37e036f36f115e909dbb02fd8a96cad3be182947e944e3a281c3cdf1ad35d4fd62c9417dcb0b3c8beffe8e558e6bab154b78ef43117c2808af1255f7c56dadf8e4ebe384f1eca918cae473e32caf7dd1126fd14c73ebcce310791625550d6582891713c38ac374993099";
    xq1 = "1fb621dce29cbb6a66cc3bf7d6";
    xq2 = "122325102c2e57c27d462e1e06";
    xq = "facc7f5f089ed9267363bc23c6c7b8f73208a36f61fa8ea8084ff777bc154107068061c4b9ead9788318eab4c3bf05729a4684f845ce9700aa70811530c50440d4ac19e47a47e5e78047e912996a79bbd9416fa10c3720174ccf8f65d32de16b0dd81187f1bee5b992792105f1d0fa191681cd305f3e113617f58b2d4a54c0cfd88db075c956c137e034fa5573fa71d67a8c076ee5e952a53369db3640438ab55e515e75a81861a99303dcc9c6efc7382cec83234742ccacc7b3e9485b002565c7af8351370aae57d26b2f2b93b7e2885429ab172c516593fb5c1b2b43957b273a2c87cf1d368e88c6f65b41815bac0d1cc9e6113d1d06a1f8ebdba6a1097343";
    q = "facc7f5f089ed9267363bc23c6c7b8f73208a36f61fa8ea8084ff777bc154107068061c4b9ead9788318eab4c3bf05729a4684f845ce9700aa70811530c50440d4ac19e47a47e5e78047e912996a79bbd9416fa10c3720174ccf8f65d32de16b0dd81187f1bee5b992792105f1d0fa191681cd305f3e113617f58b2d4a54c0cfd88db075c956c137e034fa5573fa71d67a8c076ee5e952a53369db3640438ab55e515e75a81861a99303dcc9c6efc7382cec83234742ccacc7b3e9485b002565c7af8351370aae57d26b2f2b93b7e2885429ab172c516593fb5c1b2b43957b273a2c87cf1d710fc707b5e6d58b6f3cb377b286466c4da41f592c749ebf97fca3";
    m = "d4e0061c2150cdf177232b89266af9153902cbd434a39cab549d997ed6dadcb4e84bbac6d49658428728a01bd7036bab4b0003f7e6ccf69df1effad985185c4ab0756237e4be92b2f42085d4388a29f461af98649c700d6dad5e0fe352513b578b3bff5f19b144e6304defe1b4fb43b37ecb4ed7c0e97377802d9e79c6d742837b3b71fd101fcf5ead4a114d9419af008a421d8a4c5efd4e6da8cc3c967502bd4cc1bda09e87bf7a1d0badaf0783a6dbef5c98359c59d6bda1cc9bacfaa962c841ddfa3670211e38a68998508ea1a2be519718a168d09cc0d2c1d0f8d56ca1d7199b0c4fc78ddb595f6681e5b1b96309251c0714bf134d46f58419a0273bfaab3328b59d75d8ada5e6e2745e816d17ded27b52f0b5632088ee6bf9675793adc52591abc3eacbf3ae4b59871ac9c94e98708801f534ad0a99791827e91cbacf7afbbd72e162698aeba0380f74462b8dd097fb576a99d70ad2117efee8f6ef51d6afd6fb8ce9b6c234ebf00d24d44ad505305e48af1a8037fed9a2a44235980d395bf69489309d37a04b66f236d223b1af759232ecf9d6556a71cd74c4936fc6d3efe6efb3311eea1574e0cebd657a9d36142f0719b95c98900bd32b9cdb6702ff92a7eefc5ec99c6f12709cb3a118cdaf56284dd195e0633dd689889924c42d3e6579e403bb3ecb08310128c673de301c3bea248f3bd0f63cab3f2545da9f8d6b";
    d = "237aabaf5ae2ccfd93db31ec3111d42e342b21f8b3709a1c8e1a443fce79cf737c0c9f21236e640b1686c559f92b3c9c8c8000a95122291a52fd5479962eba0c72be3b0950ca6dc87e056ba35ec1b1a8baf299661a12ace79ce502a5e30d89e3ec89ffe52ef2e0d1080cfd5048d48b489521e2794ad1933e955cefbef67935c09489e854d8054d3a723702e243599d2ac1b5af970cba7f8d1246ccb4c3be2b1f8ccaf4f01a6bf53f04d7479d2beb4679fd3a195e44b9a3ca45a219f229c6e5cc0afa545e680585097116eeb817c59b1fb843d9703c22c4cacdcaf82978e7704e8444820d4becf9e43a9115a648499081862f5683752de2367e40aef00689ff1c3a83010a2a02fd60bde977c71b5066fea69851107da6b3c26fc24ca84a0b8df91491bb3fda29e49ff7af5dd0adfbe3454739a4dac131bf48163de6a5af29c957017aac4e66c493f81440beaa685ff96c323c0f334dbb057055a96a8e7dd8297d229c9e915f2b3b7a4cb33cb5279df74b710e5b178eb456f56c07d64afb55f513df7dec96c388184208da0db6088d410e9aae8ffb46fdcc7b813d5c6a28c49a65ed1956711fb321b89ec38172747c0e09aee2ce756f84bc2f00703e8c35f9d2448a1b24dfea1c45c50d75ba01fb8eb4ae1cabcf8cc9ee5974fe9c14958958fbddc93c5d40daaa1c22e3ffcd00d9eca5d29d030c3491aacc2bb50d30fb4667bab3";
    RSAX931BuildTest(e, xp1, xp2, xp, xq1, xq2, xq, p, q, m, d, &publicKey,  &privateKey);

#endif
   
    bzero(inputpadded, 128);
    bzero(outputpadded, 128);
    bcopy(keydata->bytes, inputpadded, keydata->len);
    decryptedKeyLen = 128;
    retval = CCRSACryptorCrypt(privateKey, inputpadded, 128, encryptedKey, &encryptedKeyLen);

    ok(retval == 0, "RSA Raw Private Key Crypt");
    accum += retval;

    retval = CCRSACryptorCrypt(publicKey, encryptedKey, encryptedKeyLen, outputpadded, &decryptedKeyLen);

    ok(retval == 0, "RSA Raw Private Key Crypt");
    accum += retval;
    bcopy(outputpadded, decryptedKey, keydata->len);
    
 	dekeydata = bytesToBytes(decryptedKey, keydata->len);
    ok(bytesAreEqual(dekeydata, keydata), "RSA Raw Encrypt/Decrypt Round-Trip");
	if(!bytesAreEqual(dekeydata, keydata)) {
        diag("expected: %s\n", bytesToHexString(keydata));
        diag("     got: %s\n", bytesToHexString(dekeydata));
        diag("len = %d\n", (int) decryptedKeyLen);
    }
    
    
    static const uint8_t		kAirTunesRSAPublicKey[] = 
    {
        0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xE7, 0xD7, 0x44, 0xF2, 0xA2, 0xE2, 0x78, 
        0x8B, 0x6C, 0x1F, 0x55, 0xA0, 0x8E, 0xB7, 0x05, 0x44, 0xA8, 0xFA, 0x79, 0x45, 0xAA, 0x8B, 0xE6, 
        0xC6, 0x2C, 0xE5, 0xF5, 0x1C, 0xBD, 0xD4, 0xDC, 0x68, 0x42, 0xFE, 0x3D, 0x10, 0x83, 0xDD, 0x2E, 
        0xDE, 0xC1, 0xBF, 0xD4, 0x25, 0x2D, 0xC0, 0x2E, 0x6F, 0x39, 0x8B, 0xDF, 0x0E, 0x61, 0x48, 0xEA, 
        0x84, 0x85, 0x5E, 0x2E, 0x44, 0x2D, 0xA6, 0xD6, 0x26, 0x64, 0xF6, 0x74, 0xA1, 0xF3, 0x04, 0x92, 
        0x9A, 0xDE, 0x4F, 0x68, 0x93, 0xEF, 0x2D, 0xF6, 0xE7, 0x11, 0xA8, 0xC7, 0x7A, 0x0D, 0x91, 0xC9, 
        0xD9, 0x80, 0x82, 0x2E, 0x50, 0xD1, 0x29, 0x22, 0xAF, 0xEA, 0x40, 0xEA, 0x9F, 0x0E, 0x14, 0xC0, 
        0xF7, 0x69, 0x38, 0xC5, 0xF3, 0x88, 0x2F, 0xC0, 0x32, 0x3D, 0xD9, 0xFE, 0x55, 0x15, 0x5F, 0x51, 
        0xBB, 0x59, 0x21, 0xC2, 0x01, 0x62, 0x9F, 0xD7, 0x33, 0x52, 0xD5, 0xE2, 0xEF, 0xAA, 0xBF, 0x9B, 
        0xA0, 0x48, 0xD7, 0xB8, 0x13, 0xA2, 0xB6, 0x76, 0x7F, 0x6C, 0x3C, 0xCF, 0x1E, 0xB4, 0xCE, 0x67, 
        0x3D, 0x03, 0x7B, 0x0D, 0x2E, 0xA3, 0x0C, 0x5F, 0xFF, 0xEB, 0x06, 0xF8, 0xD0, 0x8A, 0xDD, 0xE4, 
        0x09, 0x57, 0x1A, 0x9C, 0x68, 0x9F, 0xEF, 0x10, 0x72, 0x88, 0x55, 0xDD, 0x8C, 0xFB, 0x9A, 0x8B, 
        0xEF, 0x5C, 0x89, 0x43, 0xEF, 0x3B, 0x5F, 0xAA, 0x15, 0xDD, 0xE6, 0x98, 0xBE, 0xDD, 0xF3, 0x59, 
        0x96, 0x03, 0xEB, 0x3E, 0x6F, 0x61, 0x37, 0x2B, 0xB6, 0x28, 0xF6, 0x55, 0x9F, 0x59, 0x9A, 0x78, 
        0xBF, 0x50, 0x06, 0x87, 0xAA, 0x7F, 0x49, 0x76, 0xC0, 0x56, 0x2D, 0x41, 0x29, 0x56, 0xF8, 0x98, 
        0x9E, 0x18, 0xA6, 0x35, 0x5B, 0xD8, 0x15, 0x97, 0x82, 0x5E, 0x0F, 0xC8, 0x75, 0x34, 0x3E, 0xC7, 
        0x82, 0x11, 0x76, 0x25, 0xCD, 0xBF, 0x98, 0x44, 0x7B, 0x02, 0x03, 0x01, 0x00, 0x01, 0xD4, 0x9D
    };
    
    CCRSACryptorRef		key;
    
    retval = CCRSACryptorImport( kAirTunesRSAPublicKey, sizeof( kAirTunesRSAPublicKey ), &key );

    ok(retval == kCCSuccess, "Imported Airport Key");
    accum += retval;
    
    return accum != 0;
}
#endif /* CCRSA */

