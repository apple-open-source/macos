

#ifndef IOSerialFamily_Header_h
#define IOSerialFamily_Header_h

int testOpenClose(const char *path);
int testModifyConfig(const char *path);
int testReadWrite(const char *readPath, const char *writePath, const char *message);
int testOpenReenumerate(const char *path, const char *deviceid);
int testCloseReenumerate(const char *path, const char *deviceid);
int testWriteReenumerate(const char *path, const char *deviceid);
int testReadReenumerate(const char *writepath, const char *readpath, const char *locationid);

int testWriteClose(const char *path);
int testReadClose(const char *writepath, const char *readpath);

#endif
