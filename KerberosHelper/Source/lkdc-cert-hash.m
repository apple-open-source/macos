
#import <Foundation/Foundation.h>
#import <OpenDirectory/OpenDirectory.h>

#import <CommonCrypto/CommonDigest.h>

static CFStringRef kerberosKDC = CFSTR("KerberosKDC");
static CFStringRef rootName = CFSTR("/Local/Default");
static CFStringRef realName = CFSTR("dsAttrTypeStandard:RealName");

static char *
sha1_hash(const void *data, size_t len)
{
    char *outstr, *cpOut;
    unsigned char digest[CC_SHA1_DIGEST_LENGTH];
    unsigned i;
    
    CC_SHA1(data, len, digest);

    cpOut = outstr = (char *)malloc((2 * CC_SHA1_DIGEST_LENGTH) + 1);
    if (outstr == NULL)
	return NULL;

    for(i = 0; i < sizeof(digest); i++, cpOut += 2)
	sprintf(cpOut, "%02X", (unsigned)digest[i]);
    *cpOut = '\0';
    return outstr;
}


static void
update_node(NSString *realm)
{
    ODNodeRef localRef = NULL;
    NSArray *attrs;
    ODRecordRef cfRecord;

    localRef = ODNodeCreateWithName(kCFAllocatorDefault,
				    kODSessionDefault,
				    rootName,
				    NULL);
    if (localRef == NULL)
	errx(1, "ODNodeCreateWithName");

    attrs = [NSArray arrayWithObject:(id)realName];

    cfRecord = ODNodeCopyRecord(localRef, kODRecordTypeConfiguration,
				kerberosKDC, (CFArrayRef)attrs, NULL);
    if (cfRecord == NULL) {
	NSDictionary *attributes;
	
	attributes = [[NSDictionary alloc] init];
	
	cfRecord = ODNodeCreateRecord(localRef, 
				      kODRecordTypeConfiguration,
				      kerberosKDC,
				      (CFDictionaryRef)attributes,
				      NULL);
	[attributes release];

	if (cfRecord == NULL)
	    errx(1, "failed to create node");

    }

    /* update record.realname with realname */

    bool r = ODRecordSetValue(cfRecord, (NSString *)realName,
			      [NSArray arrayWithObject:realm],
			      NULL);
    if (!r)
	errx(1, "ODRecordSetValue");

    ODRecordSynchronize(cfRecord, NULL);

    CFRelease(cfRecord);
    CFRelease(localRef);
}



int
main(int argc, char **argv)
{
    NSString* realm, *urealm;
    OSStatus ret;
    SecIdentityRef idRef = NULL;
    SecCertificateRef certRef = NULL;
    char *cert_hash = NULL;
    CFDataRef certData;
    
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    ret = SecIdentityCopySystemIdentity(kSecIdentityDomainKerberosKDC,
					 &idRef, NULL);
    if (ret) {
	cssmPerror("SecIdentityCopySystemIdentity", ret);
	return 1;
    }
    
    ret = SecIdentityCopyCertificate(idRef, &certRef);
    if (ret) {
	cssmPerror("SecIdentityCopyCertificate", ret);
	return 1;
    }
    
    /* now the actual cert data */
    certData = SecCertificateCopyData(certRef);
    if (certRef == NULL)
	errx(1, "SecCertificateCopyData");
	
    cert_hash = sha1_hash(CFDataGetBytePtr(certData), CFDataGetLength(certData));
    CFRelease(certData);
    if (cert_hash == NULL)
	errx(1, "Error obtaining cert hash");

    realm = [NSString stringWithFormat:@"LKDC:SHA1.%@", [NSString stringWithCString: cert_hash encoding:NSUTF8StringEncoding]];
    
    urealm = [realm uppercaseString];
    update_node(urealm);

    free(cert_hash);
    CFRelease(certRef);
    CFRelease(idRef);

    printf("realm: %s\n", [urealm UTF8String]);

    [pool drain];

    return 0;

}
