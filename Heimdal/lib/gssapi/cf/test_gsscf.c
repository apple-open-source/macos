/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2011 - 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <GSS.h>
#include <GSSPrivate.h>
#include <GSSItem.h>
#include <stdio.h>

static void
run_tests(void)
{
    dispatch_queue_t q = dispatch_queue_create("test_gsscf", NULL);
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    CFErrorRef error = NULL;

    printf("[TEST] add\n");

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(attrs, kGSSAttrClass, kGSSAttrClassKerberos);
    CFDictionaryAddValue(attrs, kGSSAttrNameType, kGSSAttrNameTypeGSSUsername);
    CFDictionaryAddValue(attrs, kGSSAttrName, CFSTR("ktestuser@ADS.APPLE.COM"));

    GSSItemRef item = GSSItemAdd(attrs, NULL);
    if (item == NULL) {
	printf("[FAIL] failed to add\n");
    } else {    
	CFShow(item);
	CFRelease(item);
    }
    
    CFArrayRef items = GSSItemCopyMatching(attrs, &error);
    if (items == NULL) {
	printf("[FAIL] failed to find what was just added\n");
	exit(1);
    }
    

    CFIndex n, count = CFArrayGetCount(items);
    for (n = 0; n < count; n++) {
	
	item = (GSSItemRef)CFArrayGetValueAtIndex(items, n);
	
	CFMutableDictionaryRef opts = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFDictionaryAddValue(opts, kGSSAttrCredentialPassword, CFSTR("foobar"));

	GSSItemOperation(item, kGSSOperationAcquire, opts, q, ^(CFTypeRef type, CFErrorRef e) {
	    if (type) {
		CFShow(CFSTR("Managed to acquire credential\n"));
		CFShow(type);
	    }
	    if (e)
		CFShow(e);
	    dispatch_semaphore_signal(sema);
	});

	dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
	
	CFRelease(opts);
    }
    CFRelease(items);
    
    items = GSSItemCopyMatching(attrs, NULL);
    if (items) {
	count = CFArrayGetCount(items);
	for (n = 0; n < count; n++) {
	    printf("index: %lu\n", n);
	    item = (GSSItemRef)CFArrayGetValueAtIndex(items, n);

	    CFShow(item);
	    
	    GSSItemOperation(item, kGSSOperationCredentialDiagnostics, NULL, q, ^(CFTypeRef type, CFErrorRef e) {
		if (type && CFGetTypeID(type) == CFDataGetTypeID())
		    printf("diag: \n%.*s\n", (int)CFDataGetLength(type), CFDataGetBytePtr(type));

		dispatch_semaphore_signal(sema);
	    });
	    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
	}
	CFRelease(items);
    }
    
    printf("[PASS] add\n");

    /* delete */
    
    printf("[TEST] delete\n");

    if (!GSSItemDelete(attrs, NULL)) {
	printf("failed to delete\n");
	exit(1);
    }
    
    printf("[PASS] delete\n");

    /*
     * Try with password in keychain
     */

    printf("[TEST] keychain\n");

    CFDictionaryAddValue(attrs, kGSSAttrCredentialPassword, CFSTR("foobar"));

    item = GSSItemAdd(attrs, NULL);
    if (item == NULL) {
	printf("failed to add\n");
	exit(1);
    }
    
    GSSItemOperation(item, kGSSOperationAcquire, NULL, q, ^(CFTypeRef type, CFErrorRef e) {
	if (type) {
	    CFShow(CFSTR("Managed to acquire credential\n"));
	    CFShow(type);
	} else {
	    CFShow(CFSTR("failed to acquire credential\n"));
	    exit(1);
	}
	if (e)
	    CFShow(e);
	dispatch_semaphore_signal(sema);
    });
    
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    
    CFRelease(item);
    
    printf("[PASS] keychain\n");

    /* 
     *
     */
    
    printf("[TEST] delete2\n");

    if (!GSSItemDelete(attrs, NULL)) {
	printf("failed to delete (second try)\n");
	exit(1);
    }

    printf("[PASS] delete2\n");
    
    /*
     *
     */
    
    CFRelease(attrs);
    
    printf("[TEST] keychain2\n");

    /*
     * Test entries in keychain
     */
    
    attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    CFDictionaryAddValue(attrs, kGSSAttrClass, kGSSAttrClassKerberos);
    CFDictionaryAddValue(attrs, kGSSAttrNameType, kGSSAttrNameTypeGSSUsername);
    CFDictionaryAddValue(attrs, kGSSAttrName, CFSTR("lha@SU.SE"));
    
    item = GSSItemAdd(attrs, NULL);
    if (item == NULL) {
	printf("failed to add lha@SU.SE\n");
    } else {    
	CFShow(item);
	CFRelease(item);
    }
    
    items = GSSItemCopyMatching(attrs, &error);
    if (items == NULL) {
	printf("[FAIL] failed to find lha@SU.SE\n");
	exit(1);
    }

    if (items) {
	count = CFArrayGetCount(items);
	for (n = 0; n < count; n++) {
	    printf("index: %lu\n", n);
	    item = (GSSItemRef)CFArrayGetValueAtIndex(items, n);
	    
	    CFShow(item);
	    
	    GSSItemOperation(item, kGSSOperationAcquire, NULL, q, ^(CFTypeRef type, CFErrorRef e) {
		if (type) {
		    CFShow(CFSTR("Managed to acquire credential\n"));
		    CFShow(type);
		}
		if (e)
		    CFShow(e);
		dispatch_semaphore_signal(sema);
	    });
	    
	    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
	}
	CFRelease(items);
    }
    
    printf("[PASS] keychain2\n");

    printf("[TEST] delete3\n");

    if (!GSSItemDelete(attrs, NULL)) {
	printf("[FAIL] failed to delete (third try)\n");
	exit(1);
    }

    printf("[PASS] delete3\n");

    CFRelease(attrs);

    dispatch_release(sema);
}

static void
change_password(const char *user, const char *oldpassword, const char *newpassword)
{
    CFStringRef u = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
    CFStringRef oldpw = CFStringCreateWithCString(kCFAllocatorDefault, oldpassword, kCFStringEncodingUTF8);
    CFStringRef newpw = CFStringCreateWithCString(kCFAllocatorDefault, newpassword, kCFStringEncodingUTF8);
    dispatch_queue_t q = dispatch_queue_create("test_gsscf-cpw", NULL);
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    CFErrorRef error = NULL;

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(attrs, kGSSAttrClass, kGSSAttrClassKerberos);
    CFDictionaryAddValue(attrs, kGSSAttrNameType, kGSSAttrNameTypeGSSUsername);
    CFDictionaryAddValue(attrs, kGSSAttrName, u);

    printf("foo\n");

    GSSItemRef item = GSSItemAdd(attrs, NULL);
    if (item == NULL) {
	printf("[FAIL] failed to add\n");
    } else {    
	CFShow(item);
	CFRelease(item);
    }
    
    CFArrayRef items = GSSItemCopyMatching(attrs, &error);
    if (items == NULL) {
	printf("failed to find what was just added\n");
	exit(1);
    }
    
    CFIndex n, count = CFArrayGetCount(items);
    for (n = 0; n < count; n++) {
	
	item = (GSSItemRef)CFArrayGetValueAtIndex(items, n);
	
	CFShow(item);

	CFMutableDictionaryRef opts = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFDictionaryAddValue(opts, kGSSOperationChangePasswordOldPassword, oldpw);
	CFDictionaryAddValue(opts, kGSSOperationChangePasswordNewPassword, newpw);

	GSSItemOperation(item, kGSSOperationChangePassword, opts, q, ^(CFTypeRef type, CFErrorRef e) {
	    if (e == NULL) {
		CFShow(CFSTR("Managed to change password\n"));
	    } else {
		CFShow(CFSTR("Failed to change password\n"));
		CFShow(e);
	    }
	    dispatch_semaphore_signal(sema);
	});

	dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
	
	CFRelease(opts);
    }
    CFRelease(items);

    CFRelease(attrs);

    CFRelease(u);
    CFRelease(newpw);
    CFRelease(oldpw);
}

struct checkrules {
    const char *name;
    CFStringRef input;
    CFStringRef result;
};

static void
checkRulesTable(CFMutableDictionaryRef rules, struct checkrules *checks, size_t count)
{
    CFStringRef match;
    size_t n;

    for (n = 0 ; n < count; n++) {
	printf("[TEST] %s\n", checks[n].name);

	match = GSSRuleGetMatch(rules, checks[n].input);
	if (match && checks[n].result == NULL) {
	    printf("expected no result but did get some\n");
	    printf("[FAIL] %s\n", checks[n].name);
	} else if (match == NULL && checks[n].result) {
	    printf("expected result but didn't get any\n");
	    printf("[FAIL] %s\n", checks[n].name);
	} else {
	    printf("[PASS] %s\n", checks[n].name);
	}
    }
}


static void
checkRules(void)
{
    CFMutableDictionaryRef rules;

    CFStringRef hostrule = CFSTR("hostrule");
    CFStringRef domainrule = CFSTR("domainrule");
    CFStringRef domainrule2 = CFSTR("domainrule2");
    CFStringRef plainhostrule = CFSTR("plainhostrule");
    CFStringRef plaindomainrule = CFSTR("plaindomainrule");
    CFStringRef macrule = CFSTR("macrule");
    CFStringRef allrule = CFSTR("allrule");

    rules = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (rules == NULL) {
	printf("[FAIL] ENOMEM\n");
	exit(1);
    }


    printf("[TEST] build rules\n");

    /* plain rules */
    GSSRuleAddMatch(rules, CFSTR("plainhost.me.com"), plainhostrule);
    GSSRuleAddMatch(rules, CFSTR(".plaindomain.me.com"), plaindomainrule);

    /* url rules */
    GSSRuleAddMatch(rules, CFSTR("https://host.apple.com"), hostrule);
    GSSRuleAddMatch(rules, CFSTR("https://.apple.com"), domainrule);
    GSSRuleAddMatch(rules, CFSTR("https://host.icloud.com/"), hostrule);
    GSSRuleAddMatch(rules, CFSTR("https://.icloud.com/"), domainrule);
    GSSRuleAddMatch(rules, CFSTR("https://.icloud.com/foo/"), domainrule2);

    /* any rule */
    GSSRuleAddMatch(rules, CFSTR("any://.mac.com/"), macrule);

    printf("[PASS] build rules\n");

    struct checkrules checks[] = {
	{ "hostrule",		CFSTR("https://HOST.apple.com"), hostrule },
	{ "hostrule2",		CFSTR("https://host.apple.com"), hostrule },
	{ "domainrule",		CFSTR("https://host2.apple.com"), domainrule },
	{ "hostrule3",		CFSTR("https://HOST.icloud.com"), hostrule },
	{ "hostrule4",		CFSTR("https://host.icloud.com"), hostrule },
	{ "domainrule2",	CFSTR("https://host2.icloud.com/foo"), domainrule },
	{ "domainrule3",	CFSTR("https://host2.icloud.com/foo"), domainrule },
	{ "domainrule4",	CFSTR("https://host2.icloud.com/bar/"), domainrule },
	{ "no match",		CFSTR("host.h5l.org"), NULL },
	{ "plain match",	CFSTR("http://plainhost.me.com"), plainhostrule },
	{ "plain no match",	CFSTR("http://plainhost2.me.com"), NULL },
	{ "plain domain match",	CFSTR("http://host.plaindomain.me.com"), plaindomainrule },
	{ "plain domain no",	CFSTR("http://host.2plaindomain.me.com"), NULL },
	{ "any http",		CFSTR("http://foo.mac.com/"), macrule },
	{ "any https",		CFSTR("https://foo.mac.com/"), macrule },
	{ "any https (bar)",	CFSTR("https://bar.mac.com/"), macrule },
    };

    checkRulesTable(rules, checks, 0 /* sizeof(checks)/sizeof(checks[0]) */);


    CFRelease(rules);

    struct checkrules checks2[] = {
	{ "all http",		CFSTR("http://somehost.rule.com/"),		allrule },
	{ "all http path",	CFSTR("http://somehost.rule.com/path/"),	allrule },
	{ "all http path2",	CFSTR("http://somehost.rule.com/path"),		allrule },
	{ "all http path2",	CFSTR("http://somehost.rule.com:8080/path"),	allrule },
	{ "all http no /",	CFSTR("http://somehost.rule.com"),		allrule },
	{ "all http port /",	CFSTR("http://somehost.rule.com:8080/"),	allrule },
	{ "all http port no/",	CFSTR("http://somehost.rule.com:8080"),		allrule },
	{ "all https",		CFSTR("http://somehost.rule.com/"),		allrule },
	{ "all https path",	CFSTR("https://somehost.rule.com/path/"),	allrule },
	{ "all https path2",	CFSTR("https://somehost.rule.com/path"),	allrule },
	{ "all https path3",	CFSTR("https://somehost.rule.com:3443/path"),	allrule },
	{ "all https port",	CFSTR("https://somehost.rule.com:3443/"),	allrule },
	{ "all https port no/",	CFSTR("https://somehost.rule.com:3443"),	allrule },
	{ "all https no /",	CFSTR("https://somehost.rule.com"),		allrule },
	{ "all http .se /",	CFSTR("https://somehost.rule.se/"),		NULL },
	{ "all http .se no /",	CFSTR("https://somehost.rule.se"),		NULL }
    };


    rules = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (rules == NULL) {
	printf("[FAIL] ENOMEM\n");
	exit(1);
    }

    GSSRuleAddMatch(rules, CFSTR("https://.com/"), allrule);
    GSSRuleAddMatch(rules, CFSTR("http://.com/"), allrule);
    
    checkRulesTable(rules, checks2, sizeof(checks2)/sizeof(checks2[0]));

    CFRelease(rules);
}

int
main(int argc, char **argv)
{

    if (argc > 3) {
	change_password(argv[1], argv[2], argv[3]);
    } else if (argc == 2 && strcmp(argv[1], "gssitem") == 0) {
	run_tests();
    } else {
	checkRules();
    }

    return 0;
}
