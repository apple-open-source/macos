/* Complete.h */

typedef struct _DirCache DirCache;
void InitReadline(void);
DirCache* CompleteStart(char *);
void CompleteSetFlags(char *s);
void CompleteParse(char *s);
void CompleteFinish(void);
char * CompleteGet(char *s, int off);
void CompleteOptions(char *s, int off);
void ClearDirCache(void);

#define kCompleteCmd 0
#define kCompleteFile 1
#define kCompleteDir 2
#define kCompleteLocal 3
#define kCompleteHost 4
#define kCompleteOption 5
#define kCompleteNone 6
