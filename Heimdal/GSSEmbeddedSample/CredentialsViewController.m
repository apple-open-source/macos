//
//  FirstViewController.m
//  GSSEmbeddedSample
//
//  Created by Love Hörnquist Åstrand on 2011-03-15.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "CredentialsViewController.h"
#undef __APPLE_API_PRIVATE
#import <GSS/gssapi.h>
#import <GSS/gssapi_krb5.h>


@implementation CredentialsViewController

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
    queue = dispatch_queue_create("com.apple.GSSEmbeddedSample.credentail.queue", NULL);
    [super viewDidLoad];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}


- (void)didReceiveMemoryWarning
{
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc. that aren't in use.
}


- (void)viewDidUnload
{
    [super viewDidUnload];

    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}


- (void)dealloc
{
    [super dealloc];
}

- (IBAction)addCredential:(id)sender {
    static bool running = false;
    
    if (running)
	return;

    NSLog(@"Add credential");

    running = true;
    /*
     * Run on queue in background since the all operations are blocking
     */
    dispatch_async(queue, ^{
	
	NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
			       @"foobar", kGSSICPassword,
			       nil];
	
	OM_uint32 maj_stat, min_stat;
	gss_name_t name = GSS_C_NO_NAME;
	gss_cred_id_t cred = NULL;
	CFErrorRef error = NULL;
	gss_buffer_desc buffer;
	
	char *str = "ktestuser@ADS.APPLE.COM";
	buffer.value = (void *)str;
	buffer.length = strlen(str);
	
	gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID oid, gss_cred_id_t cred) {
	    OM_uint32 foo;
	    gss_destroy_cred(&foo, &cred);
	});
	
	
	maj_stat = gss_import_name(&min_stat, &buffer, GSS_C_NT_USER_NAME, &name);
	if (maj_stat) {
	    NSLog(@"failed to import name");
	    running = false;
	    return;
	}
	
	maj_stat = gss_aapl_initial_cred(name, GSS_KRB5_MECHANISM, (CFDictionaryRef)attrs, &cred, &error);
	if (maj_stat)
	    NSLog(@"error: %d %@", (int)maj_stat, error);
	
	
	gss_iter_creds(&min_stat, 0, NULL, ^(gss_OID mech, gss_cred_id_t cred) {
	    OM_uint32 foo;
	    gss_name_t name2;
	    gss_buffer_desc buffer2;
	    if (gss_inquire_cred(&foo, cred, &name2, NULL, NULL, NULL) != GSS_S_COMPLETE)
		return;
	    
	    gss_release_cred(&foo, &cred);
	    
	    gss_display_name(&foo, name2, &buffer2, NULL);
	    gss_release_name(&foo, &name2);
	    
	    NSLog(@"name: %.*s", (int)buffer2.length, buffer2.value);
	    gss_release_buffer(&foo, &buffer2);
	});
	running = false;
    });

}
@end
