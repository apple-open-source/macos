//
//  KCAESGCMTest.m
//  Keychain Circle
//
//

#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>
#import <KeychainCircle/KCAESGCMDuplexSession.h>

@interface KCAESGCMTest : XCTestCase

@end

@implementation KCAESGCMTest

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void) sendMessage: (NSData*) message
                from: (KCAESGCMDuplexSession*) sender
                  to: (KCAESGCMDuplexSession*) receiver {
    NSError* error = nil;
    NSData* sendToRecv = [sender encrypt:message error:&error];

    XCTAssertNil(error, @"Got error");
    XCTAssertNotNil(sendToRecv, @"Failed to get data");

    error = nil;
    NSData* decryptedSendToRecv = [receiver decryptAndVerify:sendToRecv error:&error];

    XCTAssertNil(error, @"Error decrypting");
    XCTAssertNotNil(decryptedSendToRecv, @"Got decryption");

    XCTAssertEqualObjects(message, decryptedSendToRecv, @"Send to recv failed.");
}

- (void)testAESGCMDuplex {
    uint64_t context = 0x81FC134000123041;
    uint8_t secretBytes[] = { 0x11, 0x22, 0x33, 0x13, 0x44, 0xF1, 0x13, 0x92, 0x11, 0x22, 0x33, 0x13, 0x44, 0xF1, 0x13, 0x92 };
    NSData* secret = [NSData dataWithBytes:secretBytes length:sizeof(secretBytes)];

    KCAESGCMDuplexSession* sender = [KCAESGCMDuplexSession sessionAsSender:secret
                                                                   context:context];

    KCAESGCMDuplexSession* receiver = [KCAESGCMDuplexSession sessionAsReceiver:secret
                                                                       context:context];

    uint8_t sendToRecvBuffer[] = { 0x1, 0x2, 0x3, 0x88, 0xFF, 0xE1 };
    NSData* sendToRecvData = [NSData dataWithBytes:sendToRecvBuffer length:sizeof(sendToRecvBuffer)];

    [self sendMessage:sendToRecvData from:sender to:receiver];

    uint8_t recvToSendBuffer[] = { 0x81, 0x52, 0x63, 0x88, 0xFF, 0xE1 };
    NSData* recvToSendData = [NSData dataWithBytes:recvToSendBuffer length:sizeof(recvToSendBuffer)];

    [self sendMessage:recvToSendData from:receiver to:sender];
}

- (KCAESGCMDuplexSession*) archiveDearchive: (KCAESGCMDuplexSession*) original {
    NSMutableData *data = [NSMutableData data];
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initForWritingWithMutableData:data];
    [archiver encodeObject:original forKey:@"Top"];
    [archiver finishEncoding];

    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingWithData:data];

    // Customize the unarchiver.
    KCAESGCMDuplexSession *result = [unarchiver decodeObjectForKey:@"Top"];
    [unarchiver finishDecoding];

    return result;
}

- (void)doAESGCMDuplexCodingFlattenSender: (bool) flattenSender
                                 Receiver: (bool) flattenReceiver {
    uint64_t context = 0x81FC134000123041;
    uint8_t secretBytes[] = { 0x73, 0xb7, 0x7f, 0xff, 0x7f, 0xe3, 0x44, 0x6b, 0xa4, 0xec, 0x9d, 0x5d, 0x68, 0x12, 0x13, 0x71 };
    NSData* secret = [NSData dataWithBytes:secretBytes length:sizeof(secretBytes)];

    KCAESGCMDuplexSession* sender = [KCAESGCMDuplexSession sessionAsSender:secret
                                                                   context:context];

    KCAESGCMDuplexSession* receiver = [KCAESGCMDuplexSession sessionAsReceiver:secret
                                                                       context:context];

    {
        uint8_t sendToRecvBuffer[] = { 0x0e, 0x9b, 0x9d, 0x2c, 0x90, 0x96, 0x8a };
        NSData* sendToRecvData = [NSData dataWithBytes:sendToRecvBuffer length:sizeof(sendToRecvBuffer)];

        [self sendMessage:sendToRecvData from:sender to:receiver];


        uint8_t recvToSendBuffer[] = {  0x9b, 0x63, 0xaf, 0xb5, 0x4d, 0xa0, 0xfa, 0x9d, 0x90 };
        NSData* recvToSendData = [NSData dataWithBytes:recvToSendBuffer length:sizeof(recvToSendBuffer)];

        [self sendMessage:recvToSendData from:receiver to:sender];
    }

    // Re-encode...
    if (flattenSender) {
        sender = [self archiveDearchive:sender];
    }

    if (flattenReceiver) {
        receiver = [self archiveDearchive:receiver];
    }

    {
        uint8_t sendToRecvBuffer[] = { 0xae, 0xee, 0x5f, 0x62, 0xb2, 0x72, 0x6f, 0x0a, 0xb6, 0x56 };
        NSData* sendToRecvData = [NSData dataWithBytes:sendToRecvBuffer length:sizeof(sendToRecvBuffer)];

        [self sendMessage:sendToRecvData from:sender to:receiver];


        uint8_t recvToSendBuffer[] = { 0x49, 0x0b, 0xbb, 0x2d, 0x20, 0xb1, 0x8a, 0xfc, 0xba, 0xd1, 0xFF };
        NSData* recvToSendData = [NSData dataWithBytes:recvToSendBuffer length:sizeof(recvToSendBuffer)];

        [self sendMessage:recvToSendData from:receiver to:sender];
    }
}

- (void)testAESGCMDuplexCoding {
    [self doAESGCMDuplexCodingFlattenSender:NO Receiver:YES];
    [self doAESGCMDuplexCodingFlattenSender:YES Receiver:NO];
    [self doAESGCMDuplexCodingFlattenSender:YES Receiver:YES];
}


@end
