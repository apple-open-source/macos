#import <Foundation/Foundation.h>
#import <CFNetwork/CFNetwork.h>
#import <CFNetwork/CFNetworkPriv.h>

#import <sys/socket.h>
#import <arpa/inet.h>

#import "MockServer.h"

const uint16_t kHTTPServerPortAny = 0;
const int32_t kHTTPServerTimeoutSeconds = 5 * 60;


@interface HTTPServer ()
@property dispatch_queue_t queue;
@property dispatch_semaphore_t invalidationSem;
@property (assign) _CFHTTPServerRef server;
@property NSMutableDictionary *connections;
@property NSString *serviceName;
@property int cachedPort;
@end

@interface HTTPConnection ()
@property (assign) _CFHTTPServerConnectionRef connection;
@property dispatch_queue_t queue;
@property (weak) HTTPServer *server;
@property NSMutableDictionary *requests;
@property NSMutableDictionary *responses;
@property dispatch_semaphore_t invalidationSem;
@property NSString *peerAddress;
    

- (void)start;
- (void)stop:(BOOL)wait;

@end

@interface HTTPRequest ()
@property (nonatomic, readonly) _CFHTTPServerRequestRef _request;
- (id)initWithRequest:(_CFHTTPServerRequestRef)request;
@end

@interface NILServer: NSObject <HTTPServerProtocol>
@end
@implementation NILServer
- (HTTPConnection *)newConnection:(_CFHTTPServerConnectionRef)connection server:(HTTPServer *)server {
    return nil;
}
@end

@implementation HTTPServer

- (instancetype)initWithName:(NSString *)serviceName type:(NSString *)serviceType port:(uint16_t)servicePort {
    if ((self = [super init]) == nil) {
        return nil;
    }
    
    _CFHTTPServerClient serverClient = {
        _kCFHTTPServerClientCurrentVersion, // version
        (__bridge void*) self,              // info
        NULL,                               // retain
        NULL,                               // release
        NULL                                // copyDescription
    };
    
    _CFHTTPServerCallbacks callbacks = {
        _kCFHTTPServerCallbacksCurrentVersion,
        _serverDidBecomeInvalid,
        _serverDidReceiveError,
        _serverDidOpenConnection,
        _serverDidCloseConnection,
    };
    
    const char *serviceNameCStr = [serviceName cStringUsingEncoding:NSUTF8StringEncoding];
    self.serviceName = (NSString *) CFBridgingRelease(CFStringCreateWithCString(kCFAllocatorDefault, serviceNameCStr, kCFStringEncodingUTF8));
    CFStringRef st = CFStringCreateWithCString(kCFAllocatorDefault, [serviceType cStringUsingEncoding:NSUTF8StringEncoding], kCFStringEncodingUTF8);
    
    self.server = _CFHTTPServerCreateService(kCFAllocatorDefault, &serverClient, &callbacks, (__bridge CFStringRef) self.serviceName, st, servicePort);
    self.queue = dispatch_queue_create(serviceNameCStr, DISPATCH_QUEUE_SERIAL);
    self.acceptConnection = [[NILServer alloc] init];
    self.connections = [[NSMutableDictionary alloc] init];
    self.invalidationSem = dispatch_semaphore_create(0);

    CFRelease(st);

    return self;
}


- (void)dealloc {
    if (_server) {
        CFRelease(_server);
    }
}

- (void)start {
    _CFHTTPServerSetDispatchQueue(_server, _queue);
}

- (void)stop:(BOOL)wait {
    [self _stop:wait];
    if (wait) {
        dispatch_semaphore_wait(self.invalidationSem, DISPATCH_TIME_FOREVER);
    }
}

- (void)_stop:(BOOL)wait {
    dispatch_async(_queue, ^{
        _CFHTTPServerInvalidate(self.server);
        for (HTTPConnection *connection in [self.connections allValues]) {
            [connection stop:wait];
        }
    });
}

- (uint16_t)port {
    if (!_cachedPort) {
        CFTypeRef portNumRef = _CFHTTPServerCopyProperty(_server, _kCFHTTPServerServicePort);
        if (portNumRef) {
            if ((CFGetTypeID(portNumRef) != CFNumberGetTypeID()) ||
                !CFNumberGetValue((CFNumberRef) portNumRef, kCFNumberIntType, &_cachedPort)) {
                _cachedPort = -1;
            }
            CFRelease(portNumRef);
        }
    }
    return _cachedPort;
}

- (void)_didBecomeInvalid {
    dispatch_semaphore_signal(self.invalidationSem);
}

- (void)_didReceiveError:(CFErrorRef)error {
}

- (void)_didOpenConnection:(_CFHTTPServerConnectionRef)conn {
    HTTPConnection *connection = [self.acceptConnection newConnection:conn server:self];
    if (connection) {
        [_connections setObject:connection forKey:[NSValue valueWithPointer:conn]];
        [connection start];
    }
}

- (void)_didCloseConnection:(_CFHTTPServerConnectionRef)conn {
    NSValue *key = [NSValue valueWithPointer:conn];
    [_connections removeObjectForKey:key];
}

#pragma mark _CFHTTPServerCallbacks

static void _serverDidBecomeInvalid(const void* arg) {
    HTTPServer *server = (__bridge HTTPServer *) arg;
    [server _didBecomeInvalid];
}

static void _serverDidReceiveError(const void* arg, CFErrorRef err) {
    HTTPServer *server = (__bridge HTTPServer *) arg;
    [server _didReceiveError:err];
}

static void _serverDidOpenConnection(const void* arg, _CFHTTPServerConnectionRef conn) {
    HTTPServer *server = (__bridge HTTPServer *) arg;
    [server _didOpenConnection:conn];
}

static void _serverDidCloseConnection(const void* arg, _CFHTTPServerConnectionRef conn) {
    HTTPServer *server = (__bridge HTTPServer *) arg;
    [server _didCloseConnection:conn];
}


@end


@implementation HTTPConnection

- (instancetype)initWithConnectionRef:(_CFHTTPServerConnectionRef)connection server:(HTTPServer *)server {
    if (self = [super init]) {
        [self _commonInitWithConnection:connection server:server];
    }
    return self;
}

- (void)dealloc {
    if (_connection)
        CFRelease(_connection);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p> remote host: %@", [self class], self, [self peerAddress]];
}

- (void)start {
    _CFHTTPServerConnectionSetDispatchQueue(_connection, _queue);
}

- (void)stop:(BOOL)wait {
    [self _stop:wait];
    if (wait)
        dispatch_semaphore_wait(_invalidationSem, DISPATCH_TIME_FOREVER);
}

- (void)enqueueResponse:(HTTPResponse *)response {
    _CFHTTPServerRequestRef req = [[response request] _request];
    CFHTTPMessageRef msg = _CFHTTPServerRequestCreateResponseMessage(req, [response code]);
    
    NSDictionary *headers = [response headers];
    [headers enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        CFHTTPMessageSetHeaderFieldValue(msg, (__bridge CFStringRef) key, (__bridge CFStringRef) obj);
    }];
    
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, [[response data] bytes], [[response data] length]);
    _CFHTTPServerResponseRef resp = _CFHTTPServerResponseCreateWithData(req, msg, data);
    CFRelease(data);
    CFRelease(msg);
    
    dispatch_async(_queue, ^ {
        [self->_responses setObject:response forKey:[NSValue valueWithPointer:resp]];
        _CFHTTPServerResponseEnqueue(resp);
        CFRelease(resp);
    });
}

- (void)requestData:(HTTPRequest *)request withCompletion:(void (^)(NSError *, NSData *))completion {
    CFIndex contentLength = (CFIndex) [[[request headers] objectForKey:@"Content-Length"] integerValue];
    _CFHTTPServerRequestRef req = [request _request];
    
    dispatch_async(_queue, ^{
        CFReadStreamRef readStream = NULL;
        CFMutableDataRef body = NULL;
        BOOL streamWasOpened = NO;
        CFErrorRef error = NULL;
        
        if (!contentLength) {
            dispatch_async(dispatch_get_global_queue(0, 0), ^{
                completion(nil, [NSData data]);
            });
            return;
        } else {
            readStream = _CFHTTPServerRequestCopyBodyStream(req);
            if (readStream) {
                body = CFDataCreateMutable(kCFAllocatorDefault, contentLength);
                if (body) {
                    streamWasOpened = CFReadStreamOpen(readStream);
                    if (streamWasOpened) {
                        CFDataSetLength(body, contentLength);
                        uint8_t *data = CFDataGetMutableBytePtr(body);
                        CFIndex availableBytes = CFDataGetLength(body);
                        while (availableBytes) {
                            CFIndex read = CFReadStreamRead(readStream, data, availableBytes);
                            if (read < 0) {
                                error = CFReadStreamCopyError(readStream);
                                break;
                            } else if (read == 0) {
                                break;
                            } else {
                                availableBytes -= read;
                                data += read;
                            }
                        }
                        if (CFReadStreamHasBytesAvailable(readStream)) {
                            CFDataSetLength(body, 0);
                        } else if (availableBytes > 0) {
                            CFDataSetLength(body, CFDataGetLength(body) - availableBytes);
                        }
                    }
                }
            }
        }
        
        if (streamWasOpened) {
            CFReadStreamClose(readStream);
        }
        
        if (readStream) {
            CFRelease(readStream);
        }
        
        NSError *objcError = (__bridge NSError *) error;
        NSData *bodyData = (__bridge NSData *) body;
        dispatch_async(dispatch_get_global_queue(0, 0), ^{
            completion(objcError, bodyData);
        });
        
        if (body) {
            CFRelease(body);
        }
        
        if (error) {
            CFRelease(error);
        }
    });
}

- (void)didBecomeInvalid {
}

- (void)didReceiveError:(NSError *)error {
}

- (void)didReceiveRequest:(HTTPRequest *)request {
}

- (void)didSendResponse:(HTTPResponse *)response forRequest:(HTTPRequest *)request {
}

- (void)didFailToSendResponse:(HTTPResponse *)response forRequest:(HTTPRequest *)request {
}

- (void)_commonInitWithConnection:(_CFHTTPServerConnectionRef)connection server:(HTTPServer *)server {
    _CFHTTPServerClient client = {
        _kCFHTTPServerClientCurrentVersion,
        (__bridge void*) self,
        _connection_retain,
        _connection_release,
        _connection_copyDescription
    };
    
    _CFHTTPServerConnectionCallbacks callbacks = {
        _kCFHTTPServerConnectionCallbacksCurrentVersion,
        _connection_didBecomeInvalid,
        _connection_didReceiveError,
        _connection_didReceiveRequest,
        _connection_didSendResponse,
        _connection_didFailToSendResponse
    };
    
    _queue = dispatch_queue_create([[[self class] description] cStringUsingEncoding:NSUTF8StringEncoding], DISPATCH_QUEUE_SERIAL);
    _connection = (_CFHTTPServerConnectionRef) CFRetain(connection);
    _server = server;
    _requests = [[NSMutableDictionary alloc] init];
    _responses = [[NSMutableDictionary alloc] init];
    _invalidationSem = dispatch_semaphore_create(0);
    _peerAddress = [self _copyPeerAddress:_connection];
    
    _CFHTTPServerConnectionSetClient(connection, &client, &callbacks);
}

- (NSString *)_copyPeerAddress:(_CFHTTPServerConnectionRef)connection {
    NSString *result;
    struct sockaddr sa;
    CFDataRef addressData = _CFHTTPServerConnectionCopyProperty(connection, _kCFHTTPServerConnectionPeer);
    
    if (!addressData) {
        result = @"<not connected>";
    } else {
        const void *addressDataPtr = CFDataGetBytePtr(addressData);
        memmove(&sa, addressDataPtr, sizeof(struct sockaddr));
        CFRelease(addressData);
        
        if (sa.sa_family == AF_INET) {
            char str[INET_ADDRSTRLEN];
            struct sockaddr_in* sin = (struct sockaddr_in*) addressDataPtr;
            inet_ntop(AF_INET, &sin->sin_addr, str, INET_ADDRSTRLEN);
            result = [NSString stringWithFormat:@"%s:%hu", str, ntohs(sin->sin_port)];
        } else if (sa.sa_family == AF_INET6) {
            char str[INET6_ADDRSTRLEN];
            struct sockaddr_in6* sin = (struct sockaddr_in6*) addressDataPtr;
            inet_ntop(AF_INET6, &sin->sin6_addr, str, INET6_ADDRSTRLEN);
            result = [NSString stringWithFormat:@"[%s]:%hu", str, ntohs(sin->sin6_port)];
        } else {
            result = [NSString stringWithFormat:@"Unknown AF %d", (int) sa.sa_family];
        }
    }
    
    return result;
}

- (void)_stop:(BOOL)wait {
    dispatch_async(_queue, ^{
        _CFHTTPServerConnectionInvalidate(self.connection);
    });
}

- (void)_didBecomeInvalid {
    [self didBecomeInvalid];
    dispatch_semaphore_signal(_invalidationSem);
}

- (void)_didReceiveError:(NSError *)error {
    [self didReceiveError:error];
}

- (void)_didReceiveRequest:(_CFHTTPServerRequestRef)req {
    HTTPRequest *request = [[HTTPRequest alloc] initWithRequest:req];
    [_requests setObject:request forKey:[NSValue valueWithPointer:req]];
    [self didReceiveRequest:request];
}

- (void)_didSendResponse:(_CFHTTPServerResponseRef)resp forRequest:(_CFHTTPServerRequestRef)req {
    NSValue *requestKey = [NSValue valueWithPointer:req];
    HTTPRequest *request = [_requests objectForKey:requestKey];
    NSValue *responseKey = [NSValue valueWithPointer:resp];
    HTTPResponse *response = [_responses objectForKey:responseKey];

    [self didSendResponse:response forRequest:request];

    [_requests removeObjectForKey:requestKey];
    [_responses removeObjectForKey:responseKey];
}

- (void)_didFailToSendResponse:(_CFHTTPServerResponseRef)resp forRequest:(_CFHTTPServerRequestRef)req {

    NSValue *requestKey = [NSValue valueWithPointer:req];
    HTTPRequest *request = [_requests objectForKey:requestKey];
    NSValue *responseKey = [NSValue valueWithPointer:resp];
    HTTPResponse *response = [_responses objectForKey:responseKey];

    [self didFailToSendResponse:response forRequest:request];

    [_requests removeObjectForKey:requestKey];
    [_responses removeObjectForKey:responseKey];
}

#pragma mark Lifecyle

static const void* _connection_retain(const void* p) {
    if (p) {
        CFRetain(p);
    }
    
    return p;
}

static void _connection_release(const void* p) {
    if (p){
        CFRelease(p);
    }
}

static CFStringRef _connection_copyDescription(const void* p) {
    HTTPConnection *connection = (__bridge HTTPConnection *) p;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), connection);
}

#pragma mark _CFHTTPServerConnectionCallbacks

static void _connection_didBecomeInvalid(const void* arg) {
    HTTPConnection *connection = (__bridge HTTPConnection *) arg;
    [connection _didBecomeInvalid];
}

static void _connection_didReceiveError(const void* arg, CFErrorRef err) {
    HTTPConnection *connection = (__bridge HTTPConnection *) arg;
    [connection _didReceiveError:(__bridge NSError *)err];
}

static void _connection_didReceiveRequest(const void* arg, _CFHTTPServerRequestRef req) {
    HTTPConnection *connection = (__bridge HTTPConnection *) arg;
    [connection _didReceiveRequest:req];
}

static void _connection_didSendResponse(const void* arg, _CFHTTPServerRequestRef req, _CFHTTPServerResponseRef response) {
    HTTPConnection *connection = (__bridge HTTPConnection *) arg;
    [connection _didSendResponse:response forRequest:req];
}

static void _connection_didFailToSendResponse(const void* arg, _CFHTTPServerRequestRef req, _CFHTTPServerResponseRef response) {
    HTTPConnection *connection = (__bridge HTTPConnection *) arg;
    [connection _didFailToSendResponse:response forRequest:req];
}

@end


@implementation HTTPRequest {
    NSDictionary *_properties;
    _CFHTTPServerRequestRef _request;
    NSMutableDictionary *_parsedCookies;
}

- (id)initWithRequest:(_CFHTTPServerRequestRef)request {
    if (self = [super init]) {
        [self _commonInit:request];
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"HTTPRequest: %@ %@ %@", [self method], [self path], [self httpVersion]];
}

- (NSURL *)URL {
    return [_properties objectForKey:(NSString *) _kCFHTTPServerRequestURL];
}

- (NSString *)httpVersion {
    return [_properties objectForKey:(NSString *) _kCFHTTPServerRequestHTTPVersion];
}

- (NSString *)method {
    return [_properties objectForKey:(NSString *) _kCFHTTPServerRequestMethod];
}

- (NSString *)path {
    return [_properties objectForKey:(NSString *) _kCFHTTPServerRequestPath];
}

- (_CFHTTPServerRequestRef)_request {
    return _request;
}

- (NSDictionary *)headers {
    NSDictionary *headerInfo = [_properties objectForKey:(NSString *) _kCFHTTPServerRequestHeaders];
    return [headerInfo objectForKey:(NSString *) _kCFHTTPServerRequestHeaderValuesKey];
}

- (NSMutableDictionary *)cookies {
    if (!_parsedCookies) {
        _parsedCookies = [NSMutableDictionary dictionary];
        NSString *cookies = [[self headers] objectForKey:@"Cookie"];
        if (cookies) {
            NSScanner *outScanner = [NSScanner scannerWithString:cookies];
            NSCharacterSet *semicolonSet = [NSCharacterSet characterSetWithCharactersInString:@";"];
            NSCharacterSet *equalSet = [NSCharacterSet characterSetWithCharactersInString:@"="];
            while ([outScanner isAtEnd] == NO) {
                NSString *cookie, *name, *value;
                if ([outScanner scanUpToCharactersFromSet:semicolonSet intoString:&cookie]) {
                    NSScanner *inScanner = [NSScanner scannerWithString:cookie];
                    if ([inScanner scanUpToCharactersFromSet:equalSet intoString:&name] &&
                        [inScanner scanString:@"=" intoString:NULL]) {
                        value = [cookie substringFromIndex:[inScanner scanLocation]];
                        [_parsedCookies setObject:value forKey:name];
                    }
                    [outScanner scanString:@";" intoString:NULL];
                }
            }
        }
    }
    return _parsedCookies;
}

#pragma mark Internal

- (void)_commonInit:(_CFHTTPServerRequestRef)request {
    static dispatch_once_t __onceToken;
    static NSArray *__requestProperties;
    dispatch_once(&__onceToken, ^{
        __requestProperties = [[NSArray alloc] initWithObjects:(NSString *) _kCFHTTPServerRequestHTTPVersion, (NSString *) _kCFHTTPServerRequestMethod, (NSString *) _kCFHTTPServerRequestURL, (NSString *) _kCFHTTPServerRequestPath, (NSString *) _kCFHTTPServerRequestHeaders, nil];
    });
    
    NSMutableDictionary *requestProperties = [[NSMutableDictionary alloc] init];
    for (NSString *propertyKey in __requestProperties) {
        CFTypeRef propertyValue = _CFHTTPServerRequestCopyProperty(request, (__bridge CFStringRef) propertyKey);
        if (propertyValue) {
            [requestProperties setObject:(__bridge id _Nonnull)(propertyValue) forKey:propertyKey];
            CFRelease(propertyValue);
        }
    }
    _properties = requestProperties;
    _request = (_CFHTTPServerRequestRef) CFRetain(request);
}

@end

@implementation HTTPResponse {
    HTTPRequest *_request;
    NSUInteger _code;
    NSData *_data;
    NSMutableDictionary *_headers;
}

- (id)initWithRequest:(HTTPRequest *)request code:(NSUInteger)code data:(NSData *)data {
    if (self = [super init]) {
        _request = request;
        _code = code;
        _data = data;
        _headers = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"HTTPResponse: %lu (%lu bytes)", (unsigned long) _code, (unsigned long) [_data length]];
}

- (void)addHeaderValue:(NSString *)value forKey:(NSString *)key {
    [_headers setObject:value forKey:key];
}

- (NSDictionary *)headers {
    return _headers;
}

- (HTTPRequest *)request {
    return _request;
}

- (NSUInteger)code {
    return _code;
}

- (NSData *)data {
    return _data;
}

- (BOOL)hasData {
    return !!_data;
}

@end
