/* Hostwin.h */

#define kControl_L	12

/* Simple bit manipulation macros. */
#define TESTBIT(f,b) ((f) & (1 << ((b) + 1)))
#define BIT(b) (1 << ((b) + 1))

/* These are the items listed in the Site Options screen. */
#define kNicknameEditWindowItem		0
#define kHostnameEditWindowItem		1
#define kUserEditWindowItem			2
#define kPassEditWindowItem			3
#define kAcctEditWindowItem			4
#define kDirEditWindowItem			5
#define kXferTypeEditWindowItem		6
#define kPortEditWindowItem			7
#define kSizeEditWindowItem			8
#define kMdtmEditWindowItem			9
#define kPasvEditWindowItem			10
#define kOSEditWindowItem			11
#define kCommentEditWindowItem		12

#define kFirstEditWindowItem		kNicknameEditWindowItem
#define kLastEditWindowItem			kCommentEditWindowItem
#define kQuitEditWindowItem			(kLastEditWindowItem + 1)

/* We update the site options screen (gEditHostWin) one item at a time,
 * using a bit mask.  This way we can update specific items.  So to
 * do a full update we need to have our bit mask be all 1's.
 */
#define kAllWindowItems			(-1)

/* These are for use with EditHostWinGetStr() and PrefWinGetStr(). */
#define kOkayIfEmpty 1
#define kNotOkayIfEmpty 0
#define kGetAndEcho 1
#define kGetNoEcho 0

/* Parameter for use with EditHostWinDraw() and PrefWinDraw(). */
#define kNoHilite 0
#define kHilite 1

#define kToggleMsg "Space toggles;  Hit <RETURN> when done."

void UpdateHostWindows(int);
void AtoIMaybe(int *, char *);
void DrawHostList(void);
int HostWinGetKey(void);
void HostWinZoomTo(int);
void HostListLineUp(void);
void HostListLineDown(void);
void HostListPageUp(void);
void HostListPageDown(void);
void EditHostWinMsg(char *);
void EditHostWinGetStr(char *, size_t, int, int);
void EditHostWinGetNum(int *);
void EditHostWinDraw(int, int);
void ToggleXferType(void);
void EditWinToggle(int *, int, int, int);
void HostWinEdit(void);
void HostWinDup(void);
void HostWinDelete(void);
void HostWinNew(void);
void HostWinMsg(char *);
void HostWinGetStr(char *, size_t);
void SigIntHostWin(void);
int HostWindow(void);
int HostsCmd(void);
