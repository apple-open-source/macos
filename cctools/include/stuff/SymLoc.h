#include "bool.h"

extern const char * symLocForDylib(
    const char *installName,
    const char *releaseName,
    enum bool *found_project,
    enum bool disablewarnings);

extern const char * dstLocForDylib(
    const char *installName,
    const char *releaseName,
    enum bool *found_project,
    enum bool disablewarnings);

const char * LocForDylib(
    const char *installName,
    const char *releaseName,
    const char *dirname,
    enum bool *found_project,
    enum bool disablewarnings);
