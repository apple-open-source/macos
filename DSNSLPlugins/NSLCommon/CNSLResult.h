/*
 *  CNSLResult.h
 *
 *	This class represents a result of a single service.  This is composed of at least
 *	a url, as well as an optional list of attribute value pairs.
 *
 *	All values are assumed to be UTF8 encoded
 *
 *  Created by imlucid on Mon Aug 20 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#ifndef _CNSLResult_
#define _CNSLResult_ 1

#include <CoreFoundation/CoreFoundation.h>
#include <DirectoryService/DirServicesConst.h>

#define kNSLAttrServiceType			kDS1AttrServiceType
#define kNSLAttrRecordType			kDSNAttrRecordType

CFStringRef SLPRAdminNotifierCopyDesctriptionCallback ( const void *item );
Boolean SLPRAdminNotifierEqualCallback ( const void *item1, const void *item2 );

class CNSLResult
{
public:
                                    CNSLResult			();
                                    CNSLResult			( CFMutableDictionaryRef initialResults );
                                    ~CNSLResult			();
                                    
            void					SetURL				( const char* urlPtr );
            void					SetURL				( CFStringRef urlStringRef );
            
            void					SetServiceType		( const char* serviceType );
            void					SetServiceType		( CFStringRef serviceTypeRef );
            
            void					AddAttribute		( const char* key, const char* value );
            void					AddAttribute		( CFStringRef keyRef, CFStringRef valueRef );
            
            CFStringRef				GetURLRef			( void ) const { return GetAttributeRef( CFSTR(kDSNAttrURL) ); }
            CFStringRef				GetServiceTypeRef	( void ) const { return GetAttributeRef( CFSTR(kDS1AttrServiceType) ); }
            CFStringRef				GetAttributeRef		( CFStringRef keyRef ) const;
            
            CFDictionaryRef			GetAttributeDict	( void ) const { return mAttributes; }
protected:

private:
//            CFStringRef				mURLForService;
//            CFStringRef				mServiceType;
            CFMutableDictionaryRef	mAttributes;
			CNSLResult*				mSelfPtr;
};


#endif	// #ifndef









