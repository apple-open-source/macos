/* Complete.c */
/* Tim MacKenzie, t.mackenzie@trl.oz.au, April '95 */

#include "Sys.h"
#include "LineList.h"
#include "Cmdline.h"
#include "Complete.h"
#include "Prefs.h"
#include "Bookmark.h"
#include "Util.h"
#include "List.h"
#include <signal.h>
#include <setjmp.h>

extern int gNumCommands;     /* Cmdlist.c */
extern Command gCommands[];  /* Cmdlist.c */
extern PrefOpt gPrefOpts[];      /*Prefs.c */
extern int gNumEditablePrefOpts; /*Prefs.c */
extern BookmarkPtr gFirstRsi; /* Bookmark.c */
extern longstring gRemoteCWD;       /* Cmds.c */
extern Bookmark gRmtInfo;   /* Bookmark.c */
extern int gScreenWidth; /* Win.c */

/* We can't use a linked list because we need random access for the
 * output routine...
 */
typedef struct {
	char **names;
	int count;
	int alloc;
} FileList;

struct _DirCache {
	struct _DirCache *next;
	FileList files;
	char *name;
	int flags; /* Combination of flags below */
};

static DirCache *cacheHead;
static DirCache *currentCache;

static void ForgetCurrent(void);
/* Upon receipt of a signal we have to abort the completion */
static jmp_buf gCompleteJmp;

#define LS_F 1              /* ls -F */
#define LS_L 2				/* ls -l */
#define LS_DIR 4			/* Have been supplied a directory to list */
#define LS_R 8			   /* ls -R */

typedef char * (*CompleteFunc)(char *, int);

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>

#ifdef HAVE_FILENAME_COMPLETION_FUNCTION
/* This should have been in your readline.h already, but some older
 * versions of the library are still lurking out there.
 */
extern char *filename_completion_function ();	/* Yes, not a prototype... */
#endif
#endif


static char *
Strmcpy(char *dst, char *src, int howMany, size_t dstSize)
{
	size_t max;

	/* If you want to limit Strncpy to x characters, you
	 * must pass x + 1 because Strncpy subtracts one for the
	 * nul terminator.
	 */
	max = howMany + 1;

	if (max > dstSize)
		max = dstSize;
	return (Strncpy(dst, src, max));
}	/* Strmcpy */


static char *
StrnDup(char *s, int len)
{
	char *res;

	res = (char *) malloc(len+1);
	strncpy(res,s,len);
	res[len] = 0;
	return res;
}
	
static char *
ConvertDir(char *dir)
{
	int baselen;
	string res;
	int i,j;

	baselen = (int) strlen(gRemoteCWD);
	while (dir && *dir) {
		if (*dir == '/') { /* It's an absolute pathname */
			baselen = 0;
			break;
		}
		if (*dir == '.' && dir[1] == '.') { /* ../ remove bit from base */
			dir += 2;
			while (*dir && *dir == '/') dir++;
			while (baselen > 0 && gRemoteCWD[baselen-1] != '/')
				baselen--;
			baselen--; /* Move back past the '/' */
			if (baselen < 0) 
				baselen = 0;
			continue;
		}
		if (*dir == '.' && (dir[1] == '/' || dir[1] == 0)) {
			/* . or ./ just remove it from dirname */
			dir += 1;
			while (*dir && *dir == '/') dir++;
			continue;
		}
		break;
	}
	if (!dir)
		dir = "";
	Strmcpy(res, gRemoteCWD, baselen, sizeof(res));
	STRNCAT(res, "/");
	STRNCAT(res, dir);
	STRNCAT(res, "/");

	/* Remove //'s in the name */
	for (i=j=1;res[i];i++) {
		if (res[i] == '/' && res[j-1] == '/')
			continue;
		res[j++] = res[i];
	}
	res[j] = 0;
	return StrDup(res);
}

static DirCache *
FindDirCache(char *dir)
{
	DirCache *l;
	dir = ConvertDir(dir);
	for (l = cacheHead;l;l=l->next)
		if (!strcmp(dir, l->name))
			break;
	free(dir);
	return l;
}

static void
SigComplete(/* int sigNumUNUSED */ void)
{
	alarm(0);
	longjmp(gCompleteJmp,1);
}

/* Get the filenames from the current directory */
static DirCache *
GetCompleteFiles(char *dir)
{
	LineList fileList;
	LinePtr f;
	DirCache *volatile l;
	volatile Sig_t si, sp;

	l = FindDirCache(dir);
	if (l)
		return l;

	InitLineList(&fileList);
	si = SIGNAL(SIGINT, SigComplete);
	sp = SIGNAL(SIGPIPE, SigComplete);
	SetBar(NULL, "GETTING COMPLETIONS", NULL, -1, 1);
	if (setjmp(gCompleteJmp)) { /* Complete was interrupted */
		ForgetCurrent();
	} else {
		GetFileList(&fileList, dir);
		/* GetFileList returns a list of filenames with a preceding character
		 * denoting type
		 */
		l = CompleteStart(dir);
		/* GetFileList gets types, so might as well set -F */
		CompleteSetFlags("-F"); 
		 
		for (f = fileList.first; f != NULL; f = f->next) {
			char *tmp = (char *)malloc(strlen(f->line)+1);
			strcpy(tmp,f->line+1);
			if (f->line[0] == 'd')
				strcat(tmp,"/");
			CompleteParse(tmp);
			free(tmp);
		}
	}
	SetBar(NULL, NULL, NULL, -1, 1); /* Reset bar */
	CompleteFinish();
	DisposeLineListContents(&fileList);
	SIGNAL(SIGINT, si);
	SIGNAL(SIGPIPE, sp);
	return l;
}

static void
InitFileList(FileList *f)
{
	f->names = 0;
	f->count = 0;
	f->alloc = 0;
}

static void
EmptyFileList(FileList *f)
{
	int i;
	if (!f->names)
		return;
	for (i=0;i<f->count;i++)
		free(f->names[i]);
	free(f->names);
	InitFileList(f);
}

/* Add a filename to the list */
static void
FileListAdd(FileList *f, char *s)
{
	if (f->alloc <= f->count+1) {
		f->alloc += 10;
		if (f->names)
			f->names = (char **) realloc(f->names, f->alloc * sizeof(char*));
		else
			f->names = (char **) malloc(f->alloc * sizeof(char*));
	}
	f->names[f->count++] = s;
	f->names[f->count] = 0;
}

static int
CompareStrings(char **a, char **b)
{
	return strcmp(*a, *b);
}

static void
CompleteMatches(FileList *l, char *word, CompleteFunc f)
{
	char *s;
	InitFileList(l);
	for (s = (*f)(word, 0); s ; s = (*f)(word, 1))
		FileListAdd(l,s);
	if (l->count > 1)
		QSORT(l->names, l->count, sizeof (char*), CompareStrings);
	return;
}

void
ClearDirCache(void)
{
	DirCache *next, *curr;
	for (curr = cacheHead ; curr ; curr = next) {
		next = curr->next;
		EmptyFileList(&curr->files);
		if (curr->name)
			free(curr->name);
		free(curr);
	}
	cacheHead = 0;
	currentCache = 0;
}

/* Start a completion cycle - the initializes some memory to put the
 * next listing into.
 */
DirCache *
CompleteStart(char *dir)
{
	/* The flag determines whether this is a clear due to a cd or other
	 * cache invalidating command (flag=0) or due to the start of an ls
	 * command (flag=1)
	 */
	DirCache *res;
	
	if (currentCache) {
		Error(kDontPerror,
			"Starting new completion without finishing last one\n");
		CompleteFinish();
	}
	dir = ConvertDir(dir);
	res = (DirCache *)malloc(sizeof *res);
	res->name = dir;
	res->flags = 0;
	res->next = 0;
	InitFileList(&res->files);
	currentCache = res;

	return res;
}

static int
BetterCache(DirCache *a, DirCache *b)
{
	/* Returns true if a is better than b */
	/* The only way b is better is if it has directory information and 
	 * a doesn't
	 */
	if (b->flags & (LS_L|LS_F) && !(a->flags & (LS_L|LS_F)))
		return 0;
	return 1;
}

void
CompleteFinish(void)
{
	/* Finish off the current completion, adding it to the cache if it's
	 * better than the old one
	 */
	DirCache *c;
	if (!currentCache)
		return;
	
	c = FindDirCache(currentCache->name);
	if (!c) {
		currentCache->next = cacheHead;
		cacheHead = currentCache;
	} else {
		if (BetterCache(currentCache, c)) {
			/* Replace old with new - we just nuke the old and copy the new
			 * over it
			 */
			currentCache->next = c->next;
			EmptyFileList(&c->files);
			free(c->name);
			*c = *currentCache;
		} else {
			/* Discard new */
			EmptyFileList(&currentCache->files);
			free(currentCache->name);
		}
		/* We free the memory allocated to the new since we copied over the
		 * old or just left the old
		 */
		free(currentCache);
	}
	currentCache = 0;
}
	
/* Generate dir/file names for the readline completion generator */
static char *
CompleteDirFileGenerator(char *text,int state, int dir)
{
	static int len,ind;
	static DirCache *c;
	static char *find;
	static string base;
	string res;
	char *cmd;
	char *s;

	if (!state) {
		ind = 0;
		s = strrchr(text,'/');
		if (s) {
			Strmcpy(base, text, s - text + 1, sizeof(base));
			find = s+1;
			c = GetCompleteFiles(base);
		} else {
			c = GetCompleteFiles(".");
			find = text;
			base[0] = '\0';
		}
		if (!c) {
			return 0;
		}
		len = strlen(find);
	}

	while (ind < c->files.count) {
		cmd = c->files.names[ind++];
		if (!strncmp(cmd,find,len)) {
			if (dir && (c->flags & (LS_L|LS_F)) && 
					!(cmd[strlen(cmd)-1] == '/' || cmd[strlen(cmd)-1] == '@'))
				continue;
			STRNCPY(res, base);
			STRNCAT(res, cmd);
			return (StrDup(res));
		}
	}
	return 0;
}

/* Generate file names for the readline completion generator */
static char *
CompleteFileGenerator(char *text,int state)
{
	return CompleteDirFileGenerator(text,state,0);
}

/* Generate directories for the readline completion generator */
static char *
CompleteDirGenerator(char *text,int state)
{
	return CompleteDirFileGenerator(text,state,1);
}

/* Generate commands for the readline completion routines */
static char *
CompleteCommandGenerator(char *text,int state)
{
	static int len,ind;
	char *cmd;

	if (!state) {
		len = strlen(text);
		ind = 0;
	}

	while (ind < gNumCommands) {
		cmd = gCommands[ind].name;
		ind ++;
		if (!strncmp(cmd,text,len))
			return StrDup(cmd);
	}
	return 0;
}

/* Generate options for the readline completion routines */
static char *
CompleteOptionGenerator(char *text,int state)
{
	static int len,ind;
	char *cmd;

	if (!state) {
		len = strlen(text);
		ind = 0;
	}

	while (ind < gNumEditablePrefOpts) {
		cmd = gPrefOpts[ind].name;
		ind ++;
		if (!strncmp(cmd,text,len))
			return StrDup(cmd);
	}
	return 0;
}

/* Generate options for the readline completion routines */
static char *
CompleteHostGenerator(char *text,int state)
{
	static int len;
	static BookmarkPtr curr;
	char *cmd;

	if (!state) {
		len = strlen(text);
		curr = gFirstRsi;
	}

	while (curr) {
		cmd = curr->bookmarkName;
		curr = curr->next;
		if (!strncmp(cmd,text,len))
			return StrDup(cmd);
	}
	return 0;
}

static char *
CompleteNoneGenerator(void)
{
	return 0;
}

static CompleteFunc
FindCompleteFunc(char *line, int start)
{
	int len= 0;
	string cmd;
	Command *c;

	if (start == 0)
		return CompleteCommandGenerator;

	while (line[len] && isspace(line[len])) len++;
	while (line[len] && !isspace(line[len])) len++;
	Strmcpy(cmd, line, len, sizeof(cmd));
	c = GetCommand(cmd, 0);
	if (!c)
		return CompleteFileGenerator;
	switch (c->complete) {
		case kCompleteDir:
			if (gRmtInfo.isUnix)
				return CompleteDirGenerator;
			/* Fall through */
		case kCompleteFile:
			return CompleteFileGenerator;
		case kCompleteCmd:
			return CompleteCommandGenerator;
		case kCompleteOption:
			return CompleteOptionGenerator;
		case kCompleteHost:
			return CompleteHostGenerator;
		case kCompleteLocal:
#ifdef HAVE_FILENAME_COMPLETION_FUNCTION
			return filename_completion_function;
#endif
		default:
			return (CompleteFunc) CompleteNoneGenerator;
	}
}

static void
ForgetCurrent(void)
{
	DirCache *c;
	if (!currentCache)
		return;
	c = cacheHead;
	if (c != currentCache)
		return; /* Something really weird is happening */
	EmptyFileList(&c->files);
	cacheHead = c->next;
	free(c);
	currentCache = 0;
}
	
/* Look through ls flags to determine if this is ls -l/ls -F, etc. */
void
CompleteSetFlags(char *s)
{
	if (!currentCache)
		return;
	if (*s == '-') {
		if (strchr(s,'l'))
			currentCache->flags |= LS_L;
		if (strchr(s,'F'))
			currentCache->flags |= LS_F;
		if (strchr(s,'R'))
			currentCache->flags |= LS_R;
	} else if (*s) {
		if (strchr(s,'*'))
			ForgetCurrent();
		if (strchr(s,'['))
			ForgetCurrent();
		if (currentCache->flags & LS_DIR)
			ForgetCurrent();
		if (currentCache) {
			free(currentCache->name);
			currentCache->name = ConvertDir(s);
			currentCache->flags |= LS_DIR;
		}
	}
}

/* Parse the output of ls for filenames */
void
CompleteParse(char *s)
{
	char *t;
	int len=0;
	string ss;
	string tmp;

	if (!*s) return;
	if (!currentCache) return;
	if (currentCache->flags & LS_R) {
		while (s[len] && s[len] != ':') len++;
		if (s[len] == ':') {
			int tmp_type = currentCache->flags; 
			/* Save the type which is clobbered by CompleteStart */
			Strmcpy(ss, s, len, sizeof(ss));
			CompleteFinish();
			CompleteStart(ss);
			currentCache->flags = tmp_type;
			return;
		}
	}
	if (currentCache->flags & LS_L) { /* Only use the last word on the line */
		/* If it's a "total <length>" line, ignore it. */
		if (!strncmp(s,"total ",6))
			return;
		t = s+strlen(s) - 1;
		while (!isspace(*t) && t>s) t--;
		if (isspace(*t)) t++;
		if (!(currentCache->flags & LS_F) && s[0] == 'd') {
			/* We have a dir with no -F flag -
			 * get type from first character on line
			 */
			STRNCPY(tmp, t);
			STRNCAT(tmp, "/");
			t = tmp;
		} else if (s[0] == 'l') { 
			/* It's a soft link - get the name of the link, not where it goes */
			char *end = t-1;
			/* Line should be lrwxrwxrwx .... file -> dest */
			while (!(*end == '-' && end[1] == '>') && end > s) end--;
			/* Sometimes the "-> dest" is missing */
			if (end > s) {
				/* end points to the space before "->" */
				end --;
				t = end - 1;
				/* Find the start of the word */
				while (!isspace(*t) && t>s) t--;
				if (isspace(*t)) t++;
				Strmcpy(tmp, t, end-t+1, sizeof(tmp));
				strcpy(tmp + (end-t), "@");
				t = tmp;
			}
		}
	} else { /* Use every word on the line */
		t = s;
	}
	while ((*t != '\0') && (isspace(*t)))
		t++;
	while (*t != '\0') {
		len = 0;
		while (t[len] && !isspace(t[len])) len++;
		/* Ignore last char if we gave -F to ls and it is one of @=* */
		/* Note: We don't remove the '/' off directories */
		if ((currentCache->flags & LS_F) && strchr("=*",t[len-1]))
			FileListAdd(&currentCache->files,StrnDup(t,len-1));
		else
			FileListAdd(&currentCache->files,StrnDup(t,len));
		t+=len;
		while ((*t != '\0') && (isspace(*t)))
			t++;
	}
}
	
/* How much of these 2 strings match? */
static int
MatchingLen(char *a, char *b)
{
	int i;
	for (i=0;a[i] && b[i];i++)
		if (a[i] != b[i])
			break;
	return i;
}

/* Get the completion characters for the word in line that ends at
 * position off
 */
char *
CompleteGet(char *line, int off)
{
	int i;
	int wstart;
	int matchlen;
	int cplen;
	int alen;
	char *cp;
	string res;
	CompleteFunc f;
	string match;
	FileList files;

	if (!line)
		return 0;

	/* Find the start of the word */
	for (wstart = off - 1; wstart >= 0 ; wstart --) {
		if (strchr(" \t\n",line[wstart]))
			break;
	}
	wstart++;

	Strmcpy(match, line+wstart, off-wstart, sizeof(match));
	f = FindCompleteFunc(line,wstart);
	CompleteMatches(&files, match, f);

	/* No matching files - give up */
	if (!files.count)
		return 0;

	/* If there was only one match, we complete the word and add a space
	 * as well (but only if it doesn't end in '/'
	 */
	if (files.count == 1) {
		matchlen = (int) strlen(files.names[0]);
		cp = files.names[0]+off-wstart;
		cplen = matchlen - (off-wstart) + 1;
		Strmcpy(res, cp, cplen, sizeof(res));
		alen = matchlen - 1;
		if (files.names[0][alen] == '@') {
			res[strlen(res)-1] = 0;
		} else if (files.names[0][alen] != '/') {
			STRNCAT(res, " ");
		}
		EmptyFileList(&files);
		cp = StrDup(res);
		return (cp);
	}

	/* Otherwise, find the longest common prefix of all words that match */
	matchlen = strlen(files.names[0]);
	for (i=1;i<files.count;i++) {
		int newmatch;
		newmatch = MatchingLen(files.names[i], files.names[i-1]);
		if (newmatch < matchlen)
			matchlen = newmatch;
	}

	/* Now, give the minimum number of characters which matched in all words */
	if (matchlen > off-wstart) {
		cp = StrnDup(files.names[0]+off-wstart, matchlen - (off-wstart));
	} else {
		cp = 0;
	}
	EmptyFileList(&files);
	return (cp);
}

/* Find the start of the last pathname component */
static char *
FindStart(char *s)
{
	char *tmp;
	for (tmp = s;*tmp;tmp++)
		if (*tmp == '/' && tmp[1])
			s = tmp+1;
	return s;
}
		
/* Print out what options we have for completing this word */
void
CompleteOptions(char *line, int off)
{
	int wstart;
	int maxlen,len,i,j;
	CompleteFunc f;
	string match;
	FileList files;
	int lines, columns;

	if (!line)
		return;

	/* Find start of word */
	for (wstart = off - 1; wstart >= 0 ; wstart --) {
		if (strchr(" \t\n",line[wstart]))
			break;
	}
	wstart++;

	Strmcpy(match, line+wstart, off-wstart, sizeof(match));
	f = FindCompleteFunc(line,wstart);
	CompleteMatches(&files, match, f);

	if (!files.count)
		return;

	/* Find the maximum length filename that matches (for nice outputting) */
	maxlen = strlen(FindStart(files.names[0]));
	for (i=1;i<files.count;i++) {
		len = strlen(FindStart(files.names[i]));
		if (len>maxlen)
			maxlen = len;
	}

	/* Calculate how many lines to display on: we want to display like ls
	 * does: 1 3 5
	 *       2 4 6
	 * Use (gScreenWidth+1)/(maxlen+2) since we never print the last pair
	 * of spaces on a line.
	 */
	columns = (gScreenWidth+1) / (maxlen+2);
	if (columns < 1)
		columns = 1;
	lines = (files.count + columns -1) / columns;
	/* A blank line so we can see where things start */
	PrintF("\n");
	for (i = 0; i<lines;i++) {
		for (j=0;j<columns;j++) {
			char *start;
			int off2 = i + j*lines;
			if (off2 >= files.count)
				continue;
			start = FindStart(files.names[off2]);
				
			PrintF("%-*.*s",maxlen,maxlen,start);
			if (j < columns-1)
				PrintF("  ");
		}
		PrintF("\n");
	}
	/* Ensure that we can see the things that have just arrived */
	UpdateScreen(1); 

	EmptyFileList(&files);
}

#ifndef HAVE_LIBREADLINE
void
InitReadline(void)
{
}

#else


/* Completion function for readline */
static char **
ncftp_completion(char *text, int start, int end) 
{
	CompleteFunc f;

	if (end < start)
		return NULL;
	f = FindCompleteFunc(text, start);
	return completion_matches(text,f);
}

void
InitReadline(void)
{
	rl_readline_name = "ncftp";
	rl_attempted_completion_function = ncftp_completion;
}

#endif
