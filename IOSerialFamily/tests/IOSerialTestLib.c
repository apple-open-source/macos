
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <IOKit/serial/ioss.h>

#include "IOSerialTestLib.h"

int _testIOSSIOSPEEDIoctl(int fd, speed_t speed);
int _modifyAttributes(int fd, struct termios *originalOptions);
int _modifyModemLines(int fd);

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

