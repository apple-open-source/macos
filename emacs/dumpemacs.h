#define kEmacsVersionMinor	"1"
#define kEmacsBinDir		"/usr/bin"
#define kEmacsLibExecDir       	"/usr/libexec"
#define kEmacsShareDir		"/usr/share/emacs"

#ifdef __ppc__
#define kEmacsArch        "ppc"
#endif
#ifdef __ppc64__
#define kEmacsArch        "ppc64"
#endif
#ifdef __i386__
#define kEmacsArch        "i386"
#endif
#ifdef __x86_64__
#define kEmacsArch        "x86_64"
#endif

#ifndef kEmacsArch
#error "Unsupported architecture"
#endif

/*#define kEmacsWrapperPath	kEmacsBinDir "/emacs"*/
#define kEmacsDumpedPath	kEmacsBinDir "/emacs"
#define kEmacsUndumpedPath	kEmacsBinDir "/emacs-undumped"
#define kEmacsArchPath		kEmacsBinDir "/emacs-" kEmacsArch
#define kDumpEmacsPath		kEmacsLibExecDir "/dumpemacs"

int is_emacs_valid(int debugflag);
int runit(const char * const argv[], int dropprivs);
