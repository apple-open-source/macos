#include <mach/mach.h>

#include "defs.h"
#include "breakpoint.h"
#include "gdbcmd.h"

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-inferior-util.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-cfm.h"
#include "nextstep-nat-cfm-io.h"
#include "nextstep-nat-cfm-process.h"

static void check_read (const char *name, CORE_ADDR address, CORE_ADDR length)
{
  static CORE_ADDR read[1024];
  static unsigned int nread = 0;
  
  unsigned int i;

  for (i = 0; i < nread; i++) {
    if (read[i] == address) {
      return;
    }
  }

  if (nread >= 1024) {
    internal_error ("check_read table overflow");
    return;
  }

  read[nread++] = address;

  pef_read (name, address, length);
}

void next_update_cfm (void)
{
  char buf[258];

  CFMClosure *closure = gClosures;
  while (closure != NULL) {

    CFMConnection *connection = closure->mConnections;

    while (connection != NULL) {

      CFMContainer *container = connection->mContainer;
      CFMSection *section = container->mSections;
      size_t secnum = 0;

      sprintf (buf, "%.*s", container->mContainer.name[0], &container->mContainer.name[1]);
      check_read (buf, (unsigned long) container->mContainer.address,
		  (unsigned long) container->mContainer.length);
      
      while (section != NULL) {
	secnum++;
	section = section->mNext;
      }
      connection = connection->mNext;
    }
    closure = closure->mNext;
  }
}
