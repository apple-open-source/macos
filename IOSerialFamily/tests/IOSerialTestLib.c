
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <dispatch/dispatch.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <IOKit/serial/ioss.h>

#include "IOSerialTestLib.h"

int _testIOSSIOSPEEDIoctl(int fd, speed_t speed);
int _modifyAttributes(int fd, struct termios *originalOptions);
int _modifyModemLines(int fd);
void _sleepAndReenumerate(const char *deviceid, struct timespec tim);
void _sleepAndClose(int fd, struct timespec tim);
void _sleepAndWrite(int fd, struct timespec tim);
void _sleepAndRead(int fd, struct timespec tim);
int _openAndClose(const char *path);

#pragma mark -

int _testIOSSIOSPEEDIoctl(int fd, speed_t speed)
{
    struct termios options;
    
    // The IOSSIOSPEED ioctl can be used to set arbitrary baud rates other than
    // those specified by POSIX. The driver for the underlying serial hardware
    // ultimately determines which baud rates can be used. This ioctl sets both
    // the input and output speed.
    
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
        printf("[WARN] ioctl(..., IOSSIOSPEED, %lu).\n", speed);
        goto fail;
    }
    
    // Check that speed is properly modified
    if (tcgetattr(fd, &options) == -1) {
        printf("[WARN] _modifyAttributes: tcgetattr failed\n");
        goto fail;
    }
    
    if (cfgetispeed(&options) != speed ||
        cfgetospeed(&options) != speed) {
        printf("[WARN] _modifyAttributes: cfsetspeed failed, %lu, %lu.\n",
               speed,
               cfgetispeed(&options));
        goto fail;
    }
    
    return 0;
fail:
    return -1;
    
}

int _modifyAttributes(int fd, struct termios *originalOptions)
{
    int result = 0;
    unsigned long mics = 1UL;
    struct termios options;
    
    if (!originalOptions) {
        printf("[FAIL] _modifyAttributes: NULL argument unexpected\n");
        goto fail;
    }
    
    // prevent additional opens on the device, except from a root-owned process
    if (ioctl(fd, TIOCEXCL) == -1) {
        printf("[FAIL] _modifyAttributes: ioctl TIOCEXCL failed\n");
        goto fail;
    }
    
    // clear the O_NONBLOCK flag so subsequent I/O will block
    if (fcntl(fd, F_SETFL, 0) == -1) {
        printf("[FAIL] _modifyAttributes: fcntl failed\n");
        goto fail;
    }
    
    // snapshot the current terminal state in originalOptions
    if (tcgetattr(fd, originalOptions) == -1) {
        printf("[FAIL] _modifyAttributes: tcgetattr failed\n");
        goto fail;
    }
    
    options = *originalOptions;
    
    // Set raw input (non-canonical) mode
    cfmakeraw(&options);
    
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;
    
    // Set 19200 baud
    if (cfsetspeed(&options, B19200) == -1) {
        printf("[FAIL] _modifyAttributes: cfsetspeed failed\n");
        goto fail;
    }
    
    options.c_cflag |= (CS7        |    // Use 7 bit words
                        PARENB     |    // Parity enable (even parity if PARODD not also set)
                        CCTS_OFLOW |    // CTS flow control of output
                        CRTS_IFLOW);    // RTS flow control of input
    
    // Cause the new options to take effect immediately.
    if (tcsetattr(fd, TCSANOW, &options) == -1) {
        printf("[FAIL] _modifyAttributes: tcsetattr failed\n");
        goto fail;
    }
    
    // check that the tcsetattr worked properly
    if (tcgetattr(fd, &options) == -1) {
        printf("[FAIL] _modifyAttributes: tcgetattr failed\n");
        goto fail;
    }
    if ((options.c_cflag & (CS7 | PARENB | CCTS_OFLOW | CRTS_IFLOW)) !=
        (CS7 | PARENB | CCTS_OFLOW | CRTS_IFLOW)) {
        printf("[FAIL] _modifyAttributes: tcsetattr/tcgetattr failed\n");
        goto fail;
    }
    
    // Check that speed is 19200 baud
    if (cfgetispeed(&options) != B19200 ||
        cfgetospeed(&options) != B19200) {
        printf("[FAIL] _modifyAttributes: cfsetspeed failed\n");
        goto fail;
    }
    
    // Set the receive latency in microseconds
    if (ioctl(fd, IOSSDATALAT, &mics) == -1) {
        // set latency to 1 microsecond
        printf("[FAIL] _modifyAttributes: ioctl IOSSDATALAT failed\n");
        goto fail;
    }
    
    return result;
    
fail:
    return -1;
}

int _modifyModemLines(int fd)
{
    int handshake;
        
    // Assert Data Terminal Ready (DTR)
    if (ioctl(fd, TIOCSDTR) == -1) {
        printf("[FAIL] _modifyModemLines: ioctl TIOCSDTR failed\n");
        goto fail;
    }
    // Clear Data Terminal Ready (DTR)
    if (ioctl(fd, TIOCCDTR) == -1) {
        printf("[FAIL] _modifyModemLines: ioctl TIOCCDTR failed\n");
        goto fail;
    }
    
    // Set the modem lines depending on the bits set in handshake
    handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
    if (ioctl(fd, TIOCMSET, &handshake) == -1) {
        printf("[FAIL] _modifyModemLines: ioctl TIOCMSET failed\n");
        goto fail;
    }
    
    // Store the state of the modem lines in handshake
    if (ioctl(fd, TIOCMGET, &handshake) == -1) {
        printf("[FAIL] _modifyModemLines: ioctl TIOCMGET failed\n");
        goto fail;
    }
    
    return 0;
    
fail:
    return -1;
}

inline void _sleepAndReenumerate(const char *deviceid, struct timespec tim)
{
    struct timespec tim2;
    
    nanosleep(&tim, &tim2);
    
    execlp("/AppleInternal/Applications/USB Prober.app/Contents/Resources/reenumerate",
           "reenumerate", "-v", deviceid,
           NULL);
}

inline void _sleepAndClose(int fd, struct timespec tim)
{
    struct timespec tim2;
    int result = 0;
    
    nanosleep(&tim, &tim2);
    
    printf("closing\n");
    result = close(fd);
    if (result) {
        printf("close failed\n");
        return;
    }
    printf("closed\n");
}

inline void _sleepAndWrite(int fd, struct timespec tim)
{
    struct timespec tim2;
    int result = 0;
    ssize_t     numBytes;       // Number of bytes read or written
    
    nanosleep(&tim, &tim2);

    printf("writing\n");
    
    numBytes = write(fd, "Hello World", strnlen("Hello World", 256));
    if (numBytes == -1) {
        printf("write returned -1\n");
    }
    else if ((size_t)numBytes < strnlen("Hello World", 256)) {
        printf("write did not complete\n");
    }
    
    printf("write returned %zd\n", numBytes);
    
    printf("closing\n");
    result = close(fd);
    if (result) {
        printf("close failed\n");
        return;
    }
    printf("closed\n");
}

inline void _sleepAndRead(int fd, struct timespec tim)
{
    struct timespec tim2;
    int result = 0;
    ssize_t numBytes;
    char *string;
    
    string = malloc(11 *sizeof(char));
    if (!string) {
        printf("malloc failed\n");
        return;
    }
    
    nanosleep(&tim, &tim2);

    printf("reading\n");
    numBytes = read(fd, string, 11);
    if (numBytes == -1) {
        printf("read returned -1\n");
    }
    else {
        if ((size_t)numBytes < 11) {
            printf("read did not complete\n");
        }
        printf("read returned %zd\n", numBytes);
    }
    
    printf("closing\n");
    result = close(fd);
    if (result) {
        printf("close failed\n");
        goto finish;
    }
    printf("closed\n");

finish:
    if (string) {
        free(string);
    }
}

inline int _openAndClose(const char *path)
{
    int fd = -1;
    int result = 0;
    
    printf("opening\n");
    fd = open(path, O_RDWR|O_NONBLOCK);
    if (fd == -1) {
        printf("open failed\n");
        return -1;
    }
    printf("opened\n");
    
    sleep(3);
    
    printf("closing\n");
    result = close(fd);
    if (result) {
        printf("close failed\n");
        return -1;
    }
    printf("closed\n");
    
    return 0;
}

#pragma mark -

// open and close the serial connection
int testOpenClose(const char *path)
{
    int fd = -1;
    int result = 0;
    
    fd = open(path, O_RDWR|O_NONBLOCK);
    if (fd == -1) {
        return -1;
    }
    
    result = close(fd);
    if (result) {
        return -1;
    }
    
    return 0;
}

// uses ioctl() and fcntl() to modify config of the tty session
int testModifyConfig(const char *path)
{
    int fd = -1;
    int result = 0;
    speed_t speed;
    
    struct termios originalTTYAttrs;
    
    fd = open(path, O_RDWR|O_NOCTTY|O_NONBLOCK);
    if (fd == -1) {
        printf("[FAIL] testModifyConfig: open failed\n");
        goto fail;
    }
    
    if (_modifyAttributes(fd, &originalTTYAttrs)) {
        printf("[FAIL] testModifyConfig: config failed\n");
        goto fail;
    }
    
    if (_modifyModemLines(fd)) {
        printf("[FAIL] testModifyConfig: config failed\n");
        goto fail;
    }
    
    // testing for some arbitrary non-standard values
    speed = 40000;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[WARN] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
    }
    speed = 58000;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[WARN] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
    }
    speed = 250000;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[WARN] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
    }
    speed = 10400;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[WARN] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
    }
    speed = 8192;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[WARN] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
    }
    speed = 128000;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[WARN] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
    }
    
    // 31250: standard rate used for MIDI signaling, but isn't included in POSIX
    speed = 31250;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[WARN] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
    }
    
    // some standard values
    speed = 38400;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[FAIL] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
        goto fail;
    }
    speed = 115200;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[FAIL] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
        goto fail;
    }
    speed = 19200;
    if (_testIOSSIOSPEEDIoctl(fd, speed)) {
        printf("[FAIL] testModifyConfig: IOSSIOSPEED ioctl with %lu failed\n", speed);
        goto fail;
    }
    
    // resets tty attributes
    tcsetattr(fd, TCSANOW, &originalTTYAttrs);
    
    result = close(fd);
    if (result == -1) {
        printf("[FAIL] testModifyConfig: close failed\n");
        goto fail;
    }
    
    return 0;
    
fail:
    if (fd != -1) close(fd);
    return -1;
}

#define kHelloWorldString    "Hello World"

int testReadWrite(const char *readPath, const char *writePath, const char *message)
{
    int readFd = -1;
    int writeFd = -1;
    int result = 0;
    ssize_t numBytes;
    
    char myMessage[256];
    char buffer[256];    // Input buffer
    char *bufPtr;        // Current char in buffer
    
    struct termios originalReadTTYAttrs;
    struct termios originalWriteTTYAttrs;
    
    if (!message) {
        // no message argument, using default value
        if (strlcpy(myMessage, kHelloWorldString, sizeof(myMessage)) >=
            sizeof(myMessage)) {
            // message argument is too long, got truncated
            myMessage[sizeof(myMessage)-1] = '\0';
        }
    }
    else {
        if (strlcpy(myMessage, message, sizeof(myMessage)) >= sizeof(myMessage)) {
            // message argument is too long, got truncated
            myMessage[sizeof(myMessage)-1] = '\0';
        }
    }
    
    readFd = open(readPath, O_RDWR|O_NOCTTY|O_NONBLOCK);
    if (readFd == -1) {
        printf("[FAIL] testReadWrite: open failed\n");
        goto fail;
    }
    if (_modifyAttributes(readFd, &originalReadTTYAttrs)) {
        printf("[FAIL] testReadWrite: _modifyAttributes failed\n");
        goto fail;
    }
    
    writeFd = open(writePath, O_RDWR|O_NOCTTY|O_NONBLOCK);
    if (writeFd == -1) {
        printf("[FAIL] testReadWrite: open failed\n");
        goto fail;
    }
    if (_modifyAttributes(writeFd, &originalWriteTTYAttrs)) {
        printf("[FAIL] testReadWrite: _modifyAttributes failed\n");
        goto fail;
    }
    
    if (_modifyModemLines(readFd) ||
        _modifyModemLines(writeFd)) {
        printf("[FAIL] testReadWrite: _modifyModemLines failed\n");
        goto fail;
    }
    
    numBytes = write(writeFd, myMessage, strnlen(myMessage, 256));
    
    if (numBytes == -1) {
        printf("[FAIL] write returned -1\n");
        goto fail;
    }
    if ((size_t)numBytes < strnlen(myMessage, 256)) {
        printf("[FAIL] write did not complete\n");
        goto fail;
    }
    
    memset(buffer, 0, sizeof(buffer));
    
    bufPtr = buffer;
    do {
        numBytes = read(readFd, bufPtr, &buffer[sizeof(buffer)] - bufPtr - 1);
        if (numBytes > 0) {
            bufPtr += numBytes;
            if (*(bufPtr - 1) == '\n' || *(bufPtr - 1) == '\r') {
                break;
            }
        }
    } while (numBytes > 0);
    
    if (strncmp(buffer, myMessage, sizeof(myMessage))) {
        printf("[FAIL] testReadWrite: read string was: \"%s\", "
               "expected: \"%s\"\n", buffer, myMessage);
        goto fail;
    }
    
    tcsetattr(readFd, TCSANOW, &originalReadTTYAttrs);
    tcsetattr(writeFd, TCSANOW, &originalWriteTTYAttrs);
    
    result = close(readFd);
    readFd = -1;
    if (result == -1) {
        printf("[FAIL] testReadWrite: close failed\n");
        goto fail;
    }
    result = close(writeFd);
    writeFd = -1;
    if (result == -1) {
        printf("[FAIL] testReadWrite: close failed\n");
        goto fail;
    }
    
    return 0;
    
fail:
    if (readFd != -1) close(readFd);
    if (writeFd != -1) close(writeFd);
    return -1;
}

int testOpenReenumerate(const char *path, const char *deviceid)
{
//    const char *path = "/dev/cu.usbserial-A6008hCW";
//    const char *deviceid = "0x403,0x6001";
    
    int pid;
    struct stat file_stat;
    int exists = 0;
    
    struct timespec tim[34];
    
    // Use different delay values, from 0s to 1.6s, with 50ms increments
    for (int i=0; i<20; i++) {
        tim[i].tv_sec = 0;
        tim[i].tv_nsec = i*50*1000*1000;
    }
    for (int i=0; i<14; i++) {
        tim[20+i].tv_sec = 1;
        tim[20+i].tv_nsec = i*50*1000*1000;
    }
    
    int i = 0;
    while (1) {
        printf("  testOpen: i: %d\n", i);
        // check presence of device node
        if (!(exists = stat(path, &file_stat) == 0)) {
            printf("file does not exist\n");
            sleep(3);
            continue;
        }
        
        pid = fork();
        if (pid == 0) {
            // child
            _sleepAndReenumerate(deviceid, tim[i]);
        }
        else if (pid > 0) {
            //parent
            _openAndClose(path);
            wait(NULL);
            printf("[PASS] testOpen\n");
        }
        sleep(4);
        i++;
        if (i >= 34) {
            break;
        }
    }
    
    return 0;

}

int testCloseReenumerate(const char *path, const char *deviceid)
{
    //    const char *path = "/dev/cu.usbserial-A6008hCW";
    //    const char *deviceid = "0x403,0x6001";
    
    int pid;
    struct stat file_stat;
    int exists = 0;
    
    struct timespec tim[20];
    
    // Use different delay values, from 2.5ms to 7.5ms, with 250us increments
    for (int i=0; i<20; i++) {
        tim[i].tv_sec = 0;
        tim[i].tv_nsec = 25*100*1000 + i*250*1000;
    }
    
    int i = 0;
    while (1) {
        int fd;
        printf("  testClose: i: %d\n", i);
        // check presence of device node
        if (!(exists = stat(path, &file_stat) == 0)) {
            printf("file does not exist\n");
            sleep(3);
            continue;
        }
        
        printf("opening i: %d\n", i);

        fd = open(path, O_RDWR|O_NOCTTY|O_NONBLOCK);
        if (fd == -1) {
            printf("open failed\n");
            return -1;
        }
        printf("opened\n");
        
        ioctl(fd, TIOCEXCL);
        fcntl(fd, F_SETFL, 0);
        int handshake;
        handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
        ioctl(fd, TIOCMSET, &handshake);

        
        pid = fork();
        if (pid == 0) {
            // child
            execlp("/AppleInternal/Applications/USB Prober.app/Contents/Resources/reenumerate",
                   "reenumerate", "-v", deviceid,
                   NULL);
        }
        else if (pid > 0) {
            //parent
            _sleepAndClose(fd, tim[i]);
            wait(NULL);
            printf("[PASS] testClose\n");
        }
        sleep(3);
        i++;
        if (i >= 20) {
            break;
        }
    }
    
    return 0;
    
}

int testWriteReenumerate(const char *path, const char *deviceid)
{
    //    const char *path = "/dev/cu.usbserial-A6008hCW";
    //    const char *deviceid = "0x403,0x6001";
    
    int pid;
    struct stat file_stat;
    int exists = 0;
    
    struct timespec tim[24];
    
    // Use different delay values, from 5ms to 8ms, with 125us increments
    for (int i=0; i<24; i++) {
        tim[i].tv_sec = 0;
        tim[i].tv_nsec = 5*1000*1000 + i*125*1000;
    }
    
    int i = 0;
    while (1) {
        int fd;
        printf("  testWrite: i: %d\n", i);
        // check presence of device node
        if (!(exists = stat(path, &file_stat) == 0)) {
            printf("file does not exist\n");
            sleep(3);
            continue;
        }
        
        printf("opening i: %d\n", i);
        
        fd = open(path, O_RDWR|O_NOCTTY|O_NONBLOCK);
        if (fd == -1) {
            printf("open failed\n");
            return -1;
        }
        printf("opened\n");
        
        ioctl(fd, TIOCEXCL);
        fcntl(fd, F_SETFL, 0);
        int handshake;
        handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
        ioctl(fd, TIOCMSET, &handshake);
        
        pid = fork();
        if (pid == 0) {
            // child
            execlp("/AppleInternal/Applications/USB Prober.app/Contents/Resources/reenumerate",
                   "reenumerate", "-v", deviceid,
                   NULL);
        }
        else if (pid > 0) {
            //parent
            _sleepAndWrite(fd, tim[i]);
            wait(NULL);
            printf("[PASS] testWrite\n");
        }
        sleep(3);
        i++;
        if (i >= 24) {
            break;
        }
    }
    
    return 0;
    
}

int testReadReenumerate(const char *writepath, const char *readpath, const char *locationid)
{
    //    const char *path = "/dev/cu.usbserial-A6008hCW";
    //    const char *deviceid = "0x403,0x6001";
    
    int pid;
    struct stat file_stat;
    int exists = 0;
    int writefd;
    
    struct timespec tim[20];
    
    // Use different delay values, from 3 to 9ms
    for (int i=0; i<20; i++) {
        tim[i].tv_sec = 0;
        tim[i].tv_nsec = 3*1000*1000 + i*300*1000;
    }
    
    writefd = open(writepath, O_RDWR|O_NOCTTY|O_NONBLOCK);
    if (writefd == -1) {
        printf("open failed: writefd\n");
        return -1;
    }
    
    ioctl(writefd, TIOCEXCL);
    fcntl(writefd, F_SETFL, 0);
    int handshake;
    handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
    ioctl(writefd, TIOCMSET, &handshake);

    dispatch_queue_t queue = dispatch_queue_create("myqueue", DISPATCH_QUEUE_CONCURRENT);
    
    int i = 0;
    while (1) {
        int readfd;
        printf("  testRead: i: %d\n", i);
        // check presence of device node
        if (!(exists = stat(readpath, &file_stat) == 0)) {
            printf("file does not exist\n");
            sleep(3);
            continue;
        }
        
        printf("opening i: %d\n", i);
        readfd = open(readpath, O_RDWR|O_NOCTTY|O_NONBLOCK);
        if (readfd == -1) {
            printf("open failed\n");
            return -1;
        }
        printf("opened\n");
        
        ioctl(readfd, TIOCEXCL);
        fcntl(readfd, F_SETFL, 0);
        int handshake;
        handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
        ioctl(readfd, TIOCMSET, &handshake);
        
        dispatch_async(queue, ^{
            write(writefd, "Hello World", 11);
        });
        
        sleep(1); // wait for write
        
        pid = fork();
        if (pid == 0) {
            // child
            execlp("/AppleInternal/Applications/USB Prober.app/Contents/Resources/reenumerate",
                   "reenumerate", "-v", "-l", locationid,
                   NULL);
        }
        else if (pid > 0) {
            //parent
            _sleepAndRead(readfd, tim[i]);
            wait(NULL);
            printf("[PASS] testRead\n");
        }
        sleep(3);
        i++;
        
        if (i >= 20) {
            break;
        }
    }
    
    dispatch_barrier_async(queue, ^{
        dispatch_suspend(queue);
        dispatch_release(queue);
    });

    return 0;
    
}

int testWriteClose(const char *path)
{
    //    const char *path = "/dev/cu.usbserial-A6008hCW";
    //    const char *deviceid = "0x403,0x6001";
    
    struct stat file_stat;
    int exists = 0;
    
    struct timespec tim[20];
    
    dispatch_queue_t queue;

    // Use different delay values
    for (int i=0; i<20; i++) {
        tim[i].tv_sec = 0;
        tim[i].tv_nsec = i*800;
    }

    queue = dispatch_queue_create("queue", DISPATCH_QUEUE_SERIAL);
    
    int i = 0;
    while (1) {
        int fd;
        printf("  testWriteClose: i: %d\n", i);
        // check presence of device node
        if (!(exists = stat(path, &file_stat) == 0)) {
            printf("file does not exist\n");
            sleep(3);
            continue;
        }
        
        printf("opening i: %d\n", i);
        
        fd = open(path, O_RDWR|O_NOCTTY|O_NONBLOCK);
        if (fd == -1) {
            printf("[FAIL] open failed\n");
            return -1;
        }
        printf("opened\n");
        
        ioctl(fd, TIOCEXCL);
        fcntl(fd, F_SETFL, 0);
        int handshake;
        handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
        ioctl(fd, TIOCMSET, &handshake);

        struct termios options;
        struct termios originalOptions;
        // snapshot the current terminal state in originalOptions
        if (tcgetattr(fd, &originalOptions) == -1) {
            printf("[FAIL] _modifyAttributes: tcgetattr failed\n");
            return -1;
        }
        options = originalOptions;
        options.c_cflag |= (CLOCAL);
        // Cause the new options to take effect immediately.
        if (tcsetattr(fd, TCSANOW, &options) == -1) {
            printf("[FAIL] _modifyAttributes: tcsetattr failed\n");
            return -1;
        }
        
        struct timespec tim1 = tim[i];
        dispatch_async(queue, ^{
            printf("writing\n");
            
            ssize_t numBytes;
            numBytes = write(fd, "Hello World", strnlen("Hello World", 256));
            if (numBytes == -1) {
                printf("write returned -1\n");
                return;
            }
            else if ((size_t)numBytes < strnlen("Hello World", 256)) {
                printf("write did not complete\n");
            }
            
            printf("write returned %zd\n", numBytes);
        });

        struct timespec tim2;
        nanosleep(&tim1, &tim2);
        int result = close(fd);
        if (result) {
            printf("[FAIL] closing: failed\n");
            return -1;
        }
        printf("closing: closed\n");

        sleep(1);
        
        dispatch_sync(queue, ^{
            // do nothing; this just makes sure the write block is complete.
        });
        
        i++;
        if (i >= 20) {
            break;
        }
    }
    
    dispatch_barrier_async(queue, ^{
        dispatch_suspend(queue);
        dispatch_release(queue);
    });
    
    printf("[PASS] testWriteClose\n");
    return 0;
}

int testReadClose(const char *writepath, const char *readpath)
{
    //    const char *path = "/dev/cu.usbserial-A6008hCW";
    //    const char *deviceid = "0x403,0x6001";
    
    struct stat file_stat;
    int exists = 0;
    int writefd;
    
    struct timespec tim[20];
    
    dispatch_queue_t queue;
    
    // Use different delay values
    for (int i=0; i<20; i++) {
        tim[i].tv_sec = 0;
        tim[i].tv_nsec = i*3*1000;
    }
    
    if (!(exists = stat(writepath, &file_stat) == 0)) {
        printf("file does not exist\n");
        sleep(3);
        return -1;
    }
    
    writefd = open(writepath, O_RDWR|O_NOCTTY|O_NONBLOCK);
    if (writefd == -1) {
        printf("[FAIL] open failed\n");
        return -1;
    }
    
    ioctl(writefd, TIOCEXCL);
    fcntl(writefd, F_SETFL, 0);
    int handshake;
    handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
    ioctl(writefd, TIOCMSET, &handshake);
    
    struct termios options;
    struct termios originalOptions;
    // snapshot the current terminal state in originalOptions
    if (tcgetattr(writefd, &originalOptions) == -1) {
        printf("[FAIL] _modifyAttributes: tcgetattr failed\n");
        return -1;
    }
    options = originalOptions;
    options.c_cflag |= (CLOCAL);
    // Cause the new options to take effect immediately.
    if (tcsetattr(writefd, TCSANOW, &options) == -1) {
        printf("[FAIL] _modifyAttributes: tcsetattr failed\n");
        return -1;
    }
    
    queue = dispatch_queue_create("queue", DISPATCH_QUEUE_SERIAL);
    
    int i = 0;
    while (1) {
        int readfd;
        printf("  testReadClose: i: %d\n", i);
        // check presence of device node
        if (!(exists = stat(readpath, &file_stat) == 0)) {
            printf("file does not exist\n");
            sleep(3);
            continue;
        }
        
        printf("opening i: %d\n", i);
        
        readfd = open(readpath, O_RDWR|O_NOCTTY|O_NONBLOCK);
        if (readfd == -1) {
            printf("[FAIL] open failed\n");
            return -1;
        }
        printf("opened\n");
        
        ioctl(readfd, TIOCEXCL);
        fcntl(readfd, F_SETFL, 0);
        int handshake;
        handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
        ioctl(readfd, TIOCMSET, &handshake);
        
        struct termios options;
        struct termios originalOptions;
        // snapshot the current terminal state in originalOptions
        if (tcgetattr(readfd, &originalOptions) == -1) {
            printf("[FAIL] _modifyAttributes: tcgetattr failed\n");
            return -1;
        }
        options = originalOptions;
        options.c_cflag |= (CLOCAL);
        // Cause the new options to take effect immediately.
        if (tcsetattr(readfd, TCSANOW, &options) == -1) {
            printf("[FAIL] _modifyAttributes: tcsetattr failed\n");
            return -1;
        }

        sleep(1);
        
        dispatch_async(queue, ^{
            write(writefd, "Hello World", 11);
        });
        sleep(1);

        dispatch_async(queue, ^{
            printf("reading\n");
            
            char buf[20];
            ssize_t numBytes;
            numBytes = read(readfd, buf, strnlen("Hello World", 256));
            if (numBytes == -1) {
                printf("read returned -1\n");
                return;
            }
            else if ((size_t)numBytes < strnlen("Hello World", 256)) {
                printf("read did not complete\n");
            }
            
            printf("read returned %zd\n", numBytes);
        });
        
        struct timespec tim1 = tim[i];
        struct timespec tim2;
        nanosleep(&tim1, &tim2);
        int result = close(readfd);
        if (result) {
            printf("[FAIL] closing: failed\n");
            return -1;
        }
        printf("closing: closed\n");
        
        sleep(1);
        
        dispatch_sync(queue, ^{
            // do nothing; this just makes sure the read block and write block completed.
        });
        
        i++;
        if (i >= 20) {
            break;
        }
    }
    
    dispatch_barrier_async(queue, ^{
        dispatch_suspend(queue);
        dispatch_release(queue);
    });
    
    printf("[PASS] testReadClose\n");
    return 0;
}

