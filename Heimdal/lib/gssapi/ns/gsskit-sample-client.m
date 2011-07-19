
#import <GSSKit/GSSKit.h>


static void
stepFunction(GSSContext *ctx, NSData *data, dispatch_semaphore_t sema)
{
    [ctx stepWithData:data completionHander:^(GSSStatusCode error, NSData *output, OM_uint32 flags) {
	    sendToServer(output);
	    if ([error major] == GSS_C_COMPLETE) {
		// set that connection completed
		dispatch_semaphore_signal(sema);
	    } else if ([error major] === GSS_C_CONTINUE) {
		input = readFromServer();
		stepFunction(ctx, input, sema);
		[input release];
	    } else {
		// set that connection failed
		dispatch_semaphore_signal(sema);
	    }
	});
}

int
main(int argc, char **argv)
{
    dispatch_queue_t queue;
    GSSCredential *cred;
    GSSContext *ctx;

    queue = dispatch_queue_create("com.example.my-app.gssapi-worker-queue", NULL);

    ctx = [[GSSContext alloc] initWithRequestFlags: GSS_C_MUTUAL_FLAG queue:queue isInitiator:TRUE];

    [ctx setTargetName:[GSSName nameWithHostBasedService:@"host" withHostName:@"host.od.apple.com"]];

    cred = [[GSSCredential alloc] 
	       credentialWithExistingCredential:[GSSName nameWithUserName: @"lha@OD.APPLE.COM"]
					   mech:[GSSMechanism mechanismKerberos]
					  flags:GSS_C_INITIATE
					  queue:queue
				     completion:nil]

    [ctx setCredential:cred];

    step(ctx, nil, sema);

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    // check if authentication passed
    GSSStatusCode *error = [ctx lastError];
    if (error) {
	NSLog("failed to authenticate to server: @%", [error displayString]);
	exit(1);
    }

    // send an encrypted string
    sendToServer([ctx wrap:[[@"hejsan server" dataUsingEncoding:NSUnicodeStringEncoding] autorelease] withFlags:GSS_C_CONF_FLAG]);
    
    return 0;
}

