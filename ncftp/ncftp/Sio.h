/* sio.h */

#define kTimeoutErr (-2)
#define kBrokenPipeErr (-3)

typedef void (*sio_sigproc_t)(int);
typedef volatile sio_sigproc_t vsio_sigproc_t;

int Sread(int, char *, size_t, int, int);
int Swrite(int, char *, size_t, int);
