#ifdef NEXT_PDO
#ifdef _WIN32
#include "hash-winntpdo.h"	/* adds WIN32 gunk  */
#else
#include "hash-pdo.h"		/* should have no NeXT extensions or WIN32 gunk
				   for now, it's a synonym for hash-next.h  */
#endif
#else
#include "hash-next.h"		/* adds NeXT extensions  */
#endif
