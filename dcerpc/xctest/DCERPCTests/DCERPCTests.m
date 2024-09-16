//
//  DCERPCTests.m
//  DCERPCTests
//
//  Created by William Conway on 12/1/23.
//

#import <XCTest/XCTest.h>
#import <spawn.h>
#import <signal.h>
#import <unistd.h>
#import <sys/socket.h>
#import <sys/un.h>
#import <sys/select.h>
#import "test_utils.h"
#import "xctest_iface.h"

// Note on building/running DCERPCTests-
// Currently the DCERPC framework target does not build in XCode due to rdar://117383501.
// As such the DCERPCTests target fails to build in XCode as well, since it has a
// target dependency on the DCERPC framework.
//
// Fortunately building/running the DCERPCTests target can all be accomplished from the command line as follows.
// Build the DCERPCTests target:
//     cd <SRCROOT directory>
//     sudo xcodebuild clean
//     sudo xcodebuild -project dcerpc.xcodeproj -target DCERPCTests -configuration Debug -arch arm64e
//
// Execute the DCERPCTests bundle:
//    cd <SRCROOT directory>/build/Debug
//    xctest -XCTest DCERPCTests/testMain DCERPCTests.xctest
//

// The following define will set DCERPC debug switches,
// causing the framework to emit verbose debug logging.
//
// #define Enable_DCERPC_DebugLogging 1

extern char **environ;

const char server_procname[] = "dcerpctest_server";

// The dcerpctest_server indicates it's initialized and ready to receive
// RPC calls by writing the readyString over the UNIX domain socket.
const char readyString[] = "ready";

// Path for the UNIX domain socket.
char sockPathTemplate[] = "/tmp/s.dcerpctest.XXXXX";

int checkServReady(int sockfd);

#ifdef Enable_DCERPC_DebugLogging
/* XXX this needs a home in the public headers ... */
extern void rpc__dbg_set_switches(
        const char      * /*s*/,
        unsigned32      * /*st*/);
#endif

@interface DCERPCTests : XCTestCase
{
    pid_t server_pid;
    rpc_binding_handle_t server_bind_handle;
}

@property bool server_running;

@end

@implementation DCERPCTests

-(id) init
{
    self = [super init];
    if (self) {
        _server_running = false;
        server_bind_handle = NULL;
    }
    
    return self;
}

// setUp spawns the dcerpctest_server process and waits for the server to write
// a ready indication to the UNIX domain socket. The _server_running state is set to TRUE when the dcerpctest_server
// was launched successfully and is ready to receive RPC calls.
- (void)setUp {
    int                 err, save_errno;
    int                 sockfd, newsockfd;
    struct timeval      timeout;
    socklen_t           cliAddrLen, servAddrLen;
    struct sockaddr_un  cliSockAddr, servSockAddr;
    fd_set              fdset;
    char                *argvec[4];
    
    // Make a path for the UNIX domain socket
    if (mkstemp(sockPathTemplate) < 0) {
        printf("dcerpctest_client:setUp: mkstemp error: %d\n", errno);
        return;
    }
    
    // Unlink the sock path in case it already exsits.
    // bind(2) will return EADDRINUSE if sock path already exists.
    unlink(sockPathTemplate);
    
    printf("dcerpctest_client:setUp: sockPath is: %s\n", sockPathTemplate);
    
#ifdef Enable_DCERPC_DebugLogging
    uint32_t            status;
    // Turn on rpc_es_dbg_general = 1
    rpc__dbg_set_switches("1.1", &status);
    
    // Turn on rpc_es_dbg_auth = 17
    rpc__dbg_set_switches("17.1", &status);
#endif
    
    // Open a UNIX domain stream socket
    sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("dcerpctest_client:setUp: socket(2) error: %d\n", errno);
        return;
    }
    
    // Bind our local address
    bzero(&cliSockAddr, sizeof(cliSockAddr));
    cliSockAddr.sun_family = PF_UNIX;
    strcpy(cliSockAddr.sun_path, sockPathTemplate);
    cliAddrLen = sizeof(struct sockaddr_un);
    
    err = bind(sockfd, (const struct sockaddr*)&cliSockAddr, cliAddrLen);
    if (err < 0) {
        printf("dcerpctest_client:setUp: bind(2) error: %d\n", errno);
        return;
    }
    
    // Spawn the dcerpctest server
    argvec[0] = (char *)server_procname;
    argvec[1] = (char *)"-s";
    argvec[2] = sockPathTemplate;
    argvec[3] = NULL;
    
    err = posix_spawnp(&server_pid, "./dcerpctest_server", NULL, NULL, argvec, environ);
    if (err) {
        save_errno = errno;
        printf("dcerpctest_client:setUp posix_spawn error: %d\n", save_errno);
        // Cleanup the socket path file
        unlink(sockPathTemplate);
        return;
    }
    
    // Now wait for the dcerpctest_server to indicate it's initialized and ready.
    listen(sockfd, 2);
    
    servAddrLen = sizeof(servSockAddr);

    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    
    // We wait 5 seconds for the sockfd
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    // Wait for the socket to be ready for reading, before calling accept(2).
    err = select(sockfd + 1, &fdset, NULL, NULL, &timeout);
    if (err < 0) {
        printf("dcerpctest_client:setUp: select error: %d\n", errno);
        // Cleanup the socket path file
        unlink(sockPathTemplate);
        return;
    } else if (err == 0) {
        printf("dcerpctest_client:setUp: timed out waiting for the dcerpctest server to indicate ready\n");
        // Cleanup the socket path file
        unlink(sockPathTemplate);
        return;
    }
    
    newsockfd = accept(sockfd, (struct sockaddr*)&servSockAddr, &servAddrLen);
    if (newsockfd < 0) {
        printf("dcerpctest_client:setUp: accept error: %d\n", errno);
        // Cleanup the socket path file
        unlink(sockPathTemplate);
        return;
    }
    
    // Now wait for the dcerpctest_server to write the Ready indication
    // to the socket.
    if (checkServReady(newsockfd)) {
        // Never received a Ready indication from the server.
        printf("dcerpctest_client:setUp: didn't receive Ready indication from the dcerpctest server\n");
    } else {
        // Success, the dcerpc test server is ready for testing
        printf("dcerpctest_client:setUp: Success, dcerpctest_server was spawned and indicated Ready status\n");
        _server_running = true;
    }

    // Close our sockets and we're done
    close (newsockfd);
    close (sockfd);
    
    // Cleanup the socket path file
    unlink(sockPathTemplate);
}

- (void)tearDown {
    int status;
    unsigned32 res;
    
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    printf("tearDown called\n");
    if (_server_running == true) {
        printf("tearDown: terminating dcerpctest_server ...\n");
        
        // Sending SIGINT tells dcerpctest_server to bail
        kill(server_pid, SIGINT);
        
        while (waitpid(server_pid, &status, 0) != server_pid) {
            // reading the newspaper...
        }
        
        if (WIFSIGNALED(status)) {
            printf("setUp: waiting for dcerpctest_server did exit on signal, good\n");
        } else {
            printf("setUp: waiting for dcerpctest_server did NOT exit gracefully, not good, status: %d\n", status);
        }
        _server_running = false;
        
    }
    
    if (server_bind_handle != NULL) {
        rpc_binding_free(&server_bind_handle, &res);
    }
}

int checkServReady(int sockfd)
{
    ssize_t nBytes;
    char rbuf[32];
    int ret = 1;
    size_t readyLen = strlen(readyString);
    
    memset(rbuf, 0, sizeof(rbuf));
    
    // We expect to read a string "ready" from the socket, case-insensitive.
    nBytes = read(sockfd, rbuf, 32);
    
    if (nBytes < 0) {
        printf("checkServReady: read(2) error: %d\n", errno);
        goto out_of_here;
    }
    
    if (nBytes < (ssize_t)readyLen) {
        printf("checkServReady: read(2) was truncated: expectLen: %lu, actualLen: %lu\n", readyLen, nBytes);
        goto out_of_here;
    }
    
    printf("checkServReady: read(2) returned %lu bytes\n", nBytes);
    
    if (strncasecmp(rbuf, readyString, readyLen) == 0) {
        // Success, this is what we were waiting for
        ret = 0;
    } else {
        printf("checkServReady: server sent something other than the \"Ready\" indication: '%s'\n", rbuf);
    }
    
out_of_here:
    return ret;
}

- (void)testMain {
    bool result;
    int status;
    
    printf("testEchoNumbers started\n");
    
    XCTAssertTrue(_server_running, @"Fatal, dcerpctest_server is not even running\n");
    
    // Get a client binding handle for all inteface tests
    status = get_client_rpc_binding(&server_bind_handle,
                "localhost",
                "ncacn_ip_tcp",
                xctest_endpoint);
    if (!status) {
        printf ("testMain: Couldnt obtain RPC server binding. exiting.\n");
        server_bind_handle = NULL;
        XCTAssertNotEqual(status, 0, @"get_client_rpc_binding() failed");
        return;
    }
    
    result = [self testEchoUint8];
    if (result == true) {
        printf("testMain: testEchoUint8 succeeded\n");
    } else {
        printf("testMain: testEchoUint8 failed\n");
    }
    XCTAssertTrue(result, @"Unit test testEchoUint8 failed");
    
    result = [self testEchoUint16];
    if (result == true) {
        printf("testMain: testEchoUint16 succeeded\n");
    } else {
        printf("testMain: testEchoUint16 failed\n");
    }
    XCTAssertTrue(result, @"Unit test testEchoUint16 failed");

    result = [self testEchoUint32];
    if (result == true) {
        printf("testMain: testEchoUint32 succeeded\n");
    } else {
        printf("testMain: testEchoUint32 failed\n");
    }
    XCTAssertTrue(result, @"Unit test testEchoUint32 failed");

    result = [self testEchoUint64];
    if (result == true) {
        printf("testMain: testEchoUint64 succeeded\n");
    } else {
        printf("testMain: testEchoUint64 failed\n");
    }
    XCTAssertTrue(result, @"Unit test testEchoUint64 failed");
    
    result = [self testReverseStringArr];
    if (result == true) {
        printf("testMain: testReverseStringArr succeeded\n");
    } else {
        printf("testMain: testReverseStringArr failed\n");
    }
    XCTAssertTrue(result, @"Unit test testReverseStringArr failed");
}

#if 0
- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}
#endif

- (bool)testEchoUint8
{
    bool ret = false;
    unsigned32 status;
    unsigned8 inNum, outNum;
    int ok;
    
    inNum = arc4random() & 0x000000ff;
    outNum = 0;
    
    printf("*** testEchoUint8 entered ***\n");
    
    // Finally we can do the rpc call
    ok = EchoUint8(server_bind_handle, inNum, &outNum, &status);
    
    if (ok && status == error_status_ok) {
        printf ("testEchoUint8: got response from server:  sent: %u, return: %u\n", inNum, outNum);
        
        // Make sure the server returned the correct value.
        XCTAssertEqual(inNum, outNum, @"EchoUint8 echo mismatch: numIn: 0x%x, numOut: 0x%x", inNum, outNum);
        ret = true;
    } else {
        chk_dce_err(status, "EchoUint8()", "testEchoUint8", 0);
        printf ("testEchoUint8: EchoUint8 rpc call problem, ok: %d, rpc status: %u\n", ok, status);
        
        XCTAssertNotEqual(ok, 0, @"EchoUint8 rpc call returned zero");
        XCTAssertEqual(status, error_status_ok, @"EchoUint8 rpc call status: 0x%x", status);
    }
    
    return (ret);
}

- (bool)testEchoUint16
{
    bool ret = false;
    unsigned32 status;
    unsigned16 inNum, outNum;
    int ok;
    
    inNum = arc4random() & 0x0000ffff;
    outNum = 0;
    
    printf("*** testEchoUint16 entered ***\n");
    
    // Finally we can do the rpc call
    ok = EchoUint16(server_bind_handle, inNum, &outNum, &status);
    
    if (ok && status == error_status_ok) {
        printf ("testEchoUint16: got response from server:  sent: %u, return: %u\n", inNum, outNum);
        
        // Make sure the server returned the correct value.
        XCTAssertEqual(inNum, outNum, @"EchoUint16 echo mismatch: numIn: 0x%x, numOut: 0x%x", inNum, outNum);
        
        ret = true;
    } else {
        chk_dce_err(status, "EchoUint16()", "testEchoUint16", 0);
        printf ("testEchoUint32: EchoUint32 problem, ok: %d, status: %u\n", ok, status);
        
        XCTAssertNotEqual(ok, 0, @"EchoUint16 rpc call returned zero");
        XCTAssertEqual(status, error_status_ok, @"EchoUint16 rpc call status: 0x%x", status);
    }
    
    return (ret);
}

- (bool)testEchoUint32
{
    bool ret = false;
    unsigned32 status;
    unsigned32 inNum, outNum;
    int ok;
    
    inNum = arc4random();
    outNum = 0;
    
    printf("*** testEchoUint32 entered ***\n");
    
    // Finally we can do the rpc call
    ok = EchoUint32(server_bind_handle, inNum, &outNum, &status);
    
    if (ok && status == error_status_ok) {
        printf ("testEchoUint32: got response from server:  sent: %u, return: %u\n", inNum, outNum);
        
        // Make sure the server returned the correct value.
        XCTAssertEqual(inNum, outNum, @"EchoUint32 echo mismatch: numIn: 0x%x, numOut: 0x%x", inNum, outNum);
        
        ret = true;
    } else {
        chk_dce_err(status, "testEchoUint32()", "EchoUint32", 0);
        printf ("testEchoUint32: EchoUint32 problem, ok: %d, status: %u\n", ok, status);
        
        XCTAssertNotEqual(ok, 0, @"EchoUint32 rpc call returned zero");
        XCTAssertEqual(status, error_status_ok, @"EchoUint32 rpc call status: 0x%x", status);
    }
    
    return (ret);
}

- (bool)testEchoUint64
{
    bool ret = false;
    unsigned32 status;
    uint64_t inNum, outNum;
    int ok;
    
    inNum = (uint64_t)arc4random() << 32;
    inNum |= arc4random();
    outNum = 0;
    
    printf("*** testEchoUint64 entered ***\n");
    
    // Finally we can do the rpc call
    ok = EchoUint64(server_bind_handle, inNum, &outNum, &status);
    
    if (ok && status == error_status_ok) {
        printf ("testEchoUint64: got response from server:  sent: 0x%llx, return: 0x%llx\n", inNum, outNum);
        
        // Make sure the server returned the correct value.
        XCTAssertEqual(inNum, outNum, @"EchoUint64 echo mismatch: numIn: 0x%llx, numOut: 0x%llx", inNum, outNum);
        
        ret = true;
    } else {
        chk_dce_err(status, "testEchoUint64()", "EchoUint64", 0);
        printf ("testEchoUint64: EchoUint64 problem, ok: %d, status: %u\n", ok, status);
        
        XCTAssertNotEqual(ok, 0, @"EchoUint64 rpc call returned zero");
        XCTAssertEqual(status, error_status_ok, @"EchoUint64 rpc call status: 0x%x", status);
    }
    
    return (ret);
}

// ****************************
// *** testReverseStringArr ***
// ****************************
const char * const string1 = ".gnihtyreve deyortsed ylraen dna raf oot ti koot I";
const char * const string2 = ".lliw eerf esoohc lliw I .eciohc a edam evah llits uoy ,ediced ot ton esoohc uoy fI";
const char * const string3 = ".deificepsu si dnuob reppu eht hcihw rof yarra na fo noisnemid hcae rof tneserp eb tsum >rav_rtta< nA";
const char * const string4 = "Z Y X V U T S R Q P O N M L K J I H G F E D C B A 9 8 7 6 5 4 3 2 1 0";

#define NUM_STRINGS 4
- (bool)testReverseStringArr
{
    rev_str_arr_args *inStrArr, *outStrArr;
    unsigned32 status;
    bool ok;
    bool ret = false;
    
    outStrArr = NULL;
    
    // Grab some space
    inStrArr = malloc(sizeof(rev_str_arr_args) + NUM_STRINGS * sizeof(string_t));
    
    inStrArr->str_arr[0] = (unsigned char *)string1;
    inStrArr->str_arr[1] = (unsigned char *)string2;
    inStrArr->str_arr[2] = (unsigned char *)string3;
    inStrArr->str_arr[3] = (unsigned char *)string4;
    
    inStrArr->arr_len = NUM_STRINGS;
    
    // printf("\ntestReverseStringArr: INPUT Str1: %s\n", inStrArr->str_arr[0]);
    // printf("testReverseStringArr: INPUT Str2: %s\n", inStrArr->str_arr[1]);
    // printf("testReverseStringArr: INPUT Str3: %s\n", inStrArr->str_arr[2]);
    // printf("testReverseStringArr: INPUT Str4: %s\n", inStrArr->str_arr[3]);
    
    // Make the RPC call
    ok = ReverseStrArr(server_bind_handle, inStrArr, &outStrArr, &status);
    if (ok && status == error_status_ok) {
        
        if (outStrArr == NULL) {
            printf("testReverseStringArr: Big problem, major fail, output arr is NULL\n");
            return false;
        }
        
        if (outStrArr->arr_len != NUM_STRINGS) {
            printf("testReverseStringArr: Output array truncated, outStrArr->arr_len: %u != %u (NUM_STRINGS)\n",
                   outStrArr->arr_len, NUM_STRINGS);
            return false;
        }
        
        printf("\ntestReverseStringArr: REPLY str_arr[0]: %s\n", outStrArr->str_arr[0]);
        printf("testReverseStringArr: REPLY str_arr[1]: %s\n", outStrArr->str_arr[1]);
        printf("testReverseStringArr: REPLY str_arr[2]: %s\n", outStrArr->str_arr[2]);
        printf("testReverseStringArr: REPLY str_arr[3]: %s\n\n", outStrArr->str_arr[3]);
        
        // Now check if the strings were reversed correctly
        ok = stringsAreReversed((const char *)inStrArr->str_arr[0], (const char *)outStrArr->str_arr[0]);
        if (!ok) {
            printf("testReverseStringArr: outString0: %s not the reverse of inString0: %s\n",
                   (const char *)outStrArr->str_arr[0], (const char *)inStrArr->str_arr[0]);
            XCTAssertTrue(ok, @"Unit test testReverseStringArr string0 mismatch");
        }
        
        ok = stringsAreReversed((const char *)inStrArr->str_arr[1], (const char *)outStrArr->str_arr[1]);
        if (!ok) {
            printf("testReverseStringArr: outString1: %s not the reverse of inString1: %s\n",
                   (const char *)outStrArr->str_arr[1], (const char *)inStrArr->str_arr[1]);
            XCTAssertTrue(ok, @"Unit test testReverseStringArr string1 mismatch");
        }
        
        ok = stringsAreReversed((const char *)inStrArr->str_arr[2], (const char *)outStrArr->str_arr[2]);
        if (!ok) {
            printf("testReverseStringArr: outString2: %s not the reverse of inString2: %s\n",
                   (const char *)outStrArr->str_arr[2], (const char *)inStrArr->str_arr[2]);
            XCTAssertTrue(ok, @"Unit test testReverseStringArr string2 mismatch");
        }
        
        ok = stringsAreReversed((const char *)inStrArr->str_arr[3], (const char *)outStrArr->str_arr[3]);
        if (!ok) {
            printf("testReverseStringArr: outString3: %s not the reverse of inString3: %s\n",
                   (const char *)outStrArr->str_arr[3], (const char *)inStrArr->str_arr[3]);
            XCTAssertTrue(ok, @"Unit test testReverseStringArr string3 mismatch");
        }
        
        // Free outStrArr?  It would seem so.
        free (outStrArr->str_arr[0]);
        free (outStrArr->str_arr[1]);
        free (outStrArr->str_arr[2]);
        free (outStrArr->str_arr[3]);
        free (outStrArr);
    
        ret = true;
    } else {
        chk_dce_err(status, "testReverseStringArr()", "ReverseStrArr", 0);
        printf ("testReverseStringArr: EchoUint64 problem, ok: %d, status: %u\n", ok, status);
        
        XCTAssertNotEqual(ok, 0, @"ReverseStrArr rpc call returned zero");
        XCTAssertEqual(status, error_status_ok, @"ReverseStrArr rpc call status: 0x%x", status);
    }

    return ret;
}

@end
