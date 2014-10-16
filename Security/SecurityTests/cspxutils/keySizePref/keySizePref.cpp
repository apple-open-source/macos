/*
 * keySizePref.cpp - set/examime max RSA key size per system
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/simpleprefs.h>

#define kRSAKeySizePrefsDomain		"com.apple.crypto"
#define kRSAMaxKeySizePref			CFSTR("RSAMaxKeySize")
#define kRSAMacPublicExponentPref	CFSTR("RSAMaxPublicExponent")

static void usage(char **argv)
{
	printf("usage: \n");
	printf("   %s set keysize|pubexpsize <val>\n", argv[0]);
	printf("   %s get keysize|pubexpsize\n", argv[0]);
	printf("   %s illegal    -- set illegally large values for both\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	bool doSet = false;
	CFStringRef which = NULL;
	char *cWhich = NULL;
	int ourRtn = 0;
	bool doIllegal = false;
	
	if(argc < 2) {
		usage(argv);
	}
	if(!strcmp(argv[1], "set")) {
		doSet = true;
		if(argc != 4) {
			usage(argv);
		}
	}
	else if(!strcmp(argv[1], "get")) {
		if(argc != 3) {
			usage(argv);
		}
	}
	else if(!strcmp(argv[1], "illegal")) {
		if(argc != 2) {
			usage(argv);
		}
		doIllegal = true;
	}
	else {
		usage(argv);
	}
	if(!doIllegal) {
		if(!strcmp(argv[2], "keysize")) {
			which = kRSAMaxKeySizePref;
			cWhich = "Max Key Size";
		}
		else if(!strcmp(argv[2], "pubexpsize")) {
			which = kRSAMacPublicExponentPref;
			cWhich = "Max Public Exponent";
		}
		else {
			usage(argv);
		}
	}
	
	if(doSet || doIllegal) {
		MutableDictionary *prefs = NULL;
		UInt32 iVal = 0;
		try {
			prefs = new MutableDictionary(kRSAKeySizePrefsDomain, Dictionary::US_System);
		}
		catch(...) {
			/* create a new one */
			prefs = new MutableDictionary();
		}
		
		if(doIllegal) {
			SInt64 bigBad = 0x100000000LL;
			CFNumberRef cfVal = CFNumberCreate(NULL, kCFNumberSInt64Type, &bigBad);
			prefs->setValue(kRSAMaxKeySizePref, cfVal);
			prefs->setValue(kRSAMacPublicExponentPref, cfVal);
		}
		else {
			iVal = atoi(argv[3]);
			if(iVal == 0) {
				/* this means "remove" */
				prefs->removeValue(which);
			}
			else {
				CFNumberRef cfVal = CFNumberCreate(NULL, kCFNumberSInt32Type, &iVal);
				prefs->setValue(which, cfVal);
			}
		}
		bool success = prefs->writePlistToPrefs(kRSAKeySizePrefsDomain, 
			Dictionary::US_System);
		if(success) {
			if(doIllegal) {
				printf("Both prefs set to 0x100000000LL\n");
			}
			else if(iVal == 0) {
				printf("%s preference removed.\n", cWhich);
			}
			else {
				printf("%s set to %lu\n", cWhich, (unsigned long) iVal);
			}
		}
		else {
			printf("***Error setting %s\n", cWhich);
			ourRtn = -1;
		}
		delete prefs;
	}
	else {
		try {
			Dictionary prefs(kRSAKeySizePrefsDomain, Dictionary::US_System);
			CFNumberRef cfVal = (CFNumberRef)prefs.getValue(which);
			if(cfVal == NULL) {
				printf("...no %s pref found\n", cWhich);
				return 0;
			}
			if(CFGetTypeID(cfVal) != CFNumberGetTypeID()) {
				printf("***Badly formatted %s pref (1)\n", cWhich);
				return -1;
			}
			UInt32 u;
			if(!CFNumberGetValue(cfVal, kCFNumberSInt32Type, &u)) {
				printf("***Badly formatted %s pref (2)\n", cWhich);
			}
			printf("%s preference is currently %lu\n", cWhich, (unsigned long)u);
		}
		catch(...) {
			printf("...no %s prefs found\n", kRSAKeySizePrefsDomain);
		}
	}
	return 0;
}
