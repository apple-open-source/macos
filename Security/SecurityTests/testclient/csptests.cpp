#include "csptests.h"

#include <security_cdsa_client/keyclient.h>
#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/macclient.h>
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/wrapkey.h>

using namespace CssmClient;

static void testCrypt(const Guid &cspGuid);
static void testDigests(const Guid &cspGuid);
static void testRandom(const Guid &cspGuid);
static void testMac(const Guid &cspGuid);
static void testWrap(const Guid &cspGuid);


void csptests()
{
	testCrypt(gGuidAppleCSP);
	testCrypt(gGuidAppleCSPDL);
	testDigests(gGuidAppleCSP);
	testRandom(gGuidAppleCSP);
	testRandom(gGuidAppleCSPDL);
	testMac(gGuidAppleCSP);
	testMac(gGuidAppleCSPDL);
}

void testmac()
{
	testMac(gGuidAppleCSP);
}

void testwrap()
{
	testWrap(gGuidAppleCSP);
}

static void testCrypt(const Guid &cspGuid)
{
    printf("\n* performing encrypt/decrypt test...\n");
	
	CSP csp(cspGuid);

    printf("Generating key\n");
	GenerateKey genKey(csp, CSSM_ALGID_DES, 64);
	Key key = genKey(KeySpec(CSSM_KEYUSE_ANY, CSSM_KEYATTR_RETURN_DEFAULT));
    printf("done\n");

	// Gnerate IV
    printf("Generating iv\n");
	//CssmData iv = Random(csp, CSSM_ALGID_SHARandom)(8);
	CssmPolyData iv("12345678");

	CssmPolyData in("Om mani padme hum");
	printf("input=");
	dump(in);

	// Encrypt
    printf("Encrypting\n");

	Encrypt encrypt(csp, CSSM_ALGID_DES);
	encrypt.mode(CSSM_ALGMODE_CBCPadIV8);
	encrypt.padding(CSSM_PADDING_PKCS1);
	encrypt.initVector(iv);
	encrypt.key(key);
	CssmData cipher;
	CssmData remcipher;
	encrypt.encrypt(&in, 1, &cipher, 1);
	encrypt.final(remcipher);
	printf("ciphertext=");
	dump(cipher);
	printf("remainder=");
	dump(remcipher);

	// Decrypt
    printf("Decrypting\n");

	Decrypt decrypt(csp, CSSM_ALGID_DES);
	decrypt.key(key);
	decrypt.mode(CSSM_ALGMODE_CBCPadIV8);
	decrypt.padding(CSSM_PADDING_PKCS1);
	decrypt.initVector(iv);
	CssmData plain;
	CssmData remplain;
	CssmData inp[] = { cipher, remcipher };
	decrypt.decrypt(inp, 2, &plain, 1);
	decrypt.final(remplain);
	printf("plaintext=");
	dump(plain);
	printf("remainder=");
	dump(remplain);

    printf("end encrypt/decrypt test\n");
}

static void testDigests(const Guid &cspGuid)
{
	printf("\n* performing digest test...\n");
	CSP csp(cspGuid);
	Digest md5(csp, CSSM_ALGID_MD5);
	StringData data("Once in a blue moon");
	DataBuffer<20> digest;
	md5.digest(data, digest);
	printf("digest=");
	dump(digest);
}


static void testRandom(const Guid &cspGuid)
{
	printf("\n* performing random test...\n");
	CSP csp(cspGuid);
	CssmData result = Random(csp, CSSM_ALGID_APPLE_YARROW)(16);
	assert(result.length() == 16);
	printf("result=");
	dump(result);
	free(result.data());
}


void dump(const CssmData &data)
{
	unsigned char *p = data;
	for (uint32 n = 0; n < data.length(); n++)
		printf("%2.2x", p[n]);
	printf("\n");
}

static void testMac(const Guid &cspGuid)
{
    printf("\n* performing mac test...\n");
	
	CssmData keyData;
	keyData.Length = 8;
	keyData.Data = (uint8 *)"1234567";

	CSP csp(cspGuid);

	Key key(csp, keyData);

    printf("Generating key\n");
	GenerateKey genKey(csp, CSSM_ALGID_DES, 64);
	key = genKey(KeySpec(CSSM_KEYUSE_ANY, CSSM_KEYATTR_RETURN_DEFAULT));
    printf("done\n");

	GenerateMac mac(csp, CSSM_ALGID_SHA1HMAC);
	mac.key(key);
	StringData data("Om mani padme hum");
	DataBuffer<20> signature;
	mac.sign(data, signature);
	printf("signature=");
	dump(signature);
	
	VerifyMac vmac(csp, CSSM_ALGID_SHA1HMAC);
	vmac.key(key);
	vmac.verify(data, signature);
	printf("testing mac verify\n");
	
	bool failed = false;
	try
	{
		printf("testing mac verify with bad data\n");
		StringData baddata("not even close to the original");
		vmac.verify(baddata, signature);
	}
	catch(const CssmError &e)
	{
		printf("caught verify error\n");
		failed = true;
		if (e.osStatus() != CSSMERR_CSP_VERIFY_FAILED)
			throw;
	}
	if (!failed) throw Error(CSSMERR_CSP_VERIFY_FAILED);

    printf("end mac test\n");
}

static void testWrap(const Guid &cspGuid)
{
    printf("\n* performing wrap test...\n");
	
	CssmData keyData;
	keyData.Length = 8;
	keyData.Data = (uint8 *)"1234567";

	CSP csp(cspGuid);

	Key key(csp, keyData);
	
	Key wrappedKey;
	GenerateKey genKey(csp, CSSM_ALGID_RC4, 128);
	key = genKey(KeySpec(CSSM_KEYUSE_ANY, CSSM_KEYATTR_RETURN_DEFAULT));

	WrapKey wrapKey(csp, CSSM_ALGID_RC2);
	wrapKey.key(key);

	AccessCredentials(cred);
	wrapKey.cred(&cred);
	wrapKey.mode(CSSM_ALGMODE_CBC_IV8);
	CssmData initVec;
	initVec.Length = 8;
	initVec.Data = (uint8 *)"12345678";
	wrapKey.initVector(initVec);

	wrappedKey=wrapKey(key);
	
		
	printf("end wrap test\n");
}
