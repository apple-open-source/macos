/* Main.h */

typedef struct UserInfo {
	char userName[32];
	char *home, *shell, *mail;
	int uid;
} UserInfo;

#define kOurDirectoryName	".ncftp"
#define kLogName			"log"
#define kTmpLogName			"log.copy"
#define kHistoryName		"history"

/* This could be a user variable in the future. */
#define kMaxHistorySaveLines 24

/* Debugging options. */
#define kDebuggingOff		0000
#define kDebuggingOn		0001
#define kTracingOff			0000
#define kTracingOn			0001

/* Stuff for the trace log. */
#define kMaxTraceLogLines	500
#define kTraceLogName		"trace"
#define kTraceLog2Name		"traces.old"
#define kTraceLogTmpName	"traces.tmp"

/* Lock file used to tell if another ncftp is running. */
#define kLockFileName		"i.am.running"

/* Types of startup messages printed by this program. */
#define kNoStartupMsgs				0
#define kStartupMsg					00001
#define kTips						00002

extern string gOurDirectoryPath;

void GetUserInfo(void);
void OpenTraceLog(void);
void OpenLogs(void);
void InitOurDirectory(void);
void Init(void);
void CloseTraceLog(void);
void CloseLogs(void);
void StartupMsgs(void);
void Startup(void);
void main(int, char **);
void SaveHistory(void);
void LoadHistory(void);
