

#ifndef IOSerialFamily_Header_h
#define IOSerialFamily_Header_h

int testOpenClose(const char *path);
int testModifyConfig(const char *path);
int testReadWrite(const char *readPath, const char *writePath, const char *message);

#endif
