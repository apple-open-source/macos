//
//  SecondViewController.m
//  GSSEmbeddedSample
//
//  Created by Love Hörnquist Åstrand on 2011-03-15.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "SecondViewController.h"

#undef __APPLE_API_PRIVATE
#import <GSS/gssapi.h>
#import <GSS/gssapi_krb5.h>



@implementation SecondViewController

/*
// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
    [super viewDidLoad];
}
*/

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


- (IBAction)authenticate:(id)sender {
    NSLog(@"authenticate");
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_name_t server_name = GSS_C_NO_NAME;
    gss_buffer_desc buffer;
    OM_uint32 maj_stat, min_stat;
    
    char *name = "ldap@dc02.ads.apple.com";
    buffer.value = name;
    buffer.length = strlen(name);

    maj_stat = gss_import_name(&min_stat, &buffer, GSS_C_NT_HOSTBASED_SERVICE, &server_name);
    if (maj_stat != GSS_S_COMPLETE) {
	NSLog(@"import_name maj_stat: %d min_stat: %d", (int)maj_stat, (int)min_stat);

        name = "ldap/dc02.ads.apple.com@ADS.APPLE.COM";
	buffer.value = name;
	buffer.length = strlen(name);

	maj_stat = gss_import_name(&min_stat, &buffer, GSS_KRB5_NT_PRINCIPAL_NAME, &server_name);
	if (maj_stat != GSS_S_COMPLETE) {
	    NSLog(@"import_name2 maj_stat: %d min_stat: %d", (int)maj_stat, (int)min_stat);
	    return;
	}
    }
    
    maj_stat = gss_init_sec_context(&min_stat, GSS_C_NO_CREDENTIAL, 
				    &ctx, server_name, GSS_KRB5_MECHANISM,
				    GSS_C_REPLAY_FLAG|GSS_C_INTEG_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
				    NULL, NULL, &buffer, NULL, NULL);
    if (maj_stat)
	NSLog(@"init_sec_context maj_stat: %d", (int)maj_stat);
    else
	NSLog(@"have a buffer of length: %d, success", (int)buffer.length);
    
    gss_release_name(&min_stat, &server_name);
    gss_release_buffer(&min_stat, &buffer);
}


- (void)dealloc
{
    [super dealloc];
}

@end
