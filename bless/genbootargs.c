/*
 *  genbootargs.c
 *  bless
 *
 *  Created by Shantonu Sen on 1/7/05.
 *  Copyright 2005 Apple Computer, Inc. All rights reserved.
 *
 */

#include <libc.h>
#include <err.h>
#include <CoreFoundation/CoreFoundation.h>

/*
 * plist will look something like this
 <plist version="1.0">
 <dict>
	<key>3942261</key>
	<dict>
		<key>-v</key>
		<string>Preserve verbose mode</string>
	</dict>
 </dict>
 </plist>
*/ 

void mergeToMaster(const void *key, const void *value, void *context);
void extractBootArgs(const void *key, const void *value, void *context);
CFComparisonResult compareStrings(const void *val1, const void *val2, void *context);

int main(int argc, char *argv[]) {

	int i;
	CFMutableDictionaryRef masterPlist = NULL;
	CFMutableDictionaryRef bootArgs = NULL;
	CFIndex count, j;
	const void **keys = NULL;
	CFArrayRef argArrayTmp = NULL;
	CFMutableArrayRef argArray = NULL;
	
	if(argc == 1) {
		fprintf(stderr, "Usage: %s plist1 plist2\n", getprogname());
		exit(1);
	}
	
	masterPlist = CFDictionaryCreateMutable(kCFAllocatorDefault,0,
											&kCFTypeDictionaryKeyCallBacks,
											&kCFTypeDictionaryValueCallBacks);
	
	bootArgs = CFDictionaryCreateMutable(kCFAllocatorDefault,0,
										 &kCFTypeDictionaryKeyCallBacks,
										 &kCFTypeDictionaryValueCallBacks);
	
	for(i=1; i < argc; i++) {
		CFURLRef url = NULL;
		CFDataRef data = NULL;
		CFDictionaryRef dict = NULL;
		CFStringRef errorString = NULL;
		SInt32 ecode = 0;
		char *file = argv[i];
		fprintf(stderr, "Processing %s\n", file);

		url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, 
                                                      (UInt8 *)file,
													  strlen(file), false);
		if(url == NULL) {
			warn("CFURLCreateFromFileSystemRepresentation(%s)", file);
			continue;
		}
		
		if(!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, url,
													 &data, NULL, NULL,
													 &ecode)) {
			warnx("CFURLCreateDataAndPropertiesFromResource(%s): %d", file,
				  ecode);
			CFRelease(url);
			continue;
		}
		
		CFRelease(url); url = NULL;
		
		dict = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, data,
											   kCFPropertyListImmutable,
											   &errorString);
		if(dict == NULL) {
			warnx("CFPropertyListCreateFromXMLData(%s)", file);
			if(errorString) {
				CFShow(errorString);
				CFRelease(errorString);
			}
			CFRelease(data);
			continue;
		}
		
		CFRelease(data); data = NULL;
		
		if(CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
			warnx("Wrong plist type for %s", file);
			CFRelease(dict);
			continue;
		}
		
		CFDictionaryApplyFunction(dict, mergeToMaster, masterPlist);
		
		CFRelease(dict); dict = NULL;
		
	}
	
//	CFShow(masterPlist);
	
	// now that we have a master plist, extract boot args
	CFDictionaryApplyFunction(masterPlist, extractBootArgs, bootArgs);

//	CFShow(bootArgs);
	
	count = CFDictionaryGetCount(bootArgs);
	keys = calloc(count, sizeof(void *));
	CFDictionaryGetKeysAndValues(bootArgs, keys, NULL);
	
	argArrayTmp = CFArrayCreate(kCFAllocatorDefault, keys, count, &kCFTypeArrayCallBacks);
	argArray = CFArrayCreateMutableCopy(kCFAllocatorDefault, count, argArrayTmp);
	CFRelease(argArrayTmp);
	
	CFArraySortValues(argArray, CFRangeMake(0, count), compareStrings, NULL);

	CFShow(argArray);
	printf("const char *preserve_boot_args[] = {\n");
	
	for(j=0; j < count; j++) {
		CFStringRef barg = (CFStringRef)CFArrayGetValueAtIndex(argArray,j);
		CFStringRef comment = (CFStringRef)CFDictionaryGetValue(bootArgs, barg);
		
		char bargc[1024];
		char commentc[1024];
		
		if(!CFStringGetCString(barg,bargc,sizeof(bargc), kCFStringEncodingUTF8)) {
			continue;
		}
		if(!CFStringGetCString(comment,commentc,sizeof(commentc), kCFStringEncodingUTF8)) {
			continue;
		}
		
		printf("\t\"%s\"%s\t/* %s */\n", bargc, (j < count - 1) ? "," : "",
			   commentc);
	}
	printf("};\n");
	
	CFRelease(argArray); argArray = NULL;
	
	CFRelease(masterPlist); masterPlist = NULL;
	CFRelease(bootArgs); bootArgs = NULL;
	return 0;
}

void mergeToMaster(const void *key, const void *value, void *context)
{
	CFMutableDictionaryRef masterDict = (CFMutableDictionaryRef)context;
	
	CFDictionaryAddValue(masterDict, key, value);
}

void extractBootArgs(const void *key, const void *value, void *context)
{
	CFMutableDictionaryRef bootArgs = (CFMutableDictionaryRef)context;
	CFDictionaryRef argDict = (CFDictionaryRef)value;

	// we can ignore the key. the value has "{ bootarg = description }"
	CFDictionaryApplyFunction(argDict, mergeToMaster, bootArgs);
}

CFComparisonResult compareStrings(const void *val1, const void *val2, void *context)
{
	CFStringRef a = (CFStringRef)val1;
	CFStringRef b = (CFStringRef)val2;

	return CFStringCompare(a, b, 0);

}
