//
//  ViewController.m
//

#import "ViewController.h"

#import <GSS/GSS.h>


@interface ViewController ()

@end

@implementation ViewController

- (void)didReceiveMemoryWarning
{
        [super didReceiveMemoryWarning];
        // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

- (void)viewDidLoad
{
        [super viewDidLoad];
        _queue = dispatch_queue_create("com.apple.GSSSampleIOS.credential.queue", NULL);
        [self listCredentials:self];
}

- (NSUInteger)supportedInterfaceOrientations
{
        return UIInterfaceOrientationMaskPortrait;
}

- (void)kdestroyAll
{
        OM_uint32 min_stat;
        
        gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID oid, gss_cred_id_t gcred) {
                if (gcred) {
                        NSLog(@"destroy credential: %@", gcred);
                        OM_uint32 foo;
                        gss_destroy_cred(&foo, &gcred);
                }
	    });
}

- (gss_cred_id_t)getACred
{
        OM_uint32 min_stat;
	__block gss_cred_id_t first = NULL;
        
        gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID oid, gss_cred_id_t gcred) {
                if (gcred) {
			CFRetain(gcred);
			first = gcred;
		}
	});
	return first;
}


- (void)checkNoCredentials
{
        __block unsigned num = 0;
        OM_uint32 min_stat;
        
        gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID oid, gss_cred_id_t gcred) {
                if (gcred != NULL) {
                        NSLog(@"unexpected cred: %@", gcred);
                        num++;
                }
        });
        if (num)
                NSLog(@"FAIL too many credential (more then 0)");
}

- (gss_cred_id_t)acquire_cred:(NSString *)name password:(NSString *)password
{
        OM_uint32 maj_stat, min_stat;
        gss_name_t gname = GSS_C_NO_NAME;
        gss_cred_id_t cred = NULL;
        CFErrorRef error = NULL;
        gss_buffer_desc buffer;
        
        NSLog(@"acquire: %@", name);

        const char *str = [name UTF8String];
        buffer.value = (void *)str;
        buffer.length = strlen(str);

        maj_stat = gss_import_name(&min_stat, &buffer, GSS_C_NT_USER_NAME, &gname);
        if (maj_stat) {
                NSLog(@"failed to import name: %@", name);
                return NULL;
        }
        
        NSDictionary *attrs = @{ (id)kGSSICPassword : password };

        maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
        gss_release_name(&min_stat, &gname);
        if (maj_stat) {
                NSLog(@"error: %d %@", (int)maj_stat, error);
                goto out;
        }
        
        NSLog(@"acquire: %@ done", name);
        
        if (cred) {
                CFUUIDRef uuid = GSSCredentialCopyUUID(cred);
                if (uuid == NULL) {
                        NSLog(@"GSSCredentialCopyUUID error failed to get credential");
                        CFRelease(cred);
                        cred = NULL;
                        goto out;
                }
                gss_cred_id_t dupCred = GSSCreateCredentialFromUUID(uuid);
                if (dupCred == GSS_C_NO_CREDENTIAL) {
                        NSLog(@"GSSCreateCredentialFromUUID error failed to get credential");
                        CFRelease(cred);
                        cred = NULL;
                        goto out;
                }
                
                CFRelease(uuid);
                CFRelease(dupCred);
        }

  out:
        return cred;
}

- (BOOL)checkCredentialCacheName
{
        OM_uint32 maj_stat;
        gss_name_t gname = GSS_C_NO_NAME;
        gss_cred_id_t cred = NULL;
        CFErrorRef error = NULL;
        
        
        gname = GSSCreateName(@"ktestuser@ADS.APPLE.COM", GSS_C_NT_USER_NAME, NULL);
        if (gname == NULL)
                return false;

        NSString *password = @"foobar";
        
        CFUUIDRef uuid = CFUUIDCreateFromString(NULL, CFSTR("E5ECDD5B-1348-4452-A31A-A0A791F94114"));
        
        NSDictionary *attrs = @{
                                (id)kGSSICPassword : password,
                                (id)kGSSICKerberosCacheName : @"XCACHE:E5ECDD5B-1348-4452-A31A-A0A791F94114"
                                };
        
        maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
        CFRelease(gname);
        if (maj_stat) {
                NSLog(@"error: %d %@", (int)maj_stat, error);
                return false;
        }
        
        CFUUIDRef creduuid = GSSCredentialCopyUUID(cred);
        
        if (!CFEqual(creduuid, uuid))
                return false;
        
        CFRelease(cred);
        
        
        return true;
}

- (BOOL)authenticate:(gss_cred_id_t)cred nameType:(gss_OID)nameType toServer:(NSString *)serverName
{
        gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
        gss_name_t server_name = GSS_C_NO_NAME;
        gss_buffer_desc buffer;
        OM_uint32 maj_stat, min_stat;
	BOOL res;
        
        NSLog(@"acquire: %@ to %@", cred, serverName);

        
        const char *name = [serverName UTF8String];
        buffer.value = (void *)name;
        buffer.length = strlen(name);
        
        maj_stat = gss_import_name(&min_stat, &buffer, nameType, &server_name);
        if (maj_stat != GSS_S_COMPLETE) {
                NSLog(@"import_name maj_stat: %d min_stat: %d", (int)maj_stat, (int)min_stat);
                return FALSE;
        }
        
        maj_stat = gss_init_sec_context(&min_stat, cred,
                                        &ctx, server_name, GSS_KRB5_MECHANISM,
                                        GSS_C_REPLAY_FLAG|GSS_C_INTEG_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
                                        NULL, NULL, &buffer, NULL, NULL);
        if (maj_stat) {
                NSLog(@"FAIL init_sec_context maj_stat: %d", (int)maj_stat);
		res = FALSE;
        } else {
                NSLog(@"have a buffer of length: %d, success", (int)buffer.length);
		res = TRUE;
	}
        
        gss_release_name(&min_stat, &server_name);
        gss_release_buffer(&min_stat, &buffer);

	return res;
}



- (Boolean)testCSD11
{
        gss_cred_id_t cred = NULL;
        OM_uint32 min_stat;
        
        cred = [self acquire_cred:@"testuser@CSD11.APPLE.COM" password:@"testuser"];
        if (cred == NULL)
                return false;
        
        [self authenticate:cred nameType:GSS_C_NT_HOSTBASED_SERVICE toServer:@"HTTP@csd11.apple.com"];

        
        gss_release_cred(&min_stat, &cred);

        return true;
}

- (Boolean)testADS
{
        OM_uint32 maj_stat, min_stat;
        gss_name_t name = GSS_C_NO_NAME;
        gss_cred_id_t cred = NULL;
        
        cred = [self acquire_cred:@"ktestuser@ADS.APPLE.COM" password:@"foobar"];
        if (cred == NULL)
                return false;
        
        maj_stat = gss_inquire_cred(&min_stat, cred, &name, NULL, NULL, NULL);
        if (maj_stat != GSS_S_COMPLETE) {
                NSLog(@"error inquire name: %d", (int)maj_stat);
        } else {
                NSLog(@"inquire name: %@", name);
        }
        gss_release_name(&min_stat, &name);
        
        NSLog(@"start list");
        maj_stat = gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID mech, gss_cred_id_t gcred) {
                if (gcred == NULL)
                        return;
                OM_uint32 major, minor;
                gss_name_t name2;
                
                NSLog(@"list cred: %@", gcred);

                major = gss_inquire_cred(&minor, gcred, &name2, NULL, NULL, NULL);
                if (major != GSS_S_COMPLETE) {
                        NSLog(@"failed to inquire cred: %d/%d", major, minor);
                        return;
                }
                
                gss_release_cred(&minor, &gcred);
                
                NSLog(@"list name: %@", name2);
                
        });
        NSLog(@"end list");
        if (maj_stat)
                NSLog(@"list error: %d", (int)maj_stat);
        
        NSLog(@"authenticate");
        
        [self authenticate:cred nameType:GSS_C_NT_HOSTBASED_SERVICE toServer:@"ldap@dc02.ads.apple.com"];
        [self authenticate:cred nameType:GSS_KRB5_NT_PRINCIPAL_NAME toServer:@"ldap/dc02.ads.apple.com@ADS.APPLE.COM"];
        
        return true;
}

- (IBAction)listCredentials:(id)sender
{
        __block unsigned ncreds = 0;
        OM_uint32 min_stat;
        
        self.ticketView.text = @"<nocred>";
        
        
        NSMutableString *str = [NSMutableString string];
        
        gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID mech, gss_cred_id_t cred) {
                CFStringRef displayName = NULL;
                CFStringRef uuidName = NULL;
                gss_name_t name = NULL;
                CFUUIDRef uuid = NULL;
                
                if (cred == NULL)
                        return;
                
                ncreds++;
                
                name = GSSCredentialCopyName(cred);
                if (name == NULL)
                        goto out;
                
                displayName = GSSNameCreateDisplayString(name);
                if (displayName == NULL)
                        goto out;
                
                [str appendString:(__bridge NSString *)displayName];

                uuid = GSSCredentialCopyUUID(cred);
                if (uuid == NULL)
                        goto out;
                
                uuidName = CFUUIDCreateString(NULL, uuid);
               
                [str appendString:@" uuid: "];
                [str appendString:(__bridge NSString *)uuidName];
                

                out:
                [str appendString:@"\n"];
                if (displayName)
                        CFRelease(displayName);
                if (name)
                        CFRelease(name);

                if (uuidName)
                        CFRelease(uuidName);
                if (uuid)
                        CFRelease(uuid);
                
        });
        NSLog(@"num creds in list: %u", ncreds);
        self.ticketView.text = str;
        
}

- (IBAction)deleteAllCredentials:(id)sender
{
        self.ticketView.text = @"<nocred>";
        [self kdestroyAll];
}

- (IBAction)authServer:(id)sender
{
        gss_cred_id_t cred = [self getACred];
        NSString *res;
        
        if ([self authenticate:cred nameType:GSS_C_NT_HOSTBASED_SERVICE toServer:[self.authServerName text]])
                res = @"pass";
        else
                res = @"fail";
        
        self.authServerResult.text = res;
        
}

- (IBAction)nsURLFetch:(id)sender
{
        NSLog(@"nsURLFetch");
        
        NSURL *url = [NSURL URLWithString:self.urlTextField.text];
        NSError *error = NULL;

        self.urlResultTextView.text = [NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:&error];
        if ( self.urlResultTextView.text == NULL)
                self.urlResultTextView.text = [error localizedDescription];
        if ( self.urlResultTextView.text == NULL)
                self.urlResultTextView.text = @"why what ?, failed";
        
}

- (IBAction)acquirektestuserAtADS:(id)sender
{
        [self acquire_cred:@"ktestuser@ADS.APPLE.COM" password:@"foobar"];
        [self listCredentials:sender];
}



- (IBAction)addCredential:(id)sender {
        static bool running = false;
        
        NSLog(@"Add credential hit");
        
        if (running)
                return;
        
        NSLog(@"Add credential");
        
        running = true;
        /*
         * Run on queue in background since the all operations are blocking
         */
        dispatch_async(_queue, ^{
                
                NSLog(@"destroy all");
                [self kdestroyAll];
                NSLog(@"check none exists");
                [self checkNoCredentials];
                
                NSLog(@"test ADS");
                if (![self testADS])
                        goto out;
                
                [self kdestroyAll];
                [self checkNoCredentials];

                NSLog(@"test CSD11");
                if (![self testCSD11])
                        goto out;
                
                [self kdestroyAll];
                [self checkNoCredentials];

                NSLog(@"test checkCredentialCacheName");
                [self checkCredentialCacheName];
                
                [self kdestroyAll];

                NSLog(@"complete");

                out:
                running = false;
        });
        
}


@end
