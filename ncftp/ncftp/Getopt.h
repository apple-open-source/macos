/* Getopt.h */

#define kGetoptBadChar   ((int)'?')
#define kGetoptErrMsg    ""

void GetoptReset(void);
int Getopt(int nargc, char **nargv, char *ostr);
