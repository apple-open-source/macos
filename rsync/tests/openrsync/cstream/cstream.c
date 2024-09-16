/*
 * Copyright 1998, 1999, 2000, 2001 Martin Cracauer
 *
 * See the file COPYRIGHT in the distribution package for copying terms.
 */

#ifndef HAVE_STRUCT_ITIMERVAL
#error no itimerval
#define NOTIMER
#endif

#define WITH_RESOLV

/*
 * Control glibc, especially to get 64 bit file offsets
 */
#ifdef __linux
/*
 * Don't use this unless I have to:
 * #define _GNU_SOURCE
 */
/*
 * Let me see if I get that straight:
 * -first enable "some" additional calls
 */
#define _LARGEFILE_SOURCE
/*
 * -then enable the 64 bits variants of traditional calls, on seperate names
 */
#define _LARGEFILE64_SOURCE
/*
 * and now make the 64 bit variants shadow the old calls
 */
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/ioctl.h>
#if HAVE_UNISTD_H || CRAENV
#include <unistd.h>
#endif
#include <signal.h>
#if TIME_WITH_SYS_TIME || CRAENV
#include <sys/time.h>
#include <time.h>
#else /* TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif /* TIME_WITH_SYS_TIME */
#include <errno.h>

#include <sys/mman.h>

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef __linux
#define __USE_GNU
#endif
#include <fcntl.h>
#include <sys/types.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include <sys/stat.h>
#if HAVE_SYS_WAIT_H || CRAENV
#include <sys/wait.h>
#endif
#if HAVE_POLL_H
#include <poll.h>
#ifndef POLLRDNORM
#define POLLRDNORM 0
#endif
#ifndef POLLWRNORM
#define POLLWRNORM 0
#endif
#ifndef INFTIM
#define INFTIM -1
#endif
#endif /* HAVE_POLL_H */

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#ifndef NOSOUND

#ifdef HAVE_MACHINE_SOUNDCARD_H
#include <machine/soundcard.h>
#define WANT_SOUND 1
#else
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#define WANT_SOUND 1
#else
#ifdef HAVE_LINUX_SOUNDCARD_H
#include <linux/soundcard.h>
#define WANT_SOUND 1
#include <sys/ioctl.h>
#endif /* linux/soundcard.h */
#endif /* either soundcard.h */
#endif /* either soundcard.h */
#endif /* ndef NOSOUND */

#ifndef HAVE_SYS_SOCKET_H
#define NOTCP
#endif

#ifdef NOTCP
#define NORESOLV 1
#endif

#ifndef NOTCP
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifndef HAVE_NETDB_H
#define NORESOLV
#endif

/* Solaris needs this */
#ifndef	INADDR_NONE
#define	INADDR_NONE	0xffffffff
#endif

#ifndef NORESOLV
#include <netdb.h>
#endif

#if !defined(HAVE_SOCKLEN_T) && !defined(NORESOLV)
typedef u_int32_t socklen_t;
#endif

#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif

#endif /* NOTCP */

#ifdef O_SYNC
#  define MY_O_SYNC O_SYNC
#else
#  ifdef O_FSYNC
#    define MY_O_SYNC O_FSYNC
#  endif
#endif

#ifdef HAVE_LIBJACK
#include <jack/jack.h>
#endif

volatile sig_atomic_t stopit;
volatile sig_atomic_t signal_report;
volatile sig_atomic_t n_sighups;
volatile sig_atomic_t childpid = 0;

int pagesize = -1;

struct pmalloc {
  void *data;
  void *allocated;
  size_t n_bytes;
};
struct options {
  int v;
  long long n;
  int t;
  char *i;
  char *o;
  char *I;
  char *O;
  char *p;
  int l;
  int B;
  int c;
  int w;
  int S;
  int T;
  int user_specified_blocksize;
  int six;
};

void malloc_page_aligned(const struct options *o, struct pmalloc * pmalloc);

struct progstate {
  int b; /* Blocksize, to be changed to separate input and output sizes */
  int ifd;
  int ofd;
  long long bytes_transferred; /* How many bytes transferred so far */
  int bytes_firsttransfer; /* How many bytes were transferred at the
			    * first read/write pair? We need this for the
			    * extended throughput calculation. */
  long long bytes_lastreport; /* ... and since last report */
  double starttime; /* Time we began to try read/write */
  double lasttime; /* Last time we did a read/write */
  double time_firsttransfer; /* How much time did the first transfer took */
  double time_lastreport; /* How much time since the last report */
  long long n_lines;
  int pidfile_has_been_created;
  int pid;
  int teefd; /* Usually -1, but can be a file descriptor number to copy
	      * the stream to.  Currently that will always be 1 (stdout) */
  int using_o_direct;
  int using_o_direct_i; /* Input */
  struct pmalloc pmalloc; /* Size is b (blocksize) */
  off_t outputfile_position;
  char *outputfile_map;
};

#ifndef NOTIMER 
static void
sigtimer(const int signal)
{
  signal_report = 1;
}
#endif

static void
sigshutdown(const int signal)
{
  stopit = 1;
}

#if 0
/* Disable that for now */
static void
sigchld(const int sig)
{
  int status;
  int ret;


  fprintf(stderr, "sigchld\n");
  /*
   * In theory we shouldn't have to use nohang, but Linux seems to send
   * SIGCHLD when nothing is available.
   */
  ret = waitpid(0, &status, WNOHANG | WUNTRACED);
  if (ret == -1) {
#ifdef __linux
    /*
     * Linux seems to get sigchld without anyone to report on SIGSTOP
     * Screwy stuff.  This noticed for kernel 2.4.6.  Why do I never run
     * into such stuff with FreeBSD.
     */
    fprintf(stderr, "WARNING: ");
#endif

    fprintf(stderr, "cstream pid %d waiting for %d: ", getpid(), childpid);
    perror(NULL);
#ifndef __linux
    exit(1);
#else
    return;
#endif
  }
  if (ret != 0 && !WIFSTOPPED(status)) {
    signal(SIGCHLD, SIG_DFL);
    childpid = 0;
    sigshutdown(sig);
  }
}
#endif

static void
sigreport(int signal)
{
  signal_report = 1;
}

static void
sighup(int signal)
{
  const ssize_t meh = write(2, "SIGHUP\n", sizeof("SIGHUP\n") - 1);
  if (meh != sizeof("SIGHUP\n")) {
    /* ignore write error */
  }
  n_sighups++;
}

#ifdef SIGHUP
static void
handle_sighup(void)
{
  fprintf(stderr,
	  "Received SIGHUP. "
	  "If this is an error, send another one within 5 seconds\n");
  n_sighups = 1;
  sleep(5);
  if (n_sighups > 1) {
    n_sighups = 0;
    fprintf(stderr, "Continuing\n");
    return;
  }
  fprintf(stderr, "Exiting...\n");
  signal(SIGHUP, SIG_DFL);
  kill(getpid(), SIGHUP);
}
#endif

static struct options *
default_options(struct options *const o)
{
  bzero(o, sizeof(struct options));
  return o;
}

#ifdef HAVE_STRUCT_SIGACTION
static void
tsignal(const int sig, void (*const handler)(int))
{
  struct sigaction sa;

  sa.sa_handler = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(sig, &sa, NULL) == -1) {
    perror("sigaction");
    exit(2);
  }
}
#else
static void
tsignal(const int sig, void (*const handler)(int))
{
  if (signal(sig, handler) == SIG_ERR) {
    perror("signal");
    exit(2);
  }
}
#endif

#ifdef WANT_SOUND

#ifdef HAVE_LIBJACK

int jack_callback(jack_nframes_t nframes, void *arg)
{
  //fprintf(stderr, "Jackd wants %d samples\n", nframes);
  return 0;
}

void open_jack()
{
  jack_client_t *client;
  jack_status_t status;
  // TODO: use JackSessionID
  client = jack_client_open("cstream", JackNoStartServer, &status);
  if (client == 0) {
    fprintf(stderr, "Failed to open jackd 0x%X\n", status);
    exit(2);
  }
  jack_set_process_callback(client, jack_callback, 0);
  jack_port_t *port1;
  port1 = jack_port_register(client, "in_1", JACK_DEFAULT_AUDIO_TYPE
			    , JackPortIsOutput, 0);
  if (port1 == NULL) {
    fprintf(stderr, "can't register jack port\n");
    exit(2);
  }
  jack_port_t *port2;
  port2 = jack_port_register(client, "in_2", JACK_DEFAULT_AUDIO_TYPE
			    , JackPortIsOutput, 0);
  if (port2 == NULL) {
    fprintf(stderr, "can't register jack port\n");
    exit(2);
  }

  if (jack_activate (client)) {
    fprintf(stderr, "cannot activate jack client");
    exit(2);
  }

  jack_native_thread_t jack_thread;
  //jack_client_create_thread(client, &jack_thread, 0, 0, 

}
#endif

static void
setaudio(int fd, const char *spec)
{
  struct soundoptions {
    int so_format;
    int so_rate;
    int so_stereo;
  } so = {
    /* Default settings: CD quality */
    AFMT_S16_LE,
    44100,
    1
  };
  char *s;

  if ((s = getenv("CSTREAM_AUDIO_BITRATE"))) {
    so.so_rate = atol(s);
  }

  if (spec == NULL)
    spec = "-";

  if (strchr(spec, ':'))
    fprintf(stderr, "Warning: audio options will just be CD-quality "
	    "settings.\nFilespec parsing not implemented\n");

  if (ioctl(fd, SNDCTL_DSP_SETFMT, &(so.so_format)) == -1)
    fprintf(stderr, "icotl SNDCTL_DSP_SETFMT for '%s' failed: '%s'\n"
	    , spec, strerror(errno));
  if (ioctl(fd, SNDCTL_DSP_STEREO, &(so.so_stereo)) == -1)
    fprintf(stderr, "icotl SNDCTL_DSP_STEREO for '%s' failed: '%s'\n"
	    , spec, strerror(errno));
  if (ioctl(fd, SNDCTL_DSP_SPEED, &(so.so_rate)) == -1)
    fprintf(stderr, "icotl SNDCTL_DSP_SPEED for '%s' failed: '%s'\n"
	    , spec, strerror(errno));
}
#else /* WANT_SOUND */
static void
setaudio(int fd, const char *const spec)
{
  fprintf(stderr, "Sorry, don't have soundcard support compiled in, "
	  "no audio settings will be made.\nTrying to play anyway.\n");
}
#endif /* WANT_SOUND */

#ifdef NOTCP
static int
open_tcp(const struct options *const o, int mode)
{
  fprintf(stderr, "Sorry, TCP/IP socket support has not been compiled in.\n");
  exit(1);
}

#else /* NOTCP */

static void
print_inet(FILE *const f, const void *const raw)
{
  const unsigned char *c = raw;

  fprintf(stderr, "%d.%d.%d.%d", (int)*c, (int)*(c + 1)
	  , (int)*(c + 2), (int)*(c + 3));
}

// this construct's purpose is to always compile the old API
#ifdef HAVE_GETADDRINFO
static int use_getaddrlen = 1;
#else
static int use_getaddrlen = 0;
#endif

int gimme_a_socket(int domain, int type, int protocol)
{
  int fd;

  if ((fd = socket(domain, type, protocol)) == -1) {
    perror("socket");
    exit(2);
  }

  {
    int i = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) == -1) {
      perror("setsockopt(REUSEADDR), [continuing]");
    }
  }
  return fd;
}

static int
open_tcp(const struct options *const o, int mode)
{
  char *hostname;
  const char *port;
  const char *port_iterator;
  const char *spec;
  int fd = -1;
  int newfd = -1;
  struct sockaddr *use_this_serv_addr = NULL;
  socklen_t use_this_serv_addr_len = -1;

  if (mode == O_WRONLY) {
    spec = o->o;
  } else {
    spec = o->i;
  }

  port = NULL;
  for (port_iterator = strchr(spec, ':');
       port_iterator && *port_iterator;
       port_iterator = strchr(port_iterator, ':')) {
    port = port_iterator;
    port_iterator++;
  }
  port++;
  if (port == NULL) {
    fprintf(stderr, "Can't find port in IP Spec '%s'\n", spec);
    exit(1);
  }
  // this test is to allow connecting to (as a client) '::1:3333'
  // aka ipv6 localhost
  if (spec[1] != '\0' && spec[0] == ':' && spec[1] != ':') {
     /* Listen mode */
    if (o->six == -1) {
      /* fixme, pull in front, so that same code is useable for client */
      struct sockaddr_in serv_addr;
      use_this_serv_addr = (struct sockaddr *)&serv_addr;
      use_this_serv_addr_len = sizeof(serv_addr);
      bzero(&serv_addr, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_addr.s_addr = INADDR_ANY;
      serv_addr.sin_port = htons(atoi(port));

      fd = gimme_a_socket(AF_INET, SOCK_STREAM, 0);
    } else { // allow or force ipv6
      struct sockaddr_in6 sin6;
      bzero(&sin6, sizeof(sin6));
      use_this_serv_addr = (struct sockaddr *)&sin6;
      use_this_serv_addr_len = sizeof(sin6);
      // sin6.sin6_len = sizeof(sin6);
      sin6.sin6_family = AF_INET6;
      sin6.sin6_flowinfo = 0;
      sin6.sin6_port = htons(atoi(port));
      sin6.sin6_addr = in6addr_any;

      fd = gimme_a_socket(AF_INET6, SOCK_STREAM, 0);
    }
    if (o->v > 1 && o->six != -1) 
      fprintf(stderr, "Bind in IPV6 mode\n");
    if (bind(fd, (struct sockaddr *)use_this_serv_addr, 
             use_this_serv_addr_len) == -1) {
      perror("bind");
      exit(2);
    }

    if (listen(fd, 2) == -1) {
      perror("listen");
      exit(2);
    }

    if (o->v >= 1)
      fprintf(stderr, "Accepting on port %d\n", atoi(port));

    struct sockaddr addr_here;
    socklen_t addr_here_size;

    if ((newfd = accept(fd, &addr_here, &addr_here_size)) == -1) {
      fprintf(stderr, "errno: %d\n", errno);
      perror("accept");
      exit(2);
    }
    close(fd);
    fd = newfd;
  } else { /* Connect */

    hostname = strdup(spec);
    hostname[port - spec - 1] = '\0';

    if (o->v >= 2)
      fprintf(stderr, "Connecting to %s %s\n", hostname, port);

    struct sockaddr_in serv_addr;
    use_this_serv_addr = (struct sockaddr *)&serv_addr;
    use_this_serv_addr_len = sizeof(serv_addr);
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    serv_addr.sin_addr.s_addr = inet_addr(hostname);
    if (serv_addr.sin_addr.s_addr != INADDR_NONE) {
      // ipv4 address specified
      fd = gimme_a_socket(AF_INET, SOCK_STREAM, 0);
    } else {
#ifdef NORESOLV
      fprintf(stderr, "Not an IP Address and no resolver compiled in: %s\n"
	      ,hostname);
      exit(1);
#else
      if (o->v >= 2)
	fprintf(stderr, "Hostname lookup for '%s' use getaddrlen? %d\n", 
                hostname, use_getaddrlen);
      if (use_getaddrlen == 0) {
        struct in_addr **a;
        struct hostent *hostent;
        hostent = gethostbyname(hostname);
        if (hostent == NULL) {
          herror(hostname);
          exit(2);
        }
        a = (struct in_addr **)hostent->h_addr_list;
        bcopy(*a, &serv_addr.sin_addr.s_addr, sizeof(struct in_addr));
        if (o->v >= 2) {
          fprintf(stderr, "Cannonical name %s, type %s, prim addr: "
                  , hostent->h_name
                  , hostent->h_addrtype == AF_INET ? "ipv4" : "!ipv4");
          print_inet(stderr, &serv_addr.sin_addr.s_addr);
          fprintf(stderr, "\n");
        }
        fd = gimme_a_socket(AF_INET, SOCK_STREAM, 0);
      }
#ifdef HAVE_GETADDRINFO
      struct addrinfo *result_begin, *result, hints;
      int error;

      bzero(&hints, sizeof(hints));
      switch (o->six) {
      case -1: hints.ai_family = AF_INET; break;
      case 2: hints.ai_family = AF_INET6; break;
      default: // usually 1
        hints.ai_family = PF_UNSPEC; break;
      }
      hints.ai_socktype = SOCK_STREAM;

      error = getaddrinfo(hostname, port, &hints, &result_begin);
      if (error) {
        fprintf(stderr, "getaddrinfo: '%s'\n", gai_strerror(error));
        exit(3);
      }
      for (result = result_begin; result; result = result->ai_next) {
        if (result->ai_addr)
          break;
      }
      if (result == NULL) {
        fprintf(stderr, "empty getaddrinfo() result for '%s'\n", hostname);
        exit(3);
      }
      use_this_serv_addr = result->ai_addr;
      use_this_serv_addr_len = result->ai_addrlen;

      if (o->v > 1) {
        char *msg;
        switch (result->ai_family) {
        case AF_INET: msg = "ipv4"; break;
        case AF_INET6: msg = "ipv6"; break;
        default: msg = "not-IPv4/6"; break;
        }
        char name[8192];
        char addr_string[INET6_ADDRSTRLEN];
        // FIXME - use inet_ntop
        getnameinfo(use_this_serv_addr, use_this_serv_addr_len,
                    name, sizeof(name), NULL, 0, 0);
        if (inet_ntop(result->ai_family, &use_this_serv_addr, addr_string, 
                      use_this_serv_addr_len) == NULL) {
          strcpy(addr_string, "unknown");
        }
        fprintf(stderr, "Cannonical name '%s', type %s, prim addr: '%s'\n"
                , name
                , msg
                , addr_string);
      }

      fd = gimme_a_socket(result->ai_family, result->ai_socktype, 
                          result->ai_protocol);

#endif // HAVE_GETADDRINFO
    }             
    if (connect(fd, use_this_serv_addr, use_this_serv_addr_len) == -1) {
      perror("connect");
      exit(2);
    }
    free(hostname);
    // FIXME - free getaddrinfo result
#endif // host name lookup
}

#if defined(HAVE_GETNAMEINFO) && !defined(NORESOLV)
  if (o->v >= 2) {
    struct {
      int (*func)(int, struct sockaddr *, socklen_t *);
      char *name;
      char *text;
    } *it, funcs[3] = {
      {getsockname, "getsockname", "Local binding"},
      {getpeername, "getpeername", "Remote binding"},
      {NULL, NULL, NULL}
    };
    union {
      struct sockaddr sa;
      char data[8192];
    } un;
    socklen_t len;

    len = sizeof(un);
    for (it = funcs; *it->func; it++) {
      char hostname[8192];
      char service[8192];
      if (it->func(fd, &un.sa, &len) == -1) {
	perror(it->name);
	exit(2);
      }
      getnameinfo(&un.sa, len, hostname, sizeof(hostname)
		  , service, sizeof(service), NI_NUMERICHOST|NI_NUMERICSERV);
      fprintf(stderr, "%s %s:%s\n", it->text, hostname, service);
    }
  }
#endif /* HAVE_GETNAMEINFO && !NORESOLV*/
  return fd;
}
#endif /* else NOTCP */

int
get_fs_blocksize(const char *const filename, int flags, int mode)
{
      struct statvfs statfs;
      int fd;

      fd = open(filename, flags, mode);
      if (fd == -1) {
	perror("Cannot open input file (to get block size)");
	exit(2);
      }
      if (fstatvfs(fd, &statfs) == -1) {
	perror("Cannot get FS blocksize, you need to use -b<blocksize>\n");
	exit(2);
      }
      close(fd);
      return statfs.f_bsize;
}

// fixme, make this usable as open_input_file, too
void open_output_file(const struct options *const o
		      , struct progstate *const state, int flags)
{
  int mode = 0666;

  if (strchr(o->O, 'S')) {
#ifdef MY_O_SYNC
    flags |= MY_O_SYNC;
    if (o->v > 1) {
      fprintf(stderr, "Using O_SYNC on output file\n");
    }
#else
    fprintf(stderr, "Trying to use O_SYNC but this OS doesn't have it\n");
    exit(2);
#endif
  }

#ifdef HAVE_O_DIRECT
  if (state->using_o_direct) {
    flags |= O_DIRECT;
    if (o->v > 1) {
      fprintf(stderr, "Using O_DIRECT on output file with blocksize %d\n"
	      , state->b);
    }

    if (!o->user_specified_blocksize) {
#ifdef HAVE_SYS_STATVFS_H
#if 0
      // code that broke -B fixme
      tmp_blocksize = get_fs_blocksize(o->o, flags, mode);
      if (state->b && state->b != tmp_blocksize) {
	fprintf(stderr, "WARNING: blocksize set to %d, but output filesystem \n"
		"requires %d.  Continuing anyway with %d.\n"
		, state->b
		, tmp_blocksize
		, state->b);
      } else {
	state->b = tmp_blocksize;
      }
#else
      struct statvfs statfs;

      state->ofd = open(o->o, flags, mode);
      if (state->ofd == -1) {
	perror("Cannot open output file (to get block size)");
	exit(2);
      }
      if (fstatvfs(state->ofd, &statfs) == -1) {
	perror("Cannot get FS blocksize, you need to use -b<blocksize>\n");
	exit(2);
      }
      state->b = statfs.f_bsize;
      close(state->ofd);
#endif // broken code
#else
      fprintf(stderr, "This platform does not have statvfs, you need to "
	      "specify the blocksize manually with -b, it must match the "
	      "output filesystem blocksize\n");
      exit(2);
#endif
    }
  }
#endif

  state->ofd = open(o->o, flags, mode);
  if (state->ofd == -1) {
    perror("Cannot open output file");
    exit(2);
  }

  if (strchr(o->O, 'M')) { // use mmap instead of write, for hugetlbfs
    if (o->i == NULL || o->i[0] == '\0')
      fprintf(stderr, "mmap output requires that I know input file in advance\n");

    if (flags != (O_RDWR | O_CREAT | O_TRUNC)) {
      fprintf(stderr, "mmaped output mode expected open(2) flags %d, got %d (continuing anyway)\n", O_RDWR | O_CREAT | O_TRUNC, flags);
    }

    struct stat statbuf;
    if (fstat(state->ifd, &statbuf) == -1) {
      fprintf(stderr, "mmap output requires that I know input file size for '%s' fd %d", o->i, state->ifd);
      perror("stat");
      exit(2);
    }
    off_t filesize = statbuf.st_size;

    if (filesize % (2 * 1024 * 1024) != 0) {
      filesize = filesize / (2 * 1024 * 1024) + 1;
      filesize = filesize * 2 * 1024 * 1024;
    }

    if (filesize == 0) {
      fprintf(stderr, "output file mmap mode refusing to work on zero size file '%s'\n", o->i);
    }
    if (o->v > 1) {
      fprintf(stderr, "Using mmap for output file size %lld, open flags 0x%x\n", (long long)filesize, flags);
    }
#define USE_TRUNCATE

#ifdef USE_LSEEK
    if (lseek(state->ofd, filesize, SEEK_SET) == -1) {
      fprintf(stderr, "Something went wrong seeking to end for output file size %lld\n", (long long)filesize, o->o);
      perror("lseek");
      exit(2);
    }
#endif
    
#ifdef USE_WRITE
    if (lseek(state->ofd, filesize - 1, SEEK_SET) == -1) {
      perror("seek");
    }
    char buf[2] = "1";
    if (write(state->ofd, buf, 1) == -1) {
      perror("write");
      exit(2);
    }
#endif
#ifdef USE_TRUNCATE
    if (ftruncate(state->ofd, filesize) == -1) {
      perror("ftruncate for mapped output file (continuing)");
    }
#endif
    state->outputfile_position = 0;
    if ((state->outputfile_map = 
	 mmap(NULL, filesize, PROT_WRITE, MAP_SHARED, state->ofd, 0))
	== MAP_FAILED) {
      perror("mmap output file");
      exit(2);
    }
    if (o->v > 2) {
      fprintf(stderr, "mmaped from %p to %p, trying to write a byte... [expect \"worked\"]\n", state->outputfile_map, state->outputfile_map + filesize);
    }
    *(state->outputfile_map) = '\0';
    if (o->v > 2) {
      fprintf(stderr, "... worked\n");
    }
  }
}

static void
init(struct options *const o, struct progstate *const state
     , const int blocksize)
{
  struct timeval t;

  bzero(state, sizeof(*state));
  state->outputfile_position = -1; // signals that we just write(2)

  pagesize = getpagesize();

  if (o->I == NULL)
    o->I = "";
  if (o->O == NULL)
    o->O = "";

  if (strchr(o->O, 'D')) {
#ifdef HAVE_O_DIRECT
    state->using_o_direct = 1;
#else
    fprintf(stderr, "Support for O_DIRECT not compiled in\n");
    exit(1);
#endif
  }

  if (blocksize == 0) {
    state->b = 8192;
    if (o->B == 0)
      o->B = state->b;
  } else {
    state->b = blocksize;
    o->user_specified_blocksize = 1;
  }

#if 0
  // WTF?
  if (state->b == 0)
    state->b = state->b;
#endif

  if (strchr(o->I, 'D'))
    state->using_o_direct_i = 1;
  // fixme
  // need code to check that blocksize specified
  // works for both in and out

  if (o->w == 0)
    o->w = state->b;

  if (o->B && o->B < state->b) {
    fprintf(stderr, "-B must not be lower than -b or -n (%d/%d)\n"
	    , o->B, state->b);
    exit(1);
  }

  if (o->c < 0 || o->c > 4) {
    fprintf(stderr, "-c must must be 0, 1, 2, 3 or 4\n");
    exit(1);
  }

  if (o->i == NULL || o->i[0] == '\0')
    state->ifd = 0;
  else if (!strcmp(o->i, "-"))
    state->ifd = -1;
  else  {
    if (strchr(o->I, 'f')) {
      unlink(o->i);
      if (mkfifo(o->i, 0666) == -1) {
	perror("mkfifo() in");
	exit(2);
      }
      state->ifd = open(o->i, O_RDWR);
    }
    else {
      if (strchr(o->i, ':') && !strchr(o->I, 'N'))
	state->ifd = open_tcp(o, O_RDONLY);
      else {
	int flags = O_RDONLY;
	if (strchr(o->I, 'D')) {
#ifdef HAVE_O_DIRECT
	  flags |= O_DIRECT;
	  if (o->v > 1) {
	    fprintf(stderr, "Using O_DIRECT on input file with blocksize %d\n"
		    , state->b);
	  }
#else
	  fprintf(stderr, "O_DIRECT requested for input but not compiled in\n");
	  exit(1);
#endif
	}
	state->ifd = open(o->i, flags);
      }
    }
    if (state->ifd == -1) {
      fprintf(stderr, "Cannot open input file/tcpspec '%s': ", o->i);
      perror(NULL);
      exit(2);
    }
  }

  if (strchr(o->O, 't')) { /* Tee to fd mode */
    state->teefd = 3;
    if (write(state->teefd, "", 0) == -1) {
      fprintf(stderr, "stream copy to fd 3 requested, but fd3 is not open\n");
      fprintf(stderr, "Use from shell like this:\n"
	      "  cstream -O t 3> /tmp/file\n");
      exit(1);
    }
  }
  else
    state->teefd = -1;

  if (o->o == NULL || o->o[0] == '\0')
    state->ofd = 1;
  else if (!strcmp(o->o, "-"))
    state->ofd = -1;
  else {
    if (strchr(o->O, 'f')) { /* Fifo */
      unlink(o->o);
      if (mkfifo(o->o, 0666) == -1) {
	perror("mkfifo() out");
	exit(2);
      }
    }
    if (strchr(o->o, ':') && !strchr(o->O, 'N'))
      state->ofd = open_tcp(o, O_WRONLY);
    else {
      open_output_file(o, state, O_RDWR | O_CREAT | O_TRUNC); /* sets ofd */
    }
  }

  if (o->v >= 4)
    fprintf(stderr, "Files are open at fd %d/%d\n", state->ifd, state->ofd);

  if (strchr(o->I, 'a')) /* Audio in */
    setaudio(state->ifd, o->i);
  if (strchr(o->O, 'a')) /* Audio out */
    setaudio(state->ofd, o->o);

  if (o->c > 0 && (state->ofd == -1 || state->ifd == -1)) {
    fprintf(stderr, "Do not use -c > 0 if you generate or sink data.\n");
    exit(1);
  }

  if (o->c == 4 && o->t != 0) {
    fprintf(stderr, "-t not implemented for -c 4\n");
    exit(1);
  }

  state->pidfile_has_been_created = 0;
  if (o->p) {
    FILE *f;
    struct stat sb;

    if (stat(o->p, &sb) == -1 && errno == ENOENT) {
      state->pidfile_has_been_created = 1;
    }

    f = fopen(o->p, "w");
    if (f == NULL) {
      perror("fopen()/write/pidfile failed");
      exit(2);
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);
  }

  state->bytes_transferred = 0;
  if (o->l)
    state->n_lines = 0;

  stopit = 0;
  signal_report = 0;
  n_sighups = 0;
#ifdef SIGUSR2
  tsignal(SIGUSR2, sigshutdown);
#endif

#ifdef SIGUSR1
  tsignal(SIGUSR1, sigreport);
#endif
#ifdef SIGINFO
  if (!o->S)
    tsignal(SIGINFO, sigreport);
#endif
#ifdef SIGHUP
  tsignal(SIGHUP, sighup);
#endif

  state->bytes_firsttransfer = -1;
  state->time_firsttransfer = -1.0;

  state->bytes_lastreport = -1;
  state->time_lastreport = -1.0;

  if (gettimeofday(&t, NULL) == -1) {
    perror("gettimeofday() failed");
    exit(2);
  }
  state->starttime = (double)t.tv_sec + (double)t.tv_usec / 1000000.0;
  state->lasttime = -1.0;


  state->pmalloc.n_bytes = state->b;
  malloc_page_aligned(o, &state->pmalloc);
}

static void
closefiles(const struct options *const o, struct progstate *const state)
{
  if (state->ofd != 1 && state->ofd != 2 && state->ofd != -1)
    if (close(state->ofd) == -1) {
      perror("Cannot close outfile");
      exit(2);
    }
  if (state->ifd != 0 && state->ifd != -1)
    if (close(state->ifd) == -1) {
      perror("Cannot close infile");
      exit(2);
    }
}

/* Will return true if something has been written */
static void
print_kmg(const char *const pre
	  , const char *const format, const double num
	  , const char *const post, FILE *const f)
{
  if (pre != NULL)
    fprintf(f, "%s", pre);
  if (num >= 1000000000.0) {
    fprintf(f, format, num / 1024.0 / 1024.0 / 1024.0);
    fprintf(f, " G");
  } else if (num >= 1000000.0) {
    fprintf(f, format, num / 1024.0 / 1024.0);
    fprintf(f, " M");
  } else if (num >= 1000.0) {
    fprintf(f, format, num / 1024.0);
    fprintf(f, " K");
  } else {
    fprintf(f, format, num);
    fprintf(f, " ");
  }
  if (post != NULL)
    fprintf(f, "%s", post);
}

static void
report(const struct options *const o,
       struct progstate *const state,
       int curbytes)
{
  struct timeval t2;
  double sofar;
  double rate;
#ifndef NOTIMER
  struct itimerval itv;
#endif

  if (gettimeofday(&t2, NULL) == -1) {
    perror("gettimeofday() failed");
    exit(2);
  }
  sofar = ((double)t2.tv_sec + (double)t2.tv_usec / 1000000.0) -
    state->starttime;
  if (sofar > 0)
    rate = (double)state->bytes_transferred / sofar;
  else
    rate = 0.0;
  fprintf(stderr,"%.0f B",(double)state->bytes_transferred);
  print_kmg(" ", "%.1f", (double)state->bytes_transferred, "B", stderr);
  if (sofar < 200.0) {
    fprintf(stderr," %.2f s", sofar);
  } else {
    fprintf(stderr," %.1f s", sofar);
    if (sofar >= 3600.0)
      fprintf(stderr, " (%d:%02d h)", (int)sofar / 3600, (int)sofar % 3600 / 60);
    else
      fprintf(stderr, " (%d:%02d min)", (int)sofar / 60, (int)sofar % 60);
  }
  fprintf(stderr," %.0f B/s", rate);
  print_kmg(" ", "%.2f", rate, "B/s", stderr);
  if (o->l)
      fprintf(stderr, " %g lines", (double)state->n_lines);
#if 0
  // WTF?
  if (curbytes != -1 && state->b != state->b)
    fprintf(stderr, " %.1f %%buf", (double)curbytes / (double)state->b * 100.0);
#endif
  fprintf(stderr, "\n");

  if (o->v >= 2 && (sofar - state->time_firsttransfer) > 0.0) {
    rate =
      (
       (double)state->bytes_transferred - (double)state->bytes_firsttransfer
       ) / (sofar - state->time_firsttransfer);
    fprintf(stderr,"Since end of first transfer: %.0f B/s", rate);
    print_kmg(" ", "%.2f", rate, "B/s", stderr);
    fprintf(stderr, "\n");

    if (state->bytes_lastreport != -1) {
      rate =
	(
	 (double)state->bytes_transferred - (double)state->bytes_lastreport
	 ) / (sofar - state->time_lastreport);
      fprintf(stderr,"Since last report          : %.0f B/s", rate);
      print_kmg(" ", "%.2f", rate, "B/s", stderr);
      fprintf(stderr, "\n");
    }
  }

  state->bytes_lastreport = state->bytes_transferred;
  state->time_lastreport = sofar;

  if (o->T) {
#ifndef NOTIMER
    itv.it_interval.tv_sec = o->T;
    itv.it_interval.tv_usec = 0;
    itv.it_value.tv_sec = o->T;
    itv.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &itv, NULL);
#else
    fprintf(stderr, "WARNING: timer support not compiled in\n");
#endif
  }
}

int
my_write(const struct options *const o, struct progstate *const state
	 , const void *buf, const size_t n_bytes) {
  int ret;
  static int warning_printed = 0;

  if (state->using_o_direct && (long)buf % pagesize != 0) {
    if (warning_printed == 0) {
      warning_printed = 1;
      fprintf(stderr, "Write from buffer not page aligned, copying data\n");
    }
    if (state->pmalloc.n_bytes < n_bytes) {
      fprintf(stderr, "Target buffer too small, reallocating\n");
      free(state->pmalloc.allocated);
      state->pmalloc.n_bytes = n_bytes;
      malloc_page_aligned(o, &state->pmalloc);
    }
    memcpy(state->pmalloc.data, buf, n_bytes);
    buf = state->pmalloc.data;
  }

  // performance fixme.  It would be better to directly read(2) into
  // this mmap'ed space, however the read code in here has all kinds
  // of other complications so copy instead
  if (state->outputfile_position != -1) {
    if (o->v > 2) {
      // ....
      fprintf(stderr, "in mmap output file mode: copying %lld bytes, rel position %lld, addr %p\n", (long long)n_bytes, (long long)state->outputfile_position, state->outputfile_map + state->outputfile_position);
    }
    bcopy(buf, state->outputfile_map + state->outputfile_position, n_bytes);
    state->outputfile_position += n_bytes;
    return n_bytes;
  }

  // regular write
  ret = write(state->ofd, buf, n_bytes);
  if (ret != -1)
    return ret;

  /* Error */

  /* This is fine */
  if (errno == EINTR)
    return 0;

  /* If we were doing O_DIRECT, retry after reopening */
  if (state->using_o_direct) {
    if (o->v > 0)
      fprintf(stderr, "Write of %llu bytes failed at %p (%lld) (normal)"
	      ", reopening without O_DIRECT\n"
	      , (unsigned long long)n_bytes
	      , buf, (long long)(long)buf % pagesize);
    if (close(state->ofd) == -1) {
      perror("Cannot close output file\n");
      exit(2);
    }
    state->using_o_direct = 0;
    open_output_file(o, state, O_RDWR | O_APPEND);
  }

  ret = write(state->ofd, buf, n_bytes);
  if (ret != -1)
    return ret;

  if (errno == EINTR)
    return 0;

  report(o, state, -1);
  perror("write");
  exit(2);
}

void
malloc_page_aligned(const struct options *const o
		    , struct pmalloc *const pmalloc)
{
  pmalloc->allocated = malloc(pmalloc->n_bytes + pagesize);

  if (pmalloc->allocated == NULL) {
    perror("malloc");
    exit(2);
  }

  pmalloc->data = pmalloc->allocated + pagesize 
    - (long)pmalloc->allocated % pagesize;

  if (o->v >=4) {
    fprintf(stderr, "Page-aligned malloc at %p -> %p\n"
	    , pmalloc->allocated, pmalloc->data);
  }

}

#if HAVE_POLL_H
static void
pollloop(const struct options *const o
	 , struct progstate *const state)
{
  char *buf; /* Always points to base of allocation */
  char *curread, *curwrite; /* Iterators */
  int ret;
  struct pollfd pollfd[2];
  struct pollfd *pfd;
  int want_to_read;
  int want_to_write;
  struct pmalloc palloc;

  palloc.n_bytes = state->b;
  malloc_page_aligned(o, &palloc);
  buf = palloc.data;

  pollfd[0].fd = state->ifd;
  pollfd[0].events = POLLIN|POLLRDNORM;
  pollfd[1].fd = state->ofd;
  pollfd[1].events = POLLOUT|POLLWRNORM;
  curread = curwrite = buf;

  ret = -1;
  while (stopit == 0) {
    want_to_read = want_to_write = 0;

    /* Something left to write? */
    if (curwrite < curread)
      want_to_write = 1;

    /* Space in buffer for another read */
    if (curread - buf /* Bytes in Buffer */
	<=
	state->b - state->b /* Max bytes in buffer for another read() */
	) {
      want_to_read = 1;
    } else {
      /*
       * No space for another read().
       * Try to reset buffer pointers if everything has been written.
       * In that case, we also want to read into the new buffer.
       * Otherwise, we want to write, but not read.
       */
      if (curwrite == curread) {
	curread = curwrite = buf;
	want_to_read = 1;
      }
    }

    if (want_to_read)
      pfd = pollfd;
    else
      pfd = pollfd + 1;

    if (o->v >= 4)
      fprintf(stderr, "Polling, want_to_read %d want_to_write %d\n"
	      , want_to_read, want_to_write);
    ret = poll(pfd, want_to_read + want_to_write, INFTIM);
    if (ret == -1) {
      if (errno == EINTR)
	continue;
      perror("poll");
      exit(2);
    }
    if (o->v >= 4)
      fprintf(stderr, "Poll returned %d\n", ret);

    if (want_to_write && pollfd[1].revents & (POLLOUT|POLLWRNORM)) {
      int nbytes;

      if (o->w <= curread - curwrite)
	nbytes = o->w;
      else
	nbytes = curread - curwrite;

      if (o->v >= 4)
	fprintf(stderr, "Trying to write %d bytes (1)\n", nbytes);
      ret = my_write(o, state, curwrite, nbytes);
      if (o->v >= 4)
	fprintf(stderr, "Wrote %d bytes\n", ret);
      curwrite += ret;
    }

    if (want_to_read && pollfd[0].revents & (POLLIN|POLLRDNORM)) {
      if (o->v >= 4)
	fprintf(stderr, "Trying to read\n");

      if (o->n && state->bytes_transferred + state->b > o->n) {
	ret = read(state->ifd, curread, o->n - state->bytes_transferred);
      } else
	ret = read(state->ifd, curread, state->b);

      if (ret == -1) {
	if (errno == EINTR)
	  continue;
	perror("read");
	exit(2);
      }
      if (o->v >= 4)
	fprintf(stderr, "Read %d bytes\n", ret);
      curread += ret;
      state->bytes_transferred += ret;
    }
    if (signal_report) {
      report(o, state, curread - buf);
      signal_report = 0;
    }
    if (n_sighups > 0) {
      handle_sighup();
    }
    if (
	 ((want_to_read && ret == 0) || (o->n == state->bytes_transferred))
	 && curwrite == curread) {
      if (o->v > 1)
	fprintf(stderr, "#%dfinishing on condition 1\n",
		childpid ? 1 : 0);
      stopit = 1;
    }
  }
  if (strchr(o->O, 'F')) {
    if (fsync(state->ofd) < 0) {
      perror("fsync");
      exit(2);
    }
  }

  closefiles(o, state);
  if (o->v >= 1)
    report(o, state, -1);

  free(palloc.allocated);
}
#endif /* HAVE_POLL_H */

static void
loop(struct options *const o, struct progstate *const state)
{
  char *buf; /* Holds the whole buffer state->b*/
  char *curbuf; /* Iterates over buf when reading and leave after last
		 * read position */
  char *curbuf2; /* Iterate over buf when writing */
  int nbytes = 0; /* Just one system call read() */
  long long bytes_read = 0; /* Overall, both loops */
  int pipefd[2];
  struct pmalloc palloc;

  state->pid = getpid();
  if (o->c > 0) {
    if (pipe(pipefd) == -1) {
      perror("pipe");
      exit(3);
    }
    childpid = fork();
    if (childpid == -1) {
      perror("fork");
      exit(3);
    }
    if (childpid == 0) {
      /* Child */
      state->pid = getpid();

      /* reads from pipe */
      close(pipefd[1]);
      state->ifd = pipefd[0];
    } else {
      /* Parent */
#if 0
      /* Disable that for now */
      tsignal(SIGCHLD, sigchld);
#endif
      if (o->v >= 4)
	fprintf(stderr, "Child has pid (%d)\n", state->pid);

      /* Parent writes to pipe */
      close(pipefd[0]);
      state->ofd = pipefd[1];
    }
  }

  if (o->c == 1) { /* Reader will buffer */
    if (childpid == 0) { /* Then writer will just use block size */
      state->b = state->b;
    }
  } else if (o->c == 2) {
    if (childpid != 0) { /* Otherwise reader will just use block size */
      state->b = state->b;
    }
  }

  palloc.n_bytes = state->b;
  malloc_page_aligned(o, &palloc);
  buf = palloc.data;

  if (o->c == 0 || childpid != 0) { /* Only in parent process */
    if (state->ifd == -1) {
      /* Data will just be generated, fill buffer will something resembling
       * something useful
       *
       * Either ASCII text or a 440 Hz wave (audio mode)
       */
      if (!strchr(o->O, 'a')) {
	/* ASCII text for non-audio */
	int i;

	for (i = 0; i < state->b; i++) {
	  /* Newlines below 80 chars and at end of buffer */
	  if ((i+1) % 76 == 0 || i == o->n - 1)
	    buf[i] = '\n';
	  else
	    buf[i] = 'A' + rand() % 26;
	}
      } else {
	/*
	 * Audio mode, generate 440 Hz wave
	 *
	 * Do it verbose to do it right
	 */
	int i;
	double rate = 44100.0;
	double freq = 440.0;
	double pi = 3.141592653589793;
	int frames = rate / freq;
	unsigned short val;

	if (frames * 2 * 2 > state->b) {
	  fprintf(stderr, "Blocksize too small for wave\n");
	  exit(1);
	}

	state->b = frames * 2 * 2; /* 16 bit Stereo */
	for (i = 0; i < frames; i++) {
	  val = sin((double)i * 2.0 * pi / (double)frames) * 32767.0;
	  buf[i * 4] = buf[i * 4 + 2] = val % 256;
	  buf[i * 4 + 1] = buf[i * 4 + 3] = val / 256;
	}
      }
    }
  } else {
    /* In child process, don't answer to signals */
#ifdef SIGUSR1
    signal(SIGUSR1, SIG_DFL);
#endif
#ifdef SIGINFO
    signal(SIGINFO, SIG_DFL);
#endif
#ifdef SIGHUP
    signal(SIGHUP, SIG_DFL);
#endif
  }


  while (stopit == 0) {
    /* Outer loop - encapsulates pairs of read/write loops */

    for (curbuf = buf; stopit == 0 && curbuf + state->b <= buf + state->b;
	 curbuf += nbytes, bytes_read += nbytes) {
      /* Inner loop, fill buffer with reads() */

      /* Sleep long enough to satisfy througput limit. Do this before
       * the read, so that the sending process will be blocked exactly
       * the right amount of time.
       */

      if (o->t != 0 && !(o->c > 0 && childpid == 0) && bytes_read > 0) {
	struct timeval t2;
	double sofar;
	double theory;
	double time_to_sleep;

	if (gettimeofday(&t2, NULL) == -1) {
	  perror("gettimeofday() failed");
	  exit(2);
	}

	if (o->t > 0) {
	  /* Limit bandwith absolutely over whole session */

	  sofar = ((double)t2.tv_sec + (double)t2.tv_usec / 1000000.0) -
	    state->starttime;

	  /* In theory we should have needed how many useconds? */
	  theory = (double)bytes_read / (double)o->t;
	  if (o->v >= 4)
	    fprintf(stderr,"We needed %g seconds, should be %g\n"
		    , sofar, theory);
	  if (sofar < theory)
	    usleep((theory - sofar) * 1000000.0);
	} else {
	  /* limit bandwith per read/write */
	  sofar = ((double)t2.tv_sec + (double)t2.tv_usec / 1000000.0) -
	    state->lasttime;

	  /* fixme efficiency - can be moved to be computed once */
	  /* how much time should it have taken? */
	  theory = state->b / (double) - o->t;

	  time_to_sleep = (theory - sofar) * 1000000.0;
	  if (o->v >= 4)
	    fprintf(stderr,"We needed %g seconds for %d bytes"
		    ", should be %g, sleeping %g\n"
		    , sofar, state->b, theory, time_to_sleep);

	  state->lasttime = ((double)t2.tv_sec +
			     (double)t2.tv_usec / 1000000.0);

	  if (sofar < theory) {
	    usleep(time_to_sleep);
	    state->lasttime += time_to_sleep / 1000000.0;
	  }
	}
      }

      /* Now do one read to the end of the current buffer */

      if (state->ifd == -1) {
	/* Just generate data */
	if (o->n && bytes_read + state->b > o->n) {
	  nbytes = o->n - bytes_read;
	  if (o->v > 1)
	    fprintf(stderr, "#%d finishing on condition 3\n",
		    childpid ? 1 : 0);
	  stopit = 1;
	}
	else
	  nbytes = state->b;
      } else {
	if (o->v >= 4)
	  fprintf(stderr,"#%d trying to read %d bytes\n", childpid ? 1 : 0,
		  state->b);

	if (o->n && bytes_read + state->b > o->n) {
	  nbytes = read(state->ifd, curbuf, o->n - bytes_read);
	  if (o->v > 1)
	    fprintf(stderr, "#%d finishing on condition 4\n",
		    childpid ? 1 : 0);
	  stopit = 1;
	}
	else
	  nbytes = read(state->ifd, curbuf, state->b);

	if (o->v >= 4)
	  fprintf(stderr,"#%d read %d bytes\n", childpid ? 1 : 0, nbytes);
	if (nbytes == 0) {
	  if (o->v > 1)
	    fprintf(stderr, "#%d finishing on condition 1\n",
		    childpid ? 1 : 0);
	  stopit = 1;
	}

	if (nbytes == -1) {
	  if (errno == EINTR) {
	    nbytes = 0;
	  } else {
	    perror("read() failed");
	    report(o, state, -1);
	    exit(2);
	  }
	}
      }
      if (signal_report) {
	report(o, state, curbuf - buf);
	signal_report = 0;
      }
      if (n_sighups > 0) {
	handle_sighup();
      }
    }

    /* Count lines if requested */
    if (o->l) {
      char *s;

      for (s = buf; s < curbuf; s++) {
	if (*s == '\n') {
	  state->n_lines++;
	}
      }
    }

    /* Write out */
    for (curbuf2 = buf; curbuf2 < curbuf; curbuf2 += nbytes) {
      if (state->ofd == -1) {
	if (curbuf - curbuf2 < state->b)
	  nbytes = curbuf - curbuf2;
	else
	  nbytes = state->b;
      } else {
	int n;

	if (curbuf - curbuf2 < state->b)
	  n = curbuf - curbuf2;
	else
	  n = state->b;

	if (o->v >= 4)
	  fprintf(stderr, "Trying to write %d bytes (2) from %p\n", n
		  , curbuf2);
	nbytes = my_write(o, state, curbuf2, n);

	if (o->v >= 4)
	  fprintf(stderr,"#%d wrote %d bytes\n", childpid ? 1 : 0, nbytes);

	/* ? tee mode */
	if ((o->c == 0 || childpid != 0) && /* Only in parent process */
	    state->teefd != -1) {
	  if (curbuf - curbuf2 < state->b)
	    nbytes = write(state->teefd, curbuf2, curbuf - curbuf2);
	  else
	    nbytes = write(state->teefd, curbuf2, state->b);
	  if (nbytes == -1) {
	    if (errno == EINTR) {
	      nbytes = 0;
	    } else {
	      perror("write() failed");
	      report(o, state, -1);
	      exit(2);
	    }
	  }
	  if (o->v >= 4)
	    fprintf(stderr,"#%d wrote %d bytes to fd %d\n"
		    , childpid ? 1 : 0, nbytes, state->teefd);
	}
      }
      state->bytes_transferred += nbytes;
      if (o->v >= 2 && state->bytes_firsttransfer == -1) {
	struct timeval t;
	state->bytes_firsttransfer = nbytes;
	if (gettimeofday(&t, NULL) == -1) {
	  perror("gettimeofday() failed");
	  exit(2);
	}
	state->time_firsttransfer =
	  ((double)t.tv_sec + (double)t.tv_usec / 1000000.0) -
	  state->starttime;
      }

      if (o->n && state->bytes_transferred >= o->n) {
	if (o->v > 1)
	  fprintf(stderr, "#%d finishing on condition 5\n",
		  childpid ? 1 : 0);
	stopit = 1;
      }
      if (signal_report) {
	report(o, state, curbuf - buf);
	signal_report = 0;
      }
      if (n_sighups > 0) {
	handle_sighup();
      }
    }
  }

  if (strchr(o->O, 'F')) {
    if (fsync(state->ofd) < 0) {
      perror("fsync");
      exit(2);
    }
  }

  closefiles(o, state);
  if (o->v >= 1 && ! (o->c > 0 && childpid == 0)) /* Parent only */
    report(o, state, -1);

  if (o->c > 0) {
    if (childpid == 0) {
      exit(0);
    } else {
      wait(NULL);
    }
  }

  free(palloc.allocated);
}

void
cleanup(const struct options *const o, struct progstate *const state)
{
  if (o->p && state->pidfile_has_been_created)
    if (unlink(o->p) == -1)
      perror("Unlink pidfile failed - continuing");
}

static void
print_version(void)
{
  printf("%s\n", VERSION);
}

static void
usage(void)
{
  fprintf(stderr, "cstream by Martin Cracauer - version " VERSION "\n");
  fprintf(stderr,
	  "-V     = print version number to stdout and exit with 0\n"
	  "-v <n> = verbose [default: off]\n"
	  "         0 = nothing\n"
	  "         1 = report bytes transferred and throughput\n"
	  "         2 = also throughput after first read/write\n"
	  "         3 = also seperate throughput for read and write "
	  "(unimplemented)\n"
	  "         3 = verbose stats on every read/write\n"
	  "-b <n> = blocksize [default: 8192]\n"
	  "-B <n> = buffer (at most) <n> bytes [default: one block]\n"
	  "-c <n> = Concurrency, writing done by a seperate process\n"
	  "         0 = no concurrency, one one process\n"
	  "         1 = read side buffers\n"
	  "         2 = write side buffers\n"
	  "         3 = both sides buffer, -B amount of data will be "
	  "transferred at once\n"
	  "-n <n> = overall size of data [default: unlimited]\n"
	  "-t <n> = throughput in bytes/sec [default: unlimited]\n"
	  "         if positive, bandwith is average over whole session.\n"
	  "         if negative, every write is delayed to not excceed.\n"
	  "-i <s> = name of input file, - = generate stream yourself\n"
	  "         to use stdin, use -i ''\n"
	  "-o <s> = name of output file, - = just sink data\n"
	  "         to use stdout, -o ''\n"
	  "-I <s> = Type of input file\n"
	  "-O <s> = Type of ouput file\n"
          "         'f' = fifo (create it)\n"
          "         'F' = issue fsync(2) on output file before closing\n"
          "         'a' = set audio modes on file (i.e. CD quality)\n"
          "         'N' = don't use TCP even if filename has ':'\n"
          "         't' = tee - in addition to outfile, "
	  "copy stream to fd 3\n"
          "         'D' = O_DIRECT\n"
          "         'S' = O_SYNC\n"
          "         'M' = use mmap for output file (do not use write(2)), "
	  "for hugetlbfs\n"
	  "         [Multiple chars allowed]\n"
	  "-p <s> = Write pid as ascii integer to file <s>\n"
	  "-l       include line count in statistics\n"
	  "-w <n> = Set write block size (-c 5 only)\n"
	  "-S       Don't output statistic on SIGINFO\n"
	  "-T <n> = Report throughput every <n> seconds\n"
	  "SIGINFO causes statistics to be written to stderr\n"
	  "SIGUSR1 causes statistics to be written to stderr\n"
	  "SIGUSR2 causes loop end after next buffer transfer\n"
	  "<file>  if -i has not been used, specifies input file\n"
          "-6 <n>  Use IPV6: -1 = don't, 1 = allow both, 2 = force v6\n"
          "        On some platforms server mode 1 forces ipv6, as\n"
          "        they don't open both v4 and v6 ports from one bind call.\n"
	  );
  exit(1);
}

long long
atoi_kmg(const char *const s)
{
  long long res;
  char c;

  res = atoll(s);
  if (s[0] != '\0') {
    c = tolower(s[strlen(s)-1]);
    switch (c) {
    case 'k': res *= (long long)1024; break;
    case 'm': res *= (long long)1024 * (long long)1024; break;
    case 'g': res *= (long long)1024 * (long long)1024 * (long long)1024; break;
    }
  }
  return res;
}

int
main(int argc, char *const argv[])
{
  struct options o;
  struct progstate state;
  int ch;
  size_t blocksize = 0;

  default_options(&o);

  //open_jack();

  while ((ch = getopt(argc, argv, "b:B:c:i:I:n:o:O:p:St:T:v:Vl6:")) != -1) {
    switch(ch) {
    case 'v': o.v = atoi(optarg); break;
    case 'b': blocksize = atoi_kmg(optarg); break;
    case 'B': o.B = atoi_kmg(optarg); break;
    case 'c': o.c = atoi(optarg); break;
    case 'l': o.l = 1; break;
    case 'n': o.n = atoi_kmg(optarg); break;
    case 't': o.t = atoi_kmg(optarg); break;
    case 'S': o.S = 1; break;
    case 'i': o.i = strdup(optarg); break;
    case 'o': o.o = strdup(optarg); break;
    case 'I': o.I = strdup(optarg); break;
    case 'O': o.O = strdup(optarg); break;
    case 'p': o.p = strdup(optarg); break;
    case 'V': print_version(); exit(0);
    case 'w': o.w = atoi_kmg(optarg); break;
    case 'T': 
#ifdef NOTIMER 
      fprintf(stderr, "Warning: timer support not compiled in\n");
#endif
      o.T = atoi(optarg);
      break;
    case '6':
      if (!atoi(optarg) != 0) {
        fprintf(stderr, "-6 arg must be a number, -1, 1 or 2: '%s'\n", optarg);
        exit(1);
      }
      o.six = atoi(optarg);
      break;
    default: usage();
    }
  }

  argc -= optind;
  argv += optind;

  switch (argc) {
  case 0: break;
  case 1:
    if (o.i == NULL)
      o.i = strdup(argv[0]);
    else
      usage();
    break;
  default: usage();
  }

  init(&o, &state, blocksize);

  if (o.T) {
#ifndef NOTIMER 
    struct itimerval itv;

    signal(SIGALRM, sigtimer);
    itv.it_interval.tv_sec = o.T;
    itv.it_interval.tv_usec = 0;
    itv.it_value.tv_sec = o.T;
    itv.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &itv, NULL);
#endif
  }

  if (o.c == 4) {
#if HAVE_POLL_H
    pollloop(&o, &state);
#else
    fprintf(stderr, "Support for poll loop not available\n");
    exit(1);
#endif
  }
  else
    loop(&o, &state);

  cleanup(&o, &state);

  free(state.pmalloc.allocated);
  /* More freeing of memory omitted */

  return 0;
}
