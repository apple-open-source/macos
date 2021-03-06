// This file was automatically generated by protocompiler
// DO NOT EDIT!
// Compiled from stdin

#import "AWDIPMonitorInterfaceAdvisoryReport.h"
#import <ProtocolBuffer/PBConstants.h>
#import <ProtocolBuffer/PBHashUtil.h>
#import <ProtocolBuffer/PBDataReader.h>

@implementation AWDIPMonitorInterfaceAdvisoryReport

@synthesize timestamp = _timestamp;
- (void)setTimestamp:(uint64_t)v
{
    _has.timestamp = YES;
    _timestamp = v;
}
- (void)setHasTimestamp:(BOOL)f
{
    _has.timestamp = f;
}
- (BOOL)hasTimestamp
{
    return _has.timestamp;
}
@synthesize interfaceType = _interfaceType;
- (AWDIPMonitorInterfaceType)interfaceType
{
    return _has.interfaceType ? _interfaceType : AWDIPMonitorInterfaceType_IPMONITOR_INTERFACE_TYPE_OTHER;
}
- (void)setInterfaceType:(AWDIPMonitorInterfaceType)v
{
    _has.interfaceType = YES;
    _interfaceType = v;
}
- (void)setHasInterfaceType:(BOOL)f
{
    _has.interfaceType = f;
}
- (BOOL)hasInterfaceType
{
    return _has.interfaceType;
}
- (NSString *)interfaceTypeAsString:(AWDIPMonitorInterfaceType)value
{
    return AWDIPMonitorInterfaceTypeAsString(value);
}
- (AWDIPMonitorInterfaceType)StringAsInterfaceType:(NSString *)str
{
    return StringAsAWDIPMonitorInterfaceType(str);
}
@synthesize flags = _flags;
- (void)setFlags:(uint32_t)v
{
    _has.flags = YES;
    _flags = v;
}
- (void)setHasFlags:(BOOL)f
{
    _has.flags = f;
}
- (BOOL)hasFlags
{
    return _has.flags;
}
@synthesize advisoryCount = _advisoryCount;
- (void)setAdvisoryCount:(uint32_t)v
{
    _has.advisoryCount = YES;
    _advisoryCount = v;
}
- (void)setHasAdvisoryCount:(BOOL)f
{
    _has.advisoryCount = f;
}
- (BOOL)hasAdvisoryCount
{
    return _has.advisoryCount;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ %@", [super description], [self dictionaryRepresentation]];
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if (self->_has.timestamp)
    {
        [dict setObject:[NSNumber numberWithUnsignedLongLong:self->_timestamp] forKey:@"timestamp"];
    }
    if (self->_has.interfaceType)
    {
        [dict setObject:AWDIPMonitorInterfaceTypeAsString(self->_interfaceType) forKey:@"interface_type"];
    }
    if (self->_has.flags)
    {
        [dict setObject:[NSNumber numberWithUnsignedInt:self->_flags] forKey:@"flags"];
    }
    if (self->_has.advisoryCount)
    {
        [dict setObject:[NSNumber numberWithUnsignedInt:self->_advisoryCount] forKey:@"advisory_count"];
    }
    return dict;
}

BOOL AWDIPMonitorInterfaceAdvisoryReportReadFrom(AWDIPMonitorInterfaceAdvisoryReport *self, PBDataReader *reader) {
    while (PBReaderHasMoreData(reader)) {
        uint32_t tag = 0;
        uint8_t aType = 0;

        PBReaderReadTag32AndType(reader, &tag, &aType);

        if (PBReaderHasError(reader))
            break;

        if (aType == TYPE_END_GROUP) {
            break;
        }

        switch (tag) {

            case 1 /* timestamp */:
            {
                self->_has.timestamp = YES;
                self->_timestamp = PBReaderReadUint64(reader);
            }
            break;
            case 2 /* interfaceType */:
            {
                self->_has.interfaceType = YES;
                self->_interfaceType = PBReaderReadInt32(reader);
            }
            break;
            case 3 /* flags */:
            {
                self->_has.flags = YES;
                self->_flags = PBReaderReadUint32(reader);
            }
            break;
            case 4 /* advisoryCount */:
            {
                self->_has.advisoryCount = YES;
                self->_advisoryCount = PBReaderReadUint32(reader);
            }
            break;
            default:
                if (!PBReaderSkipValueWithTag(reader, tag, aType))
                    return NO;
                break;
        }
    }
    return !PBReaderHasError(reader);
}

- (BOOL)readFrom:(PBDataReader *)reader
{
    return AWDIPMonitorInterfaceAdvisoryReportReadFrom(self, reader);
}
- (void)writeTo:(PBDataWriter *)writer
{
    /* timestamp */
    {
        if (self->_has.timestamp)
        {
            PBDataWriterWriteUint64Field(writer, self->_timestamp, 1);
        }
    }
    /* interfaceType */
    {
        if (self->_has.interfaceType)
        {
            PBDataWriterWriteInt32Field(writer, self->_interfaceType, 2);
        }
    }
    /* flags */
    {
        if (self->_has.flags)
        {
            PBDataWriterWriteUint32Field(writer, self->_flags, 3);
        }
    }
    /* advisoryCount */
    {
        if (self->_has.advisoryCount)
        {
            PBDataWriterWriteUint32Field(writer, self->_advisoryCount, 4);
        }
    }
}

- (void)copyTo:(AWDIPMonitorInterfaceAdvisoryReport *)other
{
    if (self->_has.timestamp)
    {
        other->_timestamp = _timestamp;
        other->_has.timestamp = YES;
    }
    if (self->_has.interfaceType)
    {
        other->_interfaceType = _interfaceType;
        other->_has.interfaceType = YES;
    }
    if (self->_has.flags)
    {
        other->_flags = _flags;
        other->_has.flags = YES;
    }
    if (self->_has.advisoryCount)
    {
        other->_advisoryCount = _advisoryCount;
        other->_has.advisoryCount = YES;
    }
}

- (id)copyWithZone:(NSZone *)zone
{
    AWDIPMonitorInterfaceAdvisoryReport *copy = [[[self class] allocWithZone:zone] init];
    if (self->_has.timestamp)
    {
        copy->_timestamp = _timestamp;
        copy->_has.timestamp = YES;
    }
    if (self->_has.interfaceType)
    {
        copy->_interfaceType = _interfaceType;
        copy->_has.interfaceType = YES;
    }
    if (self->_has.flags)
    {
        copy->_flags = _flags;
        copy->_has.flags = YES;
    }
    if (self->_has.advisoryCount)
    {
        copy->_advisoryCount = _advisoryCount;
        copy->_has.advisoryCount = YES;
    }
    return copy;
}

- (BOOL)isEqual:(id)object
{
    AWDIPMonitorInterfaceAdvisoryReport *other = (AWDIPMonitorInterfaceAdvisoryReport *)object;
    return [other isMemberOfClass:[self class]]
    &&
    ((self->_has.timestamp && other->_has.timestamp && self->_timestamp == other->_timestamp) || (!self->_has.timestamp && !other->_has.timestamp))
    &&
    ((self->_has.interfaceType && other->_has.interfaceType && self->_interfaceType == other->_interfaceType) || (!self->_has.interfaceType && !other->_has.interfaceType))
    &&
    ((self->_has.flags && other->_has.flags && self->_flags == other->_flags) || (!self->_has.flags && !other->_has.flags))
    &&
    ((self->_has.advisoryCount && other->_has.advisoryCount && self->_advisoryCount == other->_advisoryCount) || (!self->_has.advisoryCount && !other->_has.advisoryCount))
    ;
}

- (NSUInteger)hash
{
    return 0
    ^
    (self->_has.timestamp ? PBHashInt((NSUInteger)self->_timestamp) : 0)
    ^
    (self->_has.interfaceType ? PBHashInt((NSUInteger)self->_interfaceType) : 0)
    ^
    (self->_has.flags ? PBHashInt((NSUInteger)self->_flags) : 0)
    ^
    (self->_has.advisoryCount ? PBHashInt((NSUInteger)self->_advisoryCount) : 0)
    ;
}

- (void)mergeFrom:(AWDIPMonitorInterfaceAdvisoryReport *)other
{
    if (other->_has.timestamp)
    {
        self->_timestamp = other->_timestamp;
        self->_has.timestamp = YES;
    }
    if (other->_has.interfaceType)
    {
        self->_interfaceType = other->_interfaceType;
        self->_has.interfaceType = YES;
    }
    if (other->_has.flags)
    {
        self->_flags = other->_flags;
        self->_has.flags = YES;
    }
    if (other->_has.advisoryCount)
    {
        self->_advisoryCount = other->_advisoryCount;
        self->_has.advisoryCount = YES;
    }
}

@end

