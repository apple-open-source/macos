//
//  MCDeviceDiscovery.m
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#import <TargetConditionals.h>
#import "MCDeviceDiscovery.h"

#import <SystemConfiguration/SCNetworkConfigurationPrivate.h>
#import <SystemConfiguration/SCDynamicStore.h>
#import <SystemConfiguration/SCDynamicStoreKey.h>
#import <SystemConfiguration/SCSchemaDefinitions.h>

#import <net/if.h>
#import <netdb.h>
#import <sys/ioctl.h>
#import <netinet/in.h>

char * const kHandshakingMulticastPort      = "5123";
// all nodes in link multicast address
char * const kHandshakingMulticastAddress   = "ff02::1";
// Prefix used in link local IP addresses
NSString * const kHandshakeMessagePrefix    = @"hrhs";

@interface MCDeviceDiscovery ()
{
}

// Queue for asynchronous socket sending and receiving
@property (nonatomic,strong) dispatch_queue_t handshakeWorkQueue;

// Timer to periodically send a multicast message
@property (nonatomic,strong) dispatch_source_t multicastSendTimer;

// name of local active ethernet interface (ex. @"en1")
@property (nonatomic,strong) NSString *localInterfaceName;
// IPv4 IP assigned to localInterfaceName. This is expected to be a link local address (169.254.X.X).
@property (nonatomic,strong) NSString *localIP;
// IPv4 IP assigned to remote  host. This is expected to be a link local address (169.254.X.X).
@property (nonatomic,strong) NSString *remoteIP;

// IPv6 address used for multicasting during handshaking algorithm
@property (nonatomic, assign) struct addrinfo *multicastAddr;
// IPv6 socket used for sending multicast during handshaking
@property (nonatomic, assign) int sendSocketFd;
// IPv6 socket used for receiving multicast during handshaking
@property (nonatomic, assign) int receiveSocketFd;
// Dispatch source used for asyunchronous reading
@property (nonatomic, strong) dispatch_source_t readSource;

// YES when local host knows the remoteIP and the remote host knows the localP
@property (nonatomic, assign) BOOL didCompleteHandshake;

// Block to call when finished performing handshake
@property (nonatomic, strong) MCDeviceHandshakeCompletion handshakeCompletionBlock;

@end

@implementation MCDeviceDiscovery

- (id)initWithInterface:(NSString *)interface
{
    self = [super init];
    if (self != nil) {
        self.localInterfaceName = interface;
    }
    return self;
}

#pragma mark Local Interface Info

+ (NSString *)localEthernetInterfaceName
{
    NSString * result = nil;
    SCDynamicStoreRef storeRef = SCDynamicStoreCreate(NULL,(CFStringRef)@"FindActiveEthernetInterface", NULL, NULL);

    NSString *globalPath = (__bridge_transfer NSString*)SCDynamicStoreKeyCreateNetworkGlobalEntity(kCFAllocatorDefault, kSCDynamicStoreDomainSetup, kSCEntNetIPv4);
    // equivalent to SCDynamicStoreCopyValue(storeRef, CFSTR("Setup:/Network/Global/IPv4")) but without assuming the schema structure
    NSDictionary *global = (__bridge_transfer NSDictionary*)SCDynamicStoreCopyValue(storeRef, (__bridge CFStringRef)globalPath);
    
    /* Expect global to contain something like
     ServiceOrder =     (
     "3E2652AC-CD50-4E4B-AA9B-872CB8E0E5D3",
     "CCC216BF-393A-41FE-87DB-A36E65C59A10",
     "041A0098-0BF4-4F0B-9C2D-39E034B807E6"
     );
     */
    //NSLog(@"serviceOrderList %@",global);
    NSArray *serviceOrderList = global[(__bridge NSString*)kSCPropNetServiceOrder];
    
    for (NSString *serviceID in serviceOrderList) {
        //NSLog(@"serviceID: %@", serviceID);
        // equivalent to CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Setup:/Network/Service/%@/Interface"), serviceID);
        NSString* path = (__bridge_transfer NSString*)SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault, kSCDynamicStoreDomainSetup, (__bridge CFStringRef)serviceID, kSCEntNetInterface);
        NSDictionary *serviceInterface = (__bridge_transfer NSDictionary *)SCDynamicStoreCopyValue(storeRef, (__bridge CFStringRef)path);
        /* Expect serviceInterface to contain something like
         DeviceName = en0;
         Hardware = AirPort;
         Type = Ethernet;
         UserDefinedName = "Wi-Fi";
         */
        //NSLog(@"serviceInterface %@",serviceInterface);
        NSString *interfaceName = serviceInterface[(__bridge NSString*)kSCPropNetInterfaceDeviceName];
        NSString *hardware = serviceInterface[(__bridge NSString*)kSCPropNetInterfaceHardware];
        NSString *type = serviceInterface[(__bridge NSString*)kSCPropNetInterfaceType];
        //NSString *userDefinedName = serviceInterface[(__bridge NSString*)kSCPropUserDefinedName];
        if ([interfaceName hasPrefix:@"en"] &&
            [hardware isEqualToString:(__bridge NSString*)kSCValNetInterfaceTypeEthernet] &&
            [type isEqualToString:(__bridge NSString*)kSCValNetInterfaceTypeEthernet]) {
            
            SCNetworkInterfaceRef interface = _SCNetworkInterfaceCreateWithBSDName(kCFAllocatorDefault, (__bridge CFStringRef)(interfaceName), 0);
            BOOL isActive = NO;
            // *** RY: TODO: Figure out better way to restrict or handle more interfaces
            if ( interface ) {
                NSLog(@"Found and Ethernet interface %@",serviceInterface);
                if (!_SCNetworkInterfaceIsThunderbolt(interface) &&
                    !_SCNetworkInterfaceIsTethered(interface) &&
                    !_SCNetworkInterfaceIsBuiltin(interface)) {
            
                    NSLog(@"Found and Ethernet interface %@",serviceInterface);
                    // Equivalent to CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("State:/Network/Interface/%@/Link"), interfaceName);
                    NSString *linkPath = (__bridge_transfer NSString*)SCDynamicStoreKeyCreateNetworkInterfaceEntity(kCFAllocatorDefault, kSCDynamicStoreDomainState, (__bridge CFStringRef)interfaceName, kSCEntNetLink);
                    NSDictionary *linkDict = (__bridge_transfer NSDictionary*)SCDynamicStoreCopyValue(storeRef, (__bridge CFStringRef)linkPath);
                    isActive = [linkDict[(__bridge NSString*)kSCPropNetLinkActive] boolValue];
                } else {
                    NSLog(@"Sadly said interface was not suitable");
                }
                CFRelease(interface);
            }
            
            NSLog(@"Active = %@",isActive ? @"YES" : @"NO");
            if (isActive) {
                result = interfaceName;
                break;
            }
        }
    }
    return result;
}

+ (NSString *)ipv4AddressForInterface:(NSString *)interfaceName
{
    if (!interfaceName || ![interfaceName UTF8String]) return nil;
    
    /* determine UDN according to MAC address */
    int sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        NSLog(@"Error creating socket to return IP");
        return NULL;
    }
    
    struct ifreq ifr;
    strcpy(ifr.ifr_name, [interfaceName UTF8String]);
    ifr.ifr_addr.sa_family = AF_INET;
    
    if (ioctl (sock, SIOCGIFADDR, &ifr) < 0)
    {
        NSLog(@"Error calling ioctl to return IP");
        close (sock);
        return NULL;
    }
    
    uint32_t ip = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr;
    ip = ntohl (ip);
    NSString *result = [NSString stringWithFormat:@"%d.%d.%d.%d",(ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF];
    
    close (sock);
    
    return result;
    
}

#pragma mark Handshaking algorithm

// Makes sure we have an active ethernet interace with a link local IP address
- (BOOL)discoverLocalInterface
{
    //self.localInterfaceName = nil;
    self.localIP = nil;
    
    self.localInterfaceName = self.localInterfaceName ? self.localInterfaceName : [MCDeviceDiscovery localEthernetInterfaceName];
    if (!self.localInterfaceName) {
        NSLog(@"%s: couldn't find a local active ethernet interface", __PRETTY_FUNCTION__);
        return NO;
    }
    self.localIP = [MCDeviceDiscovery ipv4AddressForInterface:self.localInterfaceName];
    if (!self.localIP) {
        NSLog(@"%s: local active ethernet interface %@ doesn't have an IP", __PRETTY_FUNCTION__, self.localInterfaceName);
        return NO;
    }
    // We don't know the remote IP yet
    self.remoteIP = @"NONE";

    return YES;
}

// Initializes multicastAddr and sendSocketFd state
- (BOOL)setUpMulticastSendSocket
{
    NSLog(@"%s: setting up multicast send socket", __PRETTY_FUNCTION__);

    if (!self.localInterfaceName || !self.localIP) {
        NSLog(@"%s: must have a local ethernet interface and IP to initialize multicast sending", __PRETTY_FUNCTION__);
        return NO;
    }
    
    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_DGRAM;
    
    int err_num;
    err_num = getaddrinfo(kHandshakingMulticastAddress, kHandshakingMulticastPort, &hints, &_multicastAddr);
    if(err_num) {
        NSLog(@"Error: %s calling getaddrinfo: %s", __PRETTY_FUNCTION__, gai_strerror(err_num));
        return NO;
    }

    _sendSocketFd = socket(_multicastAddr->ai_family, _multicastAddr->ai_socktype, _multicastAddr->ai_protocol);
    if (_sendSocketFd < 0) {
        NSLog(@"Error: %s creating socket: %s", __PRETTY_FUNCTION__, gai_strerror(err_num));
        return NO;
    }
    
    unsigned int ifindex;
    ifindex = if_nametoindex([self.localInterfaceName UTF8String]);
    if (setsockopt(_sendSocketFd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex,sizeof(ifindex)) !=0) {
        NSLog(@"Error: %s setsockopt/IPV6_MULTICAST_IF: %s", __PRETTY_FUNCTION__, gai_strerror(err_num));
        return NO;
    }
    
    return YES;
}

// initializes receiveSocketFd and joins multicast group for receiving
// must be called after setUpMulticastSendSocket because it uses multicastAddr
- (BOOL)setUpMulticastReceiveSocket
{
    NSLog(@"%s: setting up multicast receive socket", __PRETTY_FUNCTION__);
    
    if (!self.localInterfaceName || !self.localIP) {
        NSLog(@"%s: must have a local ethernet interface and IP to initialize multicast receiving", __PRETTY_FUNCTION__);
        return NO;
    }
    
    if (!_multicastAddr) {
        NSLog(@"%s: multicastAddr hasn't been initialized", __PRETTY_FUNCTION__);
        return NO;
    }

    // Get local address to intialize receiveSocketFd
    struct addrinfo hints;
    struct addrinfo *localAddr;
    bzero(&hints, sizeof(hints));
    hints.ai_family   = _multicastAddr->ai_family;
    hints.ai_socktype = _multicastAddr->ai_socktype;
    hints.ai_flags    = AI_PASSIVE;
    int err_num;
    err_num = getaddrinfo(NULL, kHandshakingMulticastPort, &hints, &localAddr);
    if(err_num) {
        NSLog(@"Error: %s calling getaddrinfo: %s", __PRETTY_FUNCTION__, gai_strerror(err_num));
        return NO;
    }
    
    _receiveSocketFd = socket(localAddr->ai_family, localAddr->ai_socktype, localAddr->ai_protocol);
    if (_receiveSocketFd < 0) {
        NSLog(@"Error: %s creating socket: %s", __PRETTY_FUNCTION__, gai_strerror(err_num));
        return NO;
    }
    
    if (bind(_receiveSocketFd, localAddr->ai_addr, localAddr->ai_addrlen) < 0) {
        NSLog(@"Error: %s binding socket: %s", __PRETTY_FUNCTION__, gai_strerror(err_num));
        return NO;
    }
    freeaddrinfo(localAddr);
    
    // set to non-blocking
    int flags;
    flags = fcntl(_receiveSocketFd,F_GETFL);
    if (fcntl(_receiveSocketFd, F_SETFL, flags | O_NONBLOCK) == -1){
        NSLog(@"Error: %s setting socket to non blocking: %s", __PRETTY_FUNCTION__, gai_strerror(err_num));
        return NO;
    }
    
    // Join the multicast group
    struct ipv6_mreq multicastRequest;
    const char *interfaceName = [self.localInterfaceName UTF8String];
    multicastRequest.ipv6mr_interface = if_nametoindex(interfaceName);
    multicastRequest.ipv6mr_multiaddr = ((struct sockaddr_in6*)(_multicastAddr->ai_addr))->sin6_addr;
    
    if ( setsockopt(_receiveSocketFd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*) &multicastRequest, sizeof(multicastRequest)) != 0 )
    {
        NSLog(@"Error: %s joining multicast group: %s", __PRETTY_FUNCTION__, gai_strerror(err_num));
        return NO;
    }
    
    return YES;
}

- (BOOL)setUpReceiveDispatchSource
{
    NSLog(@"%s: setting up asynchronous reading via dispatch_source", __PRETTY_FUNCTION__);
    
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    self.readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, _receiveSocketFd, 0, queue);
    if (!self.readSource)
    {
        NSLog(@"Error: %s setting up dispatch source", __PRETTY_FUNCTION__);
        return NO;
    }
    
    // Install the event handler
    dispatch_source_set_event_handler(self.readSource, ^{
        if (dispatch_source_get_data(self.readSource) > 0) {
            [self receiveHandshakeMulticast];
        }
    });
    
    // Install the cancellation handler
    dispatch_source_set_cancel_handler(self.readSource, ^{
        NSLog(@"%s: canceling read source", __PRETTY_FUNCTION__);
    });
    
    // Start reading the file.
    dispatch_resume(self.readSource);
    
    return YES;
}

- (void)tearDownMulticastSendReceiveState
{
    if ( self.multicastSendTimer ) {
        dispatch_source_cancel(self.multicastSendTimer);
        self.multicastSendTimer = nil;
    }

    if ( _sendSocketFd ) {
        close (_sendSocketFd);
        _sendSocketFd = 0;
    }
    
    if ( _receiveSocketFd ) {
        close (_receiveSocketFd);
        _receiveSocketFd = 0;
    }
    
    if ( _multicastAddr ) {
        freeaddrinfo(_multicastAddr);
        _multicastAddr = nil;
    }
    
    if ( self.readSource ) {
        dispatch_source_cancel(self.readSource);
        self.readSource = nil;
    }
    
}

// Sends a multicast message string composed of the local IP plus the known remote IP
- (BOOL)sendHandshakeMulticast
{
    if (!_sendSocketFd) return NO;
    
    NSString *messageString = [NSString stringWithFormat:@"%@:%@:%@", kHandshakeMessagePrefix, self.localIP, self.remoteIP];
    
    //NSLog(@"%s: sending %@", __PRETTY_FUNCTION__,messageString);
    
    const char *msg = [messageString UTF8String];
    if (sendto(_sendSocketFd, msg, strlen(msg), 0, _multicastAddr->ai_addr, _multicastAddr->ai_addrlen) < 0) {
        NSLog(@"%s: sendto failed %s", __PRETTY_FUNCTION__,gai_strerror(errno));
        return NO;
    }
    return YES;
}

- (BOOL)receiveHandshakeMulticast
{
    const int kBufferSize = 256;
    char msgbuf[kBufferSize];
    bzero(&msgbuf, kBufferSize);
    struct sockaddr_in6 rcv_addr;
    int rcv_addr_len=sizeof(rcv_addr);
    
    if ((recvfrom(_receiveSocketFd, msgbuf, kBufferSize, 0,(struct sockaddr *) &rcv_addr,(socklen_t *)&rcv_addr_len)) < 0) {
        NSLog(@"%s: recvfrom failed %s", __PRETTY_FUNCTION__,gai_strerror(errno));
        return NO;
    }

    //NSLog(@"%s message=%s", __PRETTY_FUNCTION__, msgbuf);
    
    NSString *receivedString = [NSString stringWithFormat:@"%s",msgbuf];
    if (![receivedString hasPrefix:kHandshakeMessagePrefix]) {
        NSLog(@"%s: received message with unexpected prefix %@", __PRETTY_FUNCTION__,receivedString);
        return NO;
    }
    
    NSArray *receivedComponents = [receivedString componentsSeparatedByString:@":"];
    NSUInteger componentsCount = [receivedComponents count];
    if (componentsCount != 3) {
        NSLog(@"%s: expected message with 3 components and got %@", __PRETTY_FUNCTION__,receivedComponents);
        return NO;
    }
    NSString *messageLocalIP = receivedComponents[1];
    NSString *messageRemoteIP = receivedComponents[2];
    // only process messages we didn't send
    if (![self.localIP isEqualToString:messageLocalIP]) {
        // remember the message's sender IP as the remote IP
        self.remoteIP = messageLocalIP;
        if ([self.localIP isEqualToString:messageRemoteIP]) {
            // The message sent by another host contains our own IP as a remote IP
            // This means the sender knows about us and the handshake is done
            self.didCompleteHandshake = YES;
        }
    }
    return YES;
}

- (BOOL)setUpMulticastSendTimer
{
    self.multicastSendTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, self.handshakeWorkQueue);
    if (self.multicastSendTimer)
    {
        dispatch_source_set_timer(self.multicastSendTimer, dispatch_walltime(NULL, 0), 1.0 * NSEC_PER_SEC, 0.1 * NSEC_PER_SEC);
        dispatch_source_set_event_handler(self.multicastSendTimer, ^ {
            [self sendHandshakeMulticast];
            if (self.didCompleteHandshake) {
                if (self.handshakeCompletionBlock) {
                    self.handshakeCompletionBlock(YES,self.localInterfaceName,self.remoteIP);
                }
                [self tearDownMulticastSendReceiveState];
            };
        });
        dispatch_resume(self.multicastSendTimer);
    }

    return YES;
}

- (void)performHandshakeWithCompletionBlock:(MCDeviceHandshakeCompletion)completion
{
    if ( !self.handshakeWorkQueue )
        self.handshakeWorkQueue = dispatch_queue_create("com.apple.hid.handshakeworkqueue" , DISPATCH_QUEUE_SERIAL);

    self.handshakeCompletionBlock = completion;
    BOOL didStart = NO;
    
    if ([self discoverLocalInterface]) {
        if ([self setUpMulticastSendSocket]) {
            if ([self setUpMulticastReceiveSocket]) {
                if ([self setUpReceiveDispatchSource]) {
                    didStart = [self setUpMulticastSendTimer];
                }
            }
        }
    }
    if (!didStart) {
        [self tearDownMulticastSendReceiveState];
        if (self.handshakeCompletionBlock) {
            // TODO - do a much better job at propagating an error code when an error happens.
            self.handshakeCompletionBlock(NO,nil,nil);
        }
    }
}

@end
