/* Put.h */

#ifndef _xfer_h_
#include "Xfer.h"
#endif

long AsciiGetLocalProc(char *, size_t, XferSpecPtr);
long AsciiPutRemoteProc(char *, size_t, XferSpecPtr);
int BinaryPut(char *, int, char *, long);
int AsciiPut(char *, int, char *, long);
void GetLocalSendFileName(char *, char *, size_t);
int OpenLocalSendFile(char *, long *);
int PutCmd(int, char **);
int CreateCmd(int, char **);
