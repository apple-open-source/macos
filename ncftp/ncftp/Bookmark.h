/* Bookmark.h */

typedef struct Bookmark *BookmarkPtr;
typedef struct Bookmark {
	BookmarkPtr			prev;
	BookmarkPtr			next;
	int				index;

	char				bookmarkName[16];
	char				name[64];
	char				user[64];	/* Leave room for user@host.name */
	char				pass[32];
	char				acct[16];
	char				dir[160];
	int				xferType;
	unsigned int			port;
	time_t				lastCall;
	int				hasSIZE;
	int				hasMDTM;
	int				hasPASV;
	int				isUnix;
	char				lastIP[32];
	char				comment[128];
	long				xferKbytes;
	long				xferHSeconds;
	int				nCalls;
	int				noSaveDir;
	int				xferMode;
} Bookmark;

typedef BookmarkPtr *BookmarkPtrList;

#define kBookmarkVersion		7
#define kBookmarkMinVersion		3
#define kBookmarkFileName		"bookmarks"
#define kOldBookmarkFileName		"hosts"
#define kBookmarkBupFileName		"bookmarks.old"

#define kNoBookmarkLimit 0

void SortBookmarks(void);
void UpdateBookmarkPtr(BookmarkPtr, BookmarkPtr);
BookmarkPtr AddBookmarkPtr(BookmarkPtr);
BookmarkPtr RemoveBookmarkPtr(BookmarkPtr);
void MakeBookmarkUnique(char *, size_t);
void MakeUpABookmarkName(char *, size_t, char *);
void SetBookmarkDefaults(BookmarkPtr);
void SetNewBookmarkDefaults(BookmarkPtr);
int GetBookmark(char *, size_t);
int ParseHostLine(char *, BookmarkPtr);
void ReadBookmarkFile(void);
BookmarkPtr DuplicateBookmark(BookmarkPtr);
void DeleteBookmark(BookmarkPtr);
void SaveCurHostBookmark(char *);
void SaveBookmark(char *);
void WriteBookmarkFile(void);

/* eof */
