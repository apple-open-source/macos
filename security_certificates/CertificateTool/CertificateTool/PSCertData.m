//
//  PSCertData.m
//  CertificateTool
//
//  Created by local on 1/24/13.
//  Copyright (c) 2013 James Murphy. All rights reserved.
//

#import "PSCertData.h"
#import "PSCert.h"

@interface PSCertData (PrivateMethods)
- (BOOL)createIndexAndTableDataFromArray;
@end


@implementation PSCertData

@synthesize cert_index_data = _cert_index_data;
@synthesize cert_table = _cert_table;

- (id)initWithCertificates:(NSArray *)certs;
{
    
    if ((self = [super init]))
    {
        _certs = certs;
        _cert_index_data = nil;
        _cert_table = nil;
        
        if (![self createIndexAndTableDataFromArray])
        {
            NSLog(@"Unable to create the index and certificate data");
            self = nil;
        }
    }
    return self;
}

- (BOOL)createIndexAndTableDataFromArray
{
    BOOL result = NO;
    
    // Create a dictionary keyed by the normalized subject hash for the cert
    NSMutableDictionary* records = [NSMutableDictionary dictionary];
    
    for (PSCert* aCert in _certs)
    {
        // Get the hash
        NSData* normalized_subject_hash = aCert.normalized_subject_hash;
        if (nil == normalized_subject_hash)
        {
            NSLog(@"Could not get the normalized hash for the cert at %@", aCert.file_path);
            continue;
        }
        
        // See if there is already an entry with this value
        NSMutableArray* items = [records objectForKey:normalized_subject_hash];
        if (nil == items)
        {
            // new item
            items = [NSMutableArray array];
        }
        [items addObject:aCert];
        
        
        [records setObject:items forKey:normalized_subject_hash];
    }
    
    // With all of the certificates in the dictionary get the array of keys and sort it
    NSArray* keys = [records allKeys];
    
    [keys sortedArrayUsingComparator: ^NSComparisonResult(id obj1, id obj2)
     {
         NSData* data1 = (NSData*)obj1;
         NSData* data2 = (NSData *)obj2;
         NSUInteger length1 = [data1 length];
         NSUInteger length2 = [data2 length];
         
         
         size_t compareLength = (size_t)((length1 <= length2) ? length1 : length2);
         int memcmp_result = memcmp([data1 bytes], [data2 bytes], compareLength);
         NSComparisonResult result = NSOrderedSame;
         if (memcmp_result < 0)
         {
             result = NSOrderedAscending;
         }
         else if (memcmp_result > 0)
         {
             result = NSOrderedDescending;
         }         
         return result;
     }];
    
    NSMutableData* indexData = [NSMutableData data];
    NSMutableData* tableData = [NSMutableData data];
    
    UInt32 record_ofset = 0;
    unsigned char paddingBuffer[7];
    memset(paddingBuffer, 0xFF, 7);
    
    for (NSData* keyData in keys)
    {
        // First write out the table data
        NSArray* certs = [records objectForKey:keyData];
        NSMutableData* aRecord = nil;        
        
        for (PSCert* aCert in certs)
        {
            //int32_t flags = (int32_t)[aCert.flags unsignedLongValue];
            int32_t record_length = (int32_t)(sizeof(int32_t) + sizeof(int32_t)  + [aCert.cert_data length]);
            int32_t aligned_record_length = ((record_length + 7) / 8) * 8;
            int32_t padding = aligned_record_length - record_length;
            int32_t cert_data_length = (int32_t)[aCert.cert_data length];
            
            aRecord = [NSMutableData data];
            [aRecord appendBytes:&aligned_record_length length:sizeof(aligned_record_length)];
            [aRecord appendBytes:&cert_data_length length:sizeof(cert_data_length)];
            [aRecord appendData:aCert.cert_data];
                        
            if (padding > 0)
            {
                [aRecord appendBytes:paddingBuffer length:padding];
            }
            
            [tableData appendData:aRecord];
            
            // Now update the index
            [indexData appendData:keyData];
            [indexData appendBytes:&record_ofset length:sizeof(record_ofset)];
            
            record_ofset += aligned_record_length;
        }        
     }
    
    _cert_index_data = indexData;
    _cert_table = tableData;
    
    result = YES;
     
    return result;
}

@end
