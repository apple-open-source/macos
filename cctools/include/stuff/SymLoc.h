#include "bool.h"

extern const char * symLocForDylib(
    const char *installName,
    const char *releaseName,
    enum bool *found_project,
    enum bool disablewarnings,
    enum bool no_error_if_missing);

extern const char * dstLocForDylib(
    const char *installName,
    const char *releaseName,
    enum bool *found_project,
    enum bool disablewarnings,
    enum bool no_error_if_missing);

const char * LocForDylib(
    const char *installName,
    const char *releaseName,
    const char *dirname,
    enum bool *found_project,
    enum bool disablewarnings,
    enum bool no_error_if_missing);
