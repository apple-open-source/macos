/* Prefs.h */

#define kAnonOpenPrefsWinItem		0
#define kAnonPassPrefsWinItem		1
#define kBlankLinesWinItem			2
#define kFTPModePrefsWinItem		3
#define kLogSizePrefsWinItem		4
#define kMaxBookmarksWinItem			5
#define kPagerPrefsWinItem			6
#define kProgressPrefsWinItem		7
#define kRmtMsgsPrefsWinItem		8
#define kStartupLCWDWinItem			9
#define kStartupMsgsPrefsWinItem	10
#define kTimeoutPrefsWinItem		11
#define kTracePrefsWinItem			12
#define kUTimePrefsWinItem			13
#define kVisualPrefsWinItem			14

#define kFirstPrefsWinItem			kAnonOpenPrefsWinItem
#define kLastPrefsWinItem			kVisualPrefsWinItem
#define kQuitPrefsWinItem			(kLastPrefsWinItem + 1)

#define kPrefInt					0
#define kPrefToggle					1
#define kPrefStr					2

#define kPrefsName					"prefs"

#define PREFINT(a)					kPrefInt, &a, sizeof(int), 0, 0
#define PREFBOOL(a)					kPrefToggle, &a, sizeof(int), 0, 1
#define PREFTOGGLE(a,min,max)		kPrefToggle, &a, sizeof(int), min, max
#define PREFSTR(a,b,c)				kPrefStr, a, sizeof(a), b, c


typedef struct PrefOpt {
	char *name, *label, *msg;
	int type;
	void *storage;
	size_t siz;
	int min, max;	/* Also used as flags for strings. */
} PrefOpt;

void TogglePref(int *, int, int);
void GetPrefSetting(char *, size_t, int);
void UpdatePrefsWindow(int);
void PrefsWinWinMsg(char *);
void PrefsWinGetStr(char *, int, int);
void PrefsWinGetNum(int *);
void PrefsWinDraw(int, int);
void PrefsWinToggle(int *, int, int, int);
void SigIntPrefsWin(void);
int PrefsWindow(void);
void ShowAll(void);
int SetCmd(int, char **);
int PrefsCmd(void);
void WritePrefs(void);
void ReadPrefs(void);
