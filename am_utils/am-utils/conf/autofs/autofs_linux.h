#ifdef HAVE_FS_AUTOFS

struct autofs_pending_mount {
  unsigned long wait_queue_token;	/* Associated kernel wait token */
  char *name;
  struct autofs_pending_mount *next;
};

typedef struct {
  int fd;
  int kernelfd;
  int ioctlfd;
  int version;
  struct autofs_pending_mount *pending;
} autofs_fh_t;

typedef void * autofs_data_t;

#ifndef HAVE_LINUX_AUTO_FS4_H
union autofs_packet_union {
	struct autofs_packet_hdr hdr;
	struct autofs_packet_missing missing;
	struct autofs_packet_expire expire;
};
#endif /* not HAVE_LINUX_AUTO_FS4_H */

#endif /* HAVE_FS_AUTOFS */
