/* Xscope xtrans layer */

#include "config.h"

extern short debuglevel;
#define DEBUG ((debuglevel & 4) ? 4 : 1)
#define TRANS_CLIENT
#define TRANS_SERVER
#define X11_t
#include <X11/Xtrans/transport.c>
