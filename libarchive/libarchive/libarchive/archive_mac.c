#include "archive.h"
#include "archive_platform.h"
#include "archive_mac.h"
#include "archive_read_private.h"

#ifdef HAVE_MAC_QUARANTINE
#include <quarantine.h>

void archive_read_get_quarantine_from_fd(struct archive *a, int fd)
{
	struct archive_read *ar = (struct archive_read *)a;
	qtn_file_t qf = qtn_file_alloc();
	if (qf) {
		if (!qtn_file_init_with_fd(qf, fd)) {
			ar->qf = qf;
		} else {
			qtn_file_free(qf);
		}
	}
}
#endif // HAVE_MAC_QUARANTINE
