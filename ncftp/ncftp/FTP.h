/* FTP.h */

#ifndef _ftp_h_
#define _ftp_h_ 1

/* Error types from OpenControlConnection() */
#define kConnectNoErr			0
#define kConnectErrFatal		(-1)
#define kConnectErrReTryable	(-2)

/* Types of FTP server software. */
#define kUnknownFTPd 0
#define kGenericFTPd	1
#define kWuFTPd	2
#define kNcFTPd	3

/* Parameter for OpenDataConnection() */
#define kSendPortMode			0
#define kPassiveMode			1
#define kFallBackToSendPortMode	2

/* Parameter for AcceptDataConnection() */
#define kAcceptForWriting		00100
#define kAcceptForReading		00101

#define kDefaultFTPPort 21

/* To check if the user set the port. */
#define kPortUnset 0

#ifndef INADDR_NONE
#	define INADDR_NONE (0xffffffff)		/* <netinet/in.h> should have it. */
#endif

typedef void (*HangupProc)(void);

void InitDefaultFTPPort(void);
void MyInetAddr(char *dst, size_t siz, char **src, int i);
int GetOurHostName(char *host, size_t siz);
void CloseControlConnection(void);
void SetTypeOfService(int sockfd, int tosType);
void SetLinger(int sockfd);
void SetInlineOutOfBandData(int sockfd);
int OpenControlConnection(char *host, unsigned int port);
void CloseDataConnection(int);
int OpenDataConnection(int mode);
int AcceptDataConnection(void);
void SetPostHangupOnServerProc(HangupProc proc);
void HangupOnServer(void);
void SendTelnetInterrupt(void);
int SetStartOffset(long);

#endif	/* _ftp_h_ */
