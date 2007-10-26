/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import "NSOpenDirectory.h"
#import <CFOpenDirectory/CFOpenDirectory.h>
#import <pthread.h>
#import <CoreFoundation/CFRuntime.h>
#import <DirectoryService/DirectoryService.h>

NSString *const ODSessionProxyAddress       = @"ProxyAddress";
NSString *const ODSessionProxyPort          = @"ProxyPort";
NSString *const ODSessionProxyUsername      = @"ProxyUsername";
NSString *const ODSessionProxyPassword      = @"ProxyPassword";
NSString *const ODFrameworkErrorDomain      = @"OpenDirectoryFramework";

extern void *_ODQueryGetDelegate( ODQueryRef inQueryRef );
extern void *_ODQueryGetPredicate( ODQueryRef inQueryRef );
extern void _ODQuerySetDelegate( ODQueryRef inQueryRef, void *inDelegate );
extern void _ODQuerySetPredicate( ODQueryRef inQueryRef, void *inPredicate );
extern Boolean _ODSessionInit( ODSessionRef inSession, CFDictionaryRef inOptions, CFErrorRef *outError );
extern Boolean _ODNodeInitWithType( ODNodeRef inNodeRef, ODSessionRef inSessionRef, ODNodeType inType, CFErrorRef *outError );
extern Boolean _ODNodeInitWithName( CFAllocatorRef inAllocator, ODNodeRef inNodeRef, ODSessionRef inDirRef, CFStringRef inNodeName, CFErrorRef *outError );
extern ODSessionRef _ODSessionGetShared( void );
extern CFDictionaryRef _ODRecordGetDictionary( ODRecordRef inRecordRef );

extern struct _ODSession *_createSession( CFAllocatorRef inAllocator );
extern struct _ODRecord *_createRecord( CFAllocatorRef allocator );
extern struct _ODNode *_createNode( CFAllocatorRef allocator );
extern struct _ODQuery *_createQuery( CFAllocatorRef inAllocator );

static void queryCallback( ODQueryRef inQuery, CFArrayRef inResults, CFErrorRef inError, void *inUserInfo )
{
    // inQuery may not really be the original query, so we use userInfo to store the original object
    NSAutoreleasePool   *pool       = [NSAutoreleasePool new];
    ODQuery             *odQuery    = (ODQuery *) inUserInfo;
    NSPredicate         *predicate  = (NSPredicate *) _ODQueryGetPredicate( inUserInfo );
    
    @try
    {
        id  delegate = [odQuery delegate];
        
        if( nil != delegate )
        {
            if( [delegate respondsToSelector:@selector(results:forQuery:error:)] == YES )
            {
                NSArray     *results = (inResults != NULL ? (NSArray *) CFArrayCreateCopy(kCFAllocatorDefault, inResults) : nil);
                NSError     *error   = (NSError *) inError;
                NSError     *newErr  = nil;
                
                if ( error != nil )
                    newErr = [NSError errorWithDomain: [error domain] code: [error code] userInfo: [error userInfo]]; // we make a copy
                
                if( nil != predicate )
                {
#warning still need to use predicate
                    // need to use predicate against results before we pass to delegate
                }
                
                [delegate results: [NSMakeCollectable(results) autorelease] forQuery: odQuery error: NSMakeCollectable(newErr)];
            }
        }
    }
    @catch( id exception )
    {
    }
    
    [pool release];
}

@interface ODSession (InternalAccess)

- (ODSessionRef) _getCFSessionObject;

@end

@interface NSODSession : ODSession
@end

@implementation ODSession (InternalAccess)

- (ODSessionRef) _getCFSessionObject
{
    if( [self class] != [NSODSession class] ) 
    {
        return (ODSessionRef) _internal;
    }
    
    return (ODSessionRef) self;
}

@end

@implementation ODSession

+ (void) initialize
{
    static int __done = 0;
    
    if (!__done)
    {
        __done = 1;
        ODSessionGetTypeID();
        ODNodeGetTypeID();
        ODQueryGetTypeID();
        ODRecordGetTypeID();
        ODContextGetTypeID();
    }
}

+ (id)sessionWithOptions:(NSDictionary *)inOptions error:(NSError **)outError
{
    ODSession *object = (ODSession *) ODSessionCreate( NULL, (CFDictionaryRef) inOptions, (CFErrorRef *) outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];
    
    return [NSMakeCollectable(object) autorelease];
}

+ (id)defaultSession
{
    return (id) _ODSessionGetShared();
}

+ (id)allocWithZone:(NSZone *)inZone
{
    if( [self class] == [ODSession class] )
        return (id) _createSession( NULL );
    else
        return [super allocWithZone: inZone];
}

- (void)dealloc
{
    if( [self class] != [NSODSession class] && NULL != _internal )
        CFRelease( (ODSessionRef) _internal );
    
    [super dealloc];
}

- (id)initWithOptions:(NSDictionary *)inOptions error:(NSError **)outError
{
    if ( NULL != outError )
        (*outError) = NULL;
    
    if( [self class] == [NSODSession class] )
    {
        if( _ODSessionInit( (ODSessionRef) self, (CFDictionaryRef) inOptions, (CFErrorRef *) outError ) == NO )
        {
            [self release];
            self = NULL;
        }
    }
    else
    {
        _internal = (void *) ODSessionCreate( NULL, (CFDictionaryRef) inOptions, (CFErrorRef *) outError );
        if( NULL == _internal )
        {
            [self release];
            self = NULL;
        }
    }

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];
    
    return NSMakeCollectable(self);
}

- (NSArray *)nodeNames:(NSError **)outError
{
    NSArray *result = nil;
    
    if( [self class] == [NSODSession class] )
    {
        result = (NSArray *) ODSessionCopyNodeNames( kCFAllocatorDefault, (ODSessionRef) self, (CFErrorRef *) outError );
    }
    else
    {
        result = (NSArray *) ODSessionCopyNodeNames( kCFAllocatorDefault, _internal, (CFErrorRef *) outError );
    }
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];
    
    return [NSMakeCollectable(result) autorelease];
}

@end

@implementation NSODSession

- (id) init
{
    return self;
}

- (BOOL)isEqual:(id)object
{
    if (nil == object) 
        return NO; 
    return (BOOL)CFEqual( (CFTypeRef)self, (CFTypeRef)object );
}

- (unsigned)hash
{ 
    return (unsigned)CFHash((CFTypeRef)self);
}

- (id)retain
{
    CFRetain((CFTypeRef)self); 
    return (id)self;
}

- (oneway void)release
{
    CFRelease((CFTypeRef)self);
}

- (unsigned)retainCount
{
    return (unsigned)CFGetRetainCount((CFTypeRef)self);
}

- (void)finalize
{
    const CFRuntimeClass* cfclass = _CFRuntimeGetClassWithTypeID(CFGetTypeID(self));
    
    if (cfclass->finalize)
        cfclass->finalize(self);
    
    [super finalize];
}

- (CFTypeID)_cfTypeID
{
    return ODSessionGetTypeID();
}

- (NSString *)description
{
    NSString *theDescription = (NSString *) CFCopyDescription( self );
    
    return [NSMakeCollectable(theDescription) autorelease];
}

@end

@interface NSODNode : ODNode
@end

@implementation ODNode

+ (void) initialize
{
    static int __done = 0;
    
    if( !__done )
    {
        __done = 1;

        ODSessionGetTypeID();
        ODNodeGetTypeID();
        ODQueryGetTypeID();
        ODRecordGetTypeID();
        ODContextGetTypeID();
    }
}

+ (id)nodeWithSession:(ODSession *)inSession type:(ODNodeType)inType error:(NSError **)outError
{
    id object   = nil;
    
    if( nil == inSession )
    {
        inSession = [ODSession defaultSession];
    }
        
    if( [inSession class] == [NSODSession class] )
        object = (id) ODNodeCreateWithNodeType( NULL, (ODSessionRef) inSession, inType, (CFErrorRef *) outError );
    else
        object = (id) ODNodeCreateWithNodeType( NULL, [inSession _getCFSessionObject], inType, (CFErrorRef *) outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(object) autorelease];
}

+ (id)nodeWithSession:(ODSession *)inSession name:(NSString *)inName error:(NSError **)outError
{
    id object   = nil;
    
    if( nil == inSession )
    {
        inSession = [ODSession defaultSession];
    }
        
    if( [inSession class] == [NSODSession class] )
        object = (id) ODNodeCreateWithName( NULL, (ODSessionRef) inSession, (CFStringRef) inName, (CFErrorRef *) outError );
    else
        object = (id) ODNodeCreateWithName( NULL, [inSession _getCFSessionObject], (CFStringRef) inName, (CFErrorRef *) outError );
        
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(object) autorelease];
}

+ (id)allocWithZone:(NSZone *)inZone
{
    if( [self class] == [ODNode class] )
        return (id) _createNode( NULL );
    else
        return [super allocWithZone: inZone];
}

- (id)init
{
    if( [self class] == [NSODNode class] )
        _ODNodeInitWithType( (ODNodeRef) self, NULL, kODTypeAuthenticationSearchNode, NULL );
    else
        _internal = (void *) ODNodeCreateWithNodeType( NULL, NULL, kODTypeAuthenticationSearchNode, NULL );
    
    return NSMakeCollectable(self);
}

- (id)initWithSession:(ODSession *)inSession type:(ODNodeType)inType error:(NSError **)outError
{
    CFErrorRef  error = NULL;
    
    if ( NULL != outError )
        (*outError) = NULL;

    if( nil == inSession )
    {
        inSession = [ODSession defaultSession];
    }
    
    if( [self class] == [NSODNode class] )
    {
        if( [inSession class] == [NSODSession class] )
            _ODNodeInitWithType( (ODNodeRef) self, (ODSessionRef) inSession, inType, &error );
        else
            _ODNodeInitWithType( (ODNodeRef) self, [inSession _getCFSessionObject], inType, &error );
    }
    else
    {
        if( [inSession class] == [NSODSession class] )
            _internal = (id) ODNodeCreateWithNodeType( NULL, (ODSessionRef) inSession, inType, &error );
        else
            _internal = (id) ODNodeCreateWithNodeType( NULL, [inSession _getCFSessionObject], inType, &error );
    }
    
    // if we had an error, let's release ourselves and fail
    if( error != NULL )
    {
        [self release];
        return nil;
    }
    
    if( NULL != outError )
    {
        *outError = (NSError *) error;
        [NSMakeCollectable(*outError) autorelease];
    }
    else if( NULL != error )
    {
        CFRelease( error );
    }
    
    return NSMakeCollectable(self);
}

- (id)initWithSession:(ODSession *)inSession name:(NSString *)inName error:(NSError **)outError
{
    CFErrorRef  error = NULL;

    if ( NULL != outError )
        (*outError) = NULL;

    if( nil == inSession )
    {
        inSession = [ODSession defaultSession];
    }
    
    if( [self class] == [NSODNode class] )
    {
        if( [inSession class] == [NSODSession class] )
            _ODNodeInitWithName( NULL, (ODNodeRef) self, (ODSessionRef) inSession, (CFStringRef) inName, &error );
        else
            _ODNodeInitWithName( NULL, (ODNodeRef) self, [inSession _getCFSessionObject], (CFStringRef) inName, &error );
    }
    else
    {
        if( [inSession class] == [NSODSession class] )
            _internal = (id) ODNodeCreateWithName( NULL, (ODSessionRef) inSession, (CFStringRef) inName, &error );
        else
            _internal = (id) ODNodeCreateWithName( NULL, [inSession _getCFSessionObject], (CFStringRef) inName, &error );
    }
    
    // if we had an error, let's release ourselves and fail
    if( error != NULL )
    {
        [self release];
        self = nil;
    }
    
    if( NULL != outError )
    {
        *outError = (NSError *)error;
        [NSMakeCollectable(*outError) autorelease];
    }
    else if( NULL != error )
    {
        CFRelease( error );
    }

    return NSMakeCollectable(self);    
}

- (void) dealloc
{
    if( [self class] != [NSODNode class] && NULL != _internal )
        CFRelease( _internal );
    
    [super dealloc];
}

- (id)copy
{
    if( [self class] == [NSODNode class] ) 
        return (id) ODNodeCreateCopy( NULL, (ODNodeRef) self, NULL );
    else
        return (id) ODNodeCreateCopy( NULL, (ODNodeRef) _internal, NULL );
}

- (NSString *)nodeName
{
    if( [self class] == [NSODNode class] ) 
        return (NSString *) ODNodeGetName( (ODNodeRef) self );
    else
        return (NSString *) ODNodeGetName( (ODNodeRef) _internal );
}

- (NSDictionary *)nodeDetailsForKeys:(NSArray *)inKeys error:(NSError **)inError
{
    NSDictionary *info = nil;
    
    if( [self class] == [NSODNode class] ) 
        info = (NSDictionary *) ODNodeCopyDetails( (ODNodeRef) self, (CFArrayRef) inKeys, (CFErrorRef *) inError );
    else
        info = (NSDictionary *) ODNodeCopyDetails( (ODNodeRef) _internal, (CFArrayRef) inKeys, (CFErrorRef *) inError );

    return [NSMakeCollectable(info) autorelease];
}

- (NSArray *)subnodeNames:(NSError **)outError
{
    NSArray *results =  nil;

    if( [self class] == [NSODNode class] ) 
        results = (NSArray *) ODNodeCopySubnodeNames( (ODNodeRef) self, (CFErrorRef *) outError );
    else
        results = (NSArray *) ODNodeCopySubnodeNames( (ODNodeRef) _internal, (CFErrorRef *) outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(results) autorelease];
}

- (NSArray *)unreachableSubnodeNames:(NSError **)outError
{
    NSArray *results;
    
    if( [self class] == [NSODNode class] )
        results = (NSArray *) ODNodeCopyUnreachableSubnodeNames( (ODNodeRef) self, (CFErrorRef *) outError );
    else
        results = (NSArray *) ODNodeCopyUnreachableSubnodeNames( (ODNodeRef) _internal, (CFErrorRef *) outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(results) autorelease];
}

- (NSArray *)supportedRecordTypes:(NSError **)outError
{
    NSArray  *types;

    if( [self class] == [NSODNode class] )
        types = (NSArray *) ODNodeCopySupportedRecordTypes( (ODNodeRef) self, (CFErrorRef *) outError );
    else
        types = (NSArray *) ODNodeCopySupportedRecordTypes( (ODNodeRef) _internal, (CFErrorRef *) outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(types) autorelease];
}

- (NSArray *)supportedAttributesForRecordType:(NSString *)inRecordType error:(NSError **)outError
{
    NSArray  *attribs;

    if( [self class] == [NSODNode class] )
        attribs = (NSArray *) ODNodeCopySupportedAttributes( (ODNodeRef) self, (CFStringRef) inRecordType, (CFErrorRef *) outError );
    else
        attribs = (NSArray *) ODNodeCopySupportedAttributes( (ODNodeRef) _internal, (CFStringRef) inRecordType, (CFErrorRef *) outError );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(attribs) autorelease];
}

- (BOOL)setCredentialsWithRecordType:(NSString *)inRecordType recordName:(NSString *)inRecordName password:(NSString *)inPassword
                               error:(NSError **)outError
{
    BOOL    bResult;
    
    if( [self class] == [NSODNode class] )
        bResult = ODNodeSetCredentials( (ODNodeRef) self, (CFStringRef) inRecordType, (CFStringRef) inRecordName, (CFStringRef) inPassword, 
                                        (CFErrorRef *) outError );
    else
        bResult = ODNodeSetCredentials( (ODNodeRef) _internal, (CFStringRef) inRecordType, (CFStringRef) inRecordName, (CFStringRef) inPassword, 
                                        (CFErrorRef *) outError );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)setCredentialsWithRecordType:(NSString *)inRecordType authenticationType:(NSString *)inType 
                 authenticationItems:(NSArray *)inItems continueItems:(NSArray **)outItems
                             context:(id *)outContext error:(NSError **)outError
{
    BOOL bResult;
    
    if( [self class] == [NSODNode class] )
        bResult = ODNodeSetCredentialsExtended( (ODNodeRef) self, (CFStringRef) inRecordType, (CFStringRef) inType,
                                                (CFArrayRef) inItems, (CFArrayRef *) outItems, (ODContextRef *) outContext, (CFErrorRef *) outError );
    else
        bResult = ODNodeSetCredentialsExtended( (ODNodeRef) _internal, (CFStringRef) inRecordType, (CFStringRef) inType,
                                                (CFArrayRef) inItems, (CFArrayRef *) outItems, (ODContextRef *) outContext, (CFErrorRef *) outError );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)setCredentialsUsingKerberosCache:(NSString *)inCacheName error:(NSError **)outError
{
    BOOL bResult;
    
    if( [self class] == [NSODNode class] )
        bResult = ODNodeSetCredentialsUsingKerberosCache( (ODNodeRef) self, (CFStringRef) inCacheName, (CFErrorRef *) outError );
    else
        bResult = ODNodeSetCredentialsUsingKerberosCache( (ODNodeRef) _internal, (CFStringRef) inCacheName, (CFErrorRef *) outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];
    
    return bResult;
}

- (ODRecord *)createRecordWithRecordType:(NSString *)inRecordType name:(NSString *)inRecordName attributes:(NSDictionary *)inAttributes
                                   error:(NSError **)outError
{
    ODRecord *record;

    if( [self class] == [NSODNode class] )
        record = (ODRecord *) ODNodeCreateRecord( (ODNodeRef) self, (CFStringRef) inRecordType, (CFStringRef) inRecordName, 
                                                  (CFDictionaryRef) inAttributes, (CFErrorRef *) outError );
    else
        record = (ODRecord *) ODNodeCreateRecord( (ODNodeRef) _internal, (CFStringRef) inRecordType, (CFStringRef) inRecordName, 
                                                  (CFDictionaryRef) inAttributes, (CFErrorRef *) outError );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(record) autorelease];
}

- (ODRecord *)recordWithRecordType:(NSString *)inRecordType name:(NSString *)inRecordName attributes:(NSArray *)inAttributes
                             error:(NSError **)outError
{
    ODRecord *record    = nil;
    
    if( [self class] == [NSODNode class] )
        record = (ODRecord *) ODNodeCopyRecord( (ODNodeRef) self, (CFStringRef) inRecordType, (CFStringRef) inRecordName, 
                                                (CFArrayRef) inAttributes, (CFErrorRef *) outError );
    else
        record = (ODRecord *) ODNodeCopyRecord( (ODNodeRef) _internal, (CFStringRef) inRecordType, (CFStringRef) inRecordName, 
                                                (CFArrayRef) inAttributes, (CFErrorRef *) outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(record) autorelease];
}

- (NSData *)customCall:(NSInteger)inCustomCode sendData:(NSData *)inSendData error:(NSError **)outError
{
    NSData  *result;
    
    if( [self class] == [NSODNode class] )
        result = (NSData *) ODNodeCustomCall( (ODNodeRef) self, (CFIndex) inCustomCode, (CFDataRef) inSendData, (CFErrorRef *) outError );
    else
        result = (NSData *) ODNodeCustomCall( (ODNodeRef) _internal, (CFIndex) inCustomCode, (CFDataRef) inSendData, (CFErrorRef *) outError );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(result) autorelease];
}

@end

@implementation NSODNode

- (id) init
{
    // nothing to do
    return self;
}

- (CFTypeID)_cfTypeID
{
    return ODNodeGetTypeID();
}

- (BOOL)isEqual:(id)object
{
    if (nil == object) 
        return NO; 
    return (BOOL)CFEqual((CFTypeRef)self, (CFTypeRef)object);
}

- (unsigned)hash
{ 
    return (unsigned)CFHash((CFTypeRef)self);
}

- (id)retain
{
    CFRetain( (CFTypeRef)self );
    return (id)self;
}

- (oneway void)release
{
    CFRelease( (CFTypeRef)self );
}

- (unsigned)retainCount
{
    return (unsigned)CFGetRetainCount( (CFTypeRef)self );
}

- (void)finalize
{
    const CFRuntimeClass* cfclass = _CFRuntimeGetClassWithTypeID( CFGetTypeID(self) );
    
    if (cfclass->finalize)
        cfclass->finalize( self );
    
    [super finalize];
}

- (NSString *)description
{
    NSString *theDescription = (NSString *) CFCopyDescription( self );
    
    return [NSMakeCollectable(theDescription) autorelease];
}

@end

@interface NSODRecord : ODRecord
@end

@implementation ODRecord

+ (id)allocWithZone:(NSZone *)inZone
{
    if( [self class] == [ODRecord class] )
        return (id) _createRecord( NULL );
    else
        return [super allocWithZone: inZone];
}

+ (BOOL)accessInstanceVariablesDirectly
{
    return NO;
}

// we don't dispatch back to back to CF here because there is no way to properly create an ODRecordRef
// without using our APIs.  So if someone poses as the class they must implement it all

- (BOOL)setNodeCredentials:(NSString *)inUsername password:(NSString *)inPassword error:(NSError **)outError
{
    BOOL bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordSetNodeCredentials( (ODRecordRef) self, (CFStringRef) inUsername, (CFStringRef) inPassword, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)setNodeCredentialsWithRecordType:(NSString *)inRecordType authenticationType:(NSString *)inType 
                     authenticationItems:(NSArray *)inItems continueItems:(NSArray **)outItems
                                 context:(id *)outContext error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordSetNodeCredentialsExtended( (ODRecordRef) self, (CFStringRef) inRecordType, 
                                                      (CFStringRef) inType, (CFArrayRef) inItems,
                                                      (CFArrayRef *) outItems, (ODContextRef *) outContext, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)setNodeCredentialsUsingKerberosCache:(NSString *)inCacheName error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordSetNodeCredentialsUsingKerberosCache( (ODRecordRef) self, (CFStringRef) inCacheName, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];
    
    return bResult;
}

- (NSDictionary *)passwordPolicy:(NSError **)outError
{
    NSDictionary *policy = nil;
    
    if( [self class] == [NSODRecord class] )
        policy = (NSDictionary *) ODRecordCopyPasswordPolicy( kCFAllocatorDefault, (ODRecordRef) self, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(policy) autorelease];
}

- (BOOL)verifyPassword:(NSString *)inPassword error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordVerifyPassword( (ODRecordRef) self, (CFStringRef) inPassword, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)verifyExtendedWithAuthenticationType:(NSString *)inType authenticationItems:(NSArray *)inItems 
                               continueItems:(NSArray **)outItems context:(id *)outContext error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordVerifyPasswordExtended( (ODRecordRef) self, (CFStringRef) inType, (CFArrayRef) inItems,
                                                  (CFArrayRef *) outItems, (ODContextRef *) outContext, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)changePassword:(NSString *)oldPassword toPassword:(NSString *)newPassword error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordChangePassword( (ODRecordRef) self, (CFStringRef) oldPassword, (CFStringRef) newPassword, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)synchronize:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        return ODRecordSynchronize( (ODRecordRef) self, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (NSDictionary *)recordDetailsForAttributes:(NSArray *)inValues error:(NSError **)outError
{
    NSDictionary *attribs = nil;
    
    if( [self class] == [NSODRecord class] )
        attribs = (NSDictionary *) ODRecordCopyDetails( (ODRecordRef) self, (CFArrayRef) inValues, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(attribs) autorelease];
}

- (NSArray *)valuesForAttribute:(NSString *)inAttribute error:(NSError **)outError
{
    NSArray *attribute = nil;

    if( [self class] == [NSODRecord class] )
        attribute = (NSArray *)ODRecordCopyValues( (ODRecordRef) self, (CFStringRef) inAttribute, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(attribute) autorelease];
}

- (NSString *)recordType
{
    if( [self class] == [NSODRecord class] )
        return (NSString *) ODRecordGetRecordType( (ODRecordRef) self );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    return nil;
}

- (NSString *)recordName
{
    if( [self class] == [NSODRecord class] )
        return (NSString *) ODRecordGetRecordName( (ODRecordRef) self );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    return nil;
}

- (BOOL)deleteRecord:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordDelete( (ODRecordRef) self, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)setValues:(NSArray *)inValues forAttribute:(NSString *)inAttributeName error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( nil != inValues )
    {
        if( [self class] == [NSODRecord class] )
            bResult = ODRecordSetValues( (ODRecordRef) self, (CFStringRef) inAttributeName, (CFTypeRef) inValues, (CFErrorRef *) outError );
        else
            NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    }

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)removeValuesForAttribute:(NSString *)inAttribute error:(NSError **)outError
{
    NSArray *tempArray  = [[NSArray alloc] init];
    BOOL    bResult     = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordSetValues( (ODRecordRef) self, (CFStringRef) inAttribute, (CFArrayRef) tempArray, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    [tempArray release];
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)addValue:(id)inValue toAttribute:(NSString *)inAttribute error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordAddValue( (ODRecordRef) self, (CFStringRef) inAttribute, (CFTypeRef) inValue, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)removeValue:(id)inValue fromAttribute:(NSString *)inAttribute error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordRemoveValue( (ODRecordRef) self, (CFStringRef) inAttribute, (CFTypeRef) inValue, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

@end

@implementation ODRecord (ODRecordGroupExtensions)

- (BOOL)addMemberRecord:(ODRecord *)inRecord error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        bResult = ODRecordAddMember( (ODRecordRef) self, (ODRecordRef) inRecord, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)removeMemberRecord:(ODRecord *)inRecord error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        return ODRecordRemoveMember( (ODRecordRef) self, (ODRecordRef) inRecord, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

- (BOOL)isMemberRecord:(ODRecord *)inRecord error:(NSError **)outError
{
    BOOL    bResult = NO;
    
    if( [self class] == [NSODRecord class] )
        return ODRecordContainsMember( (ODRecordRef) self, (ODRecordRef) inRecord, (CFErrorRef *) outError );
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return bResult;
}

@end

@implementation ODRecord (KeyValueCoding)

- (id)valueForUndefinedKey:(NSString *)key
{
    if( [self class] == [NSODRecord class] )
    {
        return [self valuesForAttribute: key error: nil];
    }
    else
        NSRequestConcreteImplementation( self, _cmd, [ODRecord class] );
    
    return [super valueForUndefinedKey:key];
}

@end

@implementation NSODRecord

- (CFTypeID)_cfTypeID
{
    return ODRecordGetTypeID();
}

- (BOOL)isEqual:(id)object
{
    if (nil == object) 
        return NO; 
    return (BOOL)CFEqual( (CFTypeRef)self, (CFTypeRef)object );
}

- (unsigned)hash
{ 
    return (unsigned)CFHash( (CFTypeRef)self );
}

- (id)copy
{
    return NULL;
}

- (id)retain
{
    CFRetain( (CFTypeRef)self );
    return (id)self;
}

- (oneway void)release
{
    CFRelease((CFTypeRef)self);
}

- (unsigned)retainCount
{
    return (unsigned)CFGetRetainCount((CFTypeRef)self);
}

- (void)finalize
{
    const CFRuntimeClass* cfclass = _CFRuntimeGetClassWithTypeID(CFGetTypeID(self));
    
    if (cfclass->finalize)
        cfclass->finalize(self);
    
    [super finalize];
}

- (NSString *)description
{
    NSString *theDescription = (NSString *) CFCopyDescription( self );
    
    return [NSMakeCollectable(theDescription) autorelease];
}

@end

@interface ODContext : NSObject
@end

@interface NSODContext : ODContext
@end

@implementation ODContext

- (CFTypeID)_cfTypeID
{
    return ODContextGetTypeID();
}

@end

@implementation NSODContext

- (BOOL)isEqual:(id)object
{
    if (nil == object) 
        return NO; 
    return (BOOL)CFEqual( (CFTypeRef)self, (CFTypeRef)object );
}

- (unsigned)hash
{ 
    return (unsigned)CFHash( (CFTypeRef)self );
}

- (id)copy
{
    return NULL;
}

- (id)retain
{
    CFRetain( (CFTypeRef)self );
    return (id)self;
}

- (oneway void)release
{
    CFRelease((CFTypeRef)self);
}

- (unsigned)retainCount
{
    return (unsigned)CFGetRetainCount((CFTypeRef)self);
}

- (void)finalize
{
    const CFRuntimeClass* cfclass = _CFRuntimeGetClassWithTypeID(CFGetTypeID(self));
    
    if (cfclass->finalize)
        cfclass->finalize(self);
    
    [super finalize];
}

@end

@interface NSODQuery : ODQuery
@end

@interface ODQuery (InternalAccess)
- (ODQueryRef)_getODQueryObject;
@end

@implementation ODQuery (InternalAccess)
- (ODQueryRef)_getODQueryObject
{
    if( [self class] != [NSODQuery class] ) 
    {
        return (ODQueryRef) _internal;
    }
    
    return (ODQueryRef) self;
}
@end

@implementation ODQuery

+ (void) initialize
{
    static int __done = 0;
    
    if( !__done )
    {
        __done = 1;
        
        ODSessionGetTypeID();
        ODNodeGetTypeID();
        ODQueryGetTypeID();
        ODRecordGetTypeID();
        ODContextGetTypeID();
    }
}

+ (id)allocWithZone:(NSZone *)inZone
{
    if( [self class] == [ODQuery class] )
        return (id) _createQuery( NULL );
    else
        return [super allocWithZone: inZone];
}

- (void) dealloc
{
    if( [self class] != [NSODQuery class] && NULL != _internal )
        CFRelease( _internal );
    
    [super dealloc];
}

+ (ODQuery *)queryWithNode:(ODNode *)inNode forRecordTypes:(id)inRecordTypeOrList attribute:(NSString *)inAttribute
                 matchType:(ODMatchType)inMatchType queryValues:(id)inQueryValueOrList 
          returnAttributes:(id)inReturnAttributeOrList maximumResults:(NSInteger)inMaximumResults
                     error:(NSError **)outError
{
    ODQuery *query = (ODQuery *) ODQueryCreateWithNode( NULL, (ODNodeRef) inNode, (CFTypeRef) inRecordTypeOrList,
                                                        (CFStringRef) inAttribute, inMatchType, (CFTypeRef) inQueryValueOrList,
                                                        (CFTypeRef) inReturnAttributeOrList, (CFIndex) inMaximumResults, 
                                                        (CFErrorRef *) &outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(query) autorelease];
}

+ (ODQuery *)queryWithNode:(ODNode *)inNode usingPredicate:(NSPredicate *)inPredicate
          returnAttributes:(id)inReturnAttributeOrList maximumResults:(NSInteger)inMaximumResults
                     error:(NSError **)outError
{
    ODQuery *query = [[ODQuery alloc] initWithNode: inNode usingPredicate: inPredicate returnAttributes: inReturnAttributeOrList
                                    maximumResults: inMaximumResults error: outError];

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];
    
    return [NSMakeCollectable(query) autorelease];
}

- (id)initWithNode:(ODNode *)inNode forRecordTypes:(id)inRecordTypeOrList attribute:(NSString *)inAttribute
         matchType:(ODMatchType)inMatchType queryValues:(id)inQueryValueOrList 
  returnAttributes:(id)inReturnAttributeOrList maximumResults:(NSInteger)inMaximumResults error:(NSError **)outError
{
    if ( NULL != outError )
        (*outError) = NULL;

    if( [self class] == [NSODQuery class] )
        _ODQueryInitWithNode( (ODQueryRef) self, (ODNodeRef) inNode, (CFTypeRef) inRecordTypeOrList, (CFStringRef) inAttribute,
                              inMatchType, (CFTypeRef) inQueryValueOrList, (CFTypeRef) inReturnAttributeOrList,
                              (CFIndex) inMaximumResults, (CFErrorRef *) outError );
    else
        _internal = ODQueryCreateWithNode( NULL, (ODNodeRef) inNode, (CFTypeRef) inRecordTypeOrList,
                                           (CFStringRef) inAttribute, inMatchType, (CFTypeRef) inQueryValueOrList,
                                           (CFTypeRef) inReturnAttributeOrList, (CFIndex) inMaximumResults, (CFErrorRef *) outError );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return NSMakeCollectable(self);
}

- (id)initWithNode:(ODNode *)inNode usingPredicate:(NSPredicate *)inPredicate returnAttributes:(id)inReturnAttributeOrList 
    maximumResults:(NSInteger)inMaximumResults error:(NSError **)outError
{
    ODQueryRef          query       = (ODQueryRef) self;
    ODMatchType         matchType   = kODMatchInsensitiveEqualTo;
    CFStringRef         attribute   = NULL;
    CFMutableArrayRef   recordTypes = NULL;
    CFMutableArrayRef   queryValues = NULL;
    
    if ( NULL != outError )
        (*outError) = NULL;

    if( [self class] != [NSODQuery class] )
    {
        _internal = _createQuery( NULL );
        query = (ODQueryRef) _internal;
    }
    
    // copy the predicate since we can't use changing predicates
    _ODQuerySetPredicate( query, [[inPredicate copy] autorelease] );
    
    // now we need to initialize the search with the predicate
    // need to figure out the optimal search up front, then we'll filter more later
#warning still need to parse the predicate
    _ODQueryInitWithNode( query, (ODNodeRef) inNode, recordTypes, attribute, matchType, queryValues, 
                          (CFTypeRef) inReturnAttributeOrList, (CFIndex) inMaximumResults, (CFErrorRef *) outError );
    
    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return NSMakeCollectable(self);
}

- (NSArray *)resultsAllowingPartial:(BOOL)inPartialResults error:(NSError **)outError
{
    NSArray  *results;

    if( [self class] == [NSODQuery class] )
        results = (NSArray *) ODQueryCopyResults( (ODQueryRef) self, (Boolean) inPartialResults, (CFErrorRef *) outError );
    else
        results = (NSArray *) ODQueryCopyResults( (ODQueryRef) _internal, (Boolean) inPartialResults, (CFErrorRef *) outError );

    if( NULL != outError && NULL != (*outError) )
        [NSMakeCollectable(*outError) autorelease];

    return [NSMakeCollectable(results) autorelease];
}

- (void)scheduleInRunLoop:(NSRunLoop *)inRunLoop forMode:(NSString *)inMode
{
    if( [self class] == [NSODQuery class] )
        ODQueryScheduleWithRunLoop( (ODQueryRef) self, [inRunLoop getCFRunLoop], (CFStringRef) inMode );
    else
        ODQueryScheduleWithRunLoop( (ODQueryRef) _internal, [inRunLoop getCFRunLoop], (CFStringRef) inMode );
}

- (void)removeFromRunLoop:(NSRunLoop *)inRunLoop forMode:(NSString *)inMode
{
    if( [self class] == [NSODQuery class] )    
        ODQueryUnscheduleFromRunLoop( (ODQueryRef) self, [inRunLoop getCFRunLoop], (CFStringRef) inMode );
    else
        ODQueryUnscheduleFromRunLoop( (ODQueryRef) _internal, [inRunLoop getCFRunLoop], (CFStringRef) inMode );
}

- (id)delegate
{
    if( [self class] == [NSODQuery class] )
        return _ODQueryGetDelegate( (ODQueryRef)self );
    else
        return _ODQueryGetDelegate( _internal );
}

- (void)setDelegate:(id)inDelegate
{
    if( [self class] == [NSODQuery class] )
    {
        _ODQuerySetDelegate( (ODQueryRef)self, inDelegate );
        ODQuerySetCallback( (ODQueryRef) self, queryCallback, self );
    }
    else
    {
        _ODQuerySetDelegate( _internal, inDelegate );
        ODQuerySetCallback( (ODQueryRef) _internal, queryCallback, self );
    }
}

- (void)synchronize
{
    if( [self class] == [NSODQuery class] )
        ODQuerySynchronize( (ODQueryRef) self );
    else
        ODQuerySynchronize( (ODQueryRef) _internal );
}

@end

@implementation NSODQuery

- (CFTypeID)_cfTypeID
{
    return ODQueryGetTypeID();
}

- (BOOL)isEqual:(id)object
{
    if (nil == object) 
        return NO; 
    return (BOOL)CFEqual( (CFTypeRef)self, (CFTypeRef)object );
}

- (unsigned)hash
{ 
    return (unsigned)CFHash( (CFTypeRef)self );
}

- (id)copy
{
    return NULL;
}

- (id)retain
{
    CFRetain( (CFTypeRef)self );
    return (id)self;
}

- (oneway void)release
{
    CFRelease((CFTypeRef)self);
}

- (unsigned)retainCount
{
    return (unsigned)CFGetRetainCount((CFTypeRef)self);
}

- (void)finalize
{
    const CFRuntimeClass* cfclass = _CFRuntimeGetClassWithTypeID(CFGetTypeID(self));
    
    if (cfclass->finalize)
        cfclass->finalize(self);
    
    [super finalize];
}

- (NSString *)description
{
    NSString *theDescription = (NSString *) CFCopyDescription( self );
    
    return [NSMakeCollectable(theDescription) autorelease];
}

@end
