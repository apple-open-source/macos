/* Glob.h */

#define kGlobChars "[*?"
#define GLOBCHARSINSTR(a) (strpbrk(a, kGlobChars) != NULL)

int RGlobCmd(int, char **);
void RemoteGlob(LineListPtr fileList, char *pattern, char *lsFlags);
void ExpandTilde(char *pattern, size_t siz);
void LocalGlob(LineListPtr fileList, char *pattern);
