#include <unistd.h>

int getdtablesize() {
    return sysconf(_SC_OPEN_MAX);
}
