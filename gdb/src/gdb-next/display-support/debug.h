/* debug.h created by rhagy on Fri 21-Apr-1995 */

#import <stdio.h>
#import <Foundation/NSLock.h>

extern FILE	*debug_stream;


#ifdef DEBUG

#ifdef DEBUG_MAIN
NSLock	*printLock;
#else
extern NSLock	*printLock;
#endif

# define DEBUG_INIT		\
	printLock = [[NSLock alloc] init];

# define DEBUG_PRINT(s)			\
	[printLock lock];			\
	fprintf(debug_stream, "dbg: ");	\
	fprintf(debug_stream, (s) );	\
	fflush(debug_stream);		\
	[printLock unlock];

#else
# define DEBUG_ALLOC
# define DEBUG_INIT
# define DEBUG_PRINT(s)
#endif


