//
//  DeviceCommon.m
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#import "DeviceCommon.h"
#import "MCDeviceDiscovery.h"

@interface DeviceCommon () <
QServerDelegate,
NSStreamDelegate,
NSNetServiceBrowserDelegate
>

@property (nonatomic, strong, readwrite) NSNetService *         lastService;
@property (nonatomic, strong, readwrite) NSRunLoop *            runLoop;


@end

@implementation DeviceCommon

- (id)initWithServerType:(NSString *)type port:(NSInteger)port withBonjour:(BOOL)bonjour
// See comment in header.
{
    self = [super init];
    if (self != nil) {
        self.services = [[NSMutableArray alloc] init];
        self.useBonjour = bonjour;
        self.port       = port;
        self.runLoop    = [NSRunLoop currentRunLoop];
    }
    return self;
}

#pragma mark - Connection management

- (void)openStreams
{
    
    assert(self.inputStream != nil);            // streams must exist but aren't open
    assert(self.outputStream != nil);
    assert(self.streamOpenCount == 0);
    
    NSLog(@"Openning streams\n");
    
    [self.inputStream  setDelegate:self];
    [self.inputStream  scheduleInRunLoop:self.runLoop forMode:NSDefaultRunLoopMode];
    [self.inputStream  open];
    
    [self.outputStream setDelegate:self];
    [self.outputStream scheduleInRunLoop:self.runLoop forMode:NSDefaultRunLoopMode];
    [self.outputStream open];
}

- (void)closeStreams
{
    assert( (self.inputStream != nil) == (self.outputStream != nil) );      // should either have both or neither
    if (self.inputStream != nil) {
        if (self.server)
            [self.server closeOneConnection:self];
        
        [self.inputStream removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        [self.inputStream close];
        self.inputStream = nil;
        
        [self.outputStream removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        [self.outputStream close];
        self.outputStream = nil;
    }
    self.streamOpenCount = 0;
}

- (void)sendObjectPayload:(NSObject *)object forType: (HIDPayloadType)type forDeviceID: (uint64_t)deviceID
{
    CFDataRef data = nil;
    
    if ( object ) {
        CFWriteStreamRef stream = CFWriteStreamCreateWithAllocatedBuffers(kCFAllocatorDefault, kCFAllocatorDefault);
        if (stream) {
            if (CFWriteStreamOpen(stream)) {
                CFPropertyListWriteToStream((__bridge CFPropertyListRef)(object), stream, kCFPropertyListBinaryFormat_v1_0, NULL);
                CFWriteStreamClose(stream);
                
                data = CFWriteStreamCopyProperty(stream, kCFStreamPropertyDataWritten);
            }
            CFRelease(stream);
        }
    }
    
    if ( data ) {
        [self sendGenericPayloadBytes:CFDataGetBytePtr(data) maxLength:CFDataGetLength(data) forType:type forDeviceID:deviceID];
        CFRelease(data);
    }
    
    
}

- (void)sendGenericPayloadBytes:(const uint8_t *)data maxLength:(NSUInteger)length forType:(HIDPayloadType)type forDeviceID: (uint64_t)deviceID
{
    NSUInteger maxLength = length+sizeof(HIDPayloadHeader);
    
    switch ( type ) {
        case kHIDPayloadTypeMatching:
        case kHIDPayloadTypeEnumeration:
        case kHIDPayloadTypeTermination:
            maxLength += sizeof(HIDPayloadGeneric);
            break;
        default:
            break;
    }
    
    uint8_t * payload = malloc(maxLength);
    if ( payload ) {
        ((HIDPayload*)payload)->header.type = type;
        ((HIDPayload*)payload)->header.deviceID = deviceID;
        
        if ( data ) {
            ((HIDPayload*)payload)->payload.generic.length = (uint32_t)length;
            bcopy(data, ((HIDPayload*)payload)->payload.generic.data, length);
        }
        
        [self send:payload maxLength:maxLength];
        
        free(payload);
    }
}

-(void)sendReportBytes:(uint8_t*)report maxLength:(NSUInteger)reportLength forReportType:(IOHIDReportType)reportType forReportID: (uint8_t)reportID forDeviceID:(uintptr_t)deviceID
{
    NSUInteger maxLength = reportLength+sizeof(HIDPayloadHeader)+sizeof(HIDPayloadReport);
    
    uint8_t * payload = malloc(maxLength);
    if ( payload ) {
        ((HIDPayload*)payload)->header.type                 = kHIDPayloadTypeReport;
        ((HIDPayload*)payload)->header.deviceID             = deviceID;
    
        ((HIDPayload*)payload)->payload.report.reportID     = reportID;
        ((HIDPayload*)payload)->payload.report.reportType   = reportType;
        
        ((HIDPayload*)payload)->payload.report.length = (uint32_t)reportLength;
        bcopy(report, ((HIDPayload*)payload)->payload.report.data, reportLength);
        
        [self send:payload maxLength:maxLength];
        
        free(payload);
    }
}


- (void)send:(const uint8_t *)data maxLength:(NSUInteger)length
{
    assert(self.streamOpenCount == 2);
    
    // Only write to the stream if it has space available, otherwise we might block.
    // In a real app you have to handle this case properly but in this sample code it's
    // OK to ignore it; if the stream stops transferring data the user is going to have
    // to tap a lot before we fill up our stream buffer (-:
    
    if ( [self.outputStream hasSpaceAvailable] ) {
        [self.outputStream write:data maxLength:length];
    }
}

- (void) connectToService:(NSNetService *)service
{
    BOOL                success;
    NSInputStream *     inStream;
    NSOutputStream *    outStream;
    
    assert(service != nil);
    
    if ( service!=self.lastService ) {
    
        // Create and open streams for the service.
        //
        // -getInputStream:outputStream: just creates the streams, it doesn't hit the
        // network, and thus it shouldn't fail under normal circumstances (in fact, its
        // CFNetService equivalent, CFStreamCreatePairWithSocketToNetService, returns no status
        // at all).  So, I didn't spend too much time worrying about the error case here.  If
        // we do get an error, you end up staying in the picker.  OTOH, actual connection errors
        // get handled via the NSStreamEventErrorOccurred event.
        
        success = [service getInputStream:&inStream outputStream:&outStream];
        if ( ! success ) {
            //[self setupForNewGame];
        } else if (self.inputStream != inStream && self.outputStream != outStream){
            
            
            [self closeStreams];
            
            self.inputStream  = inStream;
            self.outputStream = outStream;
            
            [self openStreams];
            
            self.lastService = service;
        }
    }
}

#pragma mark - QServer delegate

- (void)serverDidStart:(QServer *)server
{
#pragma unused(server)
    // start IOHIDManager
    
}

- (id)server:(QServer *)server connectionForInputStream:(NSInputStream *)inputStream outputStream:(NSOutputStream *)outputStream
{
    id  result;
    
    assert(server == self.server);
#pragma unused(server)
    assert(inputStream != nil);
    assert(outputStream != nil);
    
    assert( (self.inputStream != nil) == (self.outputStream != nil) );      // should either have both or neither
    
    if (self.inputStream != nil) {
        // We already have a game in place; reject this new one.
        result = nil;
    } else {
        // Start up the new game.  Start by deregistering the server, to discourage
        // other folks from connecting to us (and being disappointed when we reject
        // the connection).
        
        //[self.server deregister];
        
        // Latch the input and output sterams and kick off an open.
        [self closeStreams];
        
        self.inputStream  = inputStream;
        self.outputStream = outputStream;
        
        [self openStreams];
        
        // This is kinda bogus.  Because we only support a single input stream
        // we use the app delegate as the connection object.  It makes sense if
        // you think about it long enough, but it's definitely strange.
        
        result = self;
    }
    
    return result;
}

- (void)server:(QServer *)server didStopWithError:(NSError *)error
// This is called when the server stops of its own accord.  The only reason
// that might happen is if the Bonjour registration fails when we reregister
// the server, and that's hard to trigger because we use auto-rename.  I've
// left an assert here so that, if this does happen, we can figure out why it
// happens and then decide how best to handle it.
{
    NSLog(@"%s: Error: %@", __PRETTY_FUNCTION__, error);
    assert(server == self.server);
#pragma unused(server)
#pragma unused(error)
    assert(NO);
}

- (void)server:(QServer *)server closeConnection:(id)connection
{
    // This is called when the server shuts down, which currently never happens.
    assert(server == self.server);
#pragma unused(server)
#pragma unused(connection)
    assert(NO);
}

#pragma mark * Browser view callbacks

- (void)sortAndReloadTable
{
    // Sort the services by name.
    if ( [self.services count] ) {
        
        [self.services sortUsingComparator:^NSComparisonResult(id obj1, id obj2) {
            return [[obj1 name] localizedCaseInsensitiveCompare:[obj2 name]];
        }];
        
        [self connectToService:[self.services objectAtIndex:0]];
    }
    
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser didRemoveService:(NSNetService *)service moreComing:(BOOL)moreComing
{
    assert(browser == self.browser);
#pragma unused(browser)
    assert(service != nil);
    
    // Remove the service from our array (assume it's there, of course).
    
    if ( (self.localService == nil) || ! [self.localService isEqual:service] ) {
        [self.services removeObject:service];
    }
    
    // Only update the UI once we get the no-more-coming indication.
    
    if ( ! moreComing ) {
        [self sortAndReloadTable];
    }
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser didFindService:(NSNetService *)service moreComing:(BOOL)moreComing
{
    assert(browser == self.browser);
#pragma unused(browser)
    assert(service != nil);
    
    // Add the service to our array (unless its our own service).
    
    if ( (self.localService == nil) || ! [self.localService isEqual:service] ) {
        [self.services addObject:service];
    }
    
    // Only update the UI once we get the no-more-coming indication.
    
    if ( ! moreComing ) {
        [self sortAndReloadTable];
    }
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser didNotSearch:(NSDictionary *)errorDict
{
    assert(browser == self.browser);
#pragma unused(browser)
    assert(errorDict != nil);
#pragma unused(errorDict)
    assert(NO);         // The usual reason for us not searching is a programming error.
}

@end
