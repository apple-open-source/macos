#include <Security/cssmapple.h>

//#define DLGUID "{0000fade-0002-0003-0102030405060708}"
#define DLGUID gGuidAppleFileDL

#define DBNAME1 "dbname1.db"

extern void dltests(bool autoCommit);
void dumpDb(char *dbName, bool printSchema);

extern const CSSM_GUID* gSelectedFileGuid;
