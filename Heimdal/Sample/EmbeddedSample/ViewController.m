//
//  ViewController.m
//

#import "ViewController.h"

#import <GSS/gssapi.h>
#import <GSS/gssapi_krb5.h>

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
    _queue = dispatch_queue_create("com.apple.GSSEmbeddedSample.credentail.queue", NULL);
    // Do any additional setup after loading the view, typically from a nib.
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
}

- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
	if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
	    return (interfaceOrientation != UIInterfaceOrientationPortraitUpsideDown);
	} else {
	    return YES;
	}
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
        
	    NSDictionary *attrs = @{ (id)kGSSICPassword : @"foobar" };
        
	    OM_uint32 maj_stat, min_stat;
	    gss_name_t name = GSS_C_NO_NAME;
	    gss_cred_id_t cred = NULL;
	    CFErrorRef error = NULL;
	    gss_buffer_desc buffer;
	    
	    gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID oid, gss_cred_id_t gcred) {
		    if (gcred) {
			    NSLog(@"destroy credential: %@", gcred);
			    OM_uint32 foo;
			    gss_destroy_cred(&foo, &gcred);
		    }
	    });
	    
	    char *str = "ktestuser@ADS.APPLE.COM";
	    buffer.value = (void *)str;
	    buffer.length = strlen(str);
	    
	    maj_stat = gss_import_name(&min_stat, &buffer, GSS_C_NT_USER_NAME, &name);
	    if (maj_stat) {
		    NSLog(@"failed to import name");
		    running = false;
		    return;
	    }
	    
	    maj_stat = gss_aapl_initial_cred(name, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)attrs, &cred, &error);
	    if (maj_stat)
		    NSLog(@"error: %d %@", (int)maj_stat, error);
	    
	    maj_stat = gss_inquire_cred(&min_stat, cred, &name, NULL, NULL, NULL);
	    if (maj_stat != GSS_S_COMPLETE) {
		    NSLog(@"error inquire name: %d", (int)maj_stat);
	    } else {
		    NSLog(@"inquire name: %@", name);
	    }
	    gss_release_name(&min_stat, &name);
	    
	    maj_stat = gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID mech, gss_cred_id_t gcred) {
		    if (gcred == NULL)
			    return;
		    OM_uint32 foo;
		    gss_name_t name2;

		    if (gss_inquire_cred(&foo, gcred, &name2, NULL, NULL, NULL) != GSS_S_COMPLETE)
			    return;
		    
		    gss_release_cred(&foo, &gcred);

		    NSLog(@"list name: %@", name2);

	    });
	    if (maj_stat)
		    NSLog(@"list error: %d", (int)maj_stat);
	    NSLog(@"complete");
	    running = false;
    });
    
}


@end
