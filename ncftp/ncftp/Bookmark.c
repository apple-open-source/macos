/* Bookmark.c */

#include "Sys.h"

#include <ctype.h>

/* TO-DO: If I decide to implement strstr matching, watch out for
 * local domain hosts, with simple names.
 */

#include "Util.h"
#include "Bookmark.h"
#include "FTP.h"

/* We keep an array of structures of all the sites we know about in
 * a contigious block of memory.
 */
BookmarkPtr gHosts = (BookmarkPtr) 0;

/* But we also keep an array of pointers to the structures, so we can
 * use that for sorting purposes.  We don't want to have to shuffle
 * large structures around when sorting.  As the name hints, we have
 * this array sorted by nickname, for quick searching.
 */
BookmarkPtrList gBookmarks = (BookmarkPtrList) 0;

/* This is the number of elements in both gHosts and gBookmarks. */
int gNumBookmarks = 0;

/* We don't want the other portions of the program overwriting the
 * current entry in the gHosts list, because we may want to save our
 * changes under a new entry.  So if the host was in our list, we make
 * a copy of the data, and let them write into that.  Then we can write
 * the changed structure under a new entry, or overwrite the old one.
 * This also works for entirely new entries.  We just give the caller
 * this, initialized to the default values.
 */
Bookmark gRmtInfo = { NULL, NULL, 0, "", "" };

/* Used to tell if we got the information from the host information list,
 * or we were using a new entry.
 */
int gRmtInfoIsNew;

/* We use this outside of this module to tell if we should actually
 * save the information collected.  We don't want to save it if the
 * stuff wasn't really valid, so we won't save unless you logged in
 * successfully.
 */
int gWantRmtInfoSaved;

/* Used to tell if the host file needs to be written back out.
 * If we haven't changed anything, then don't waste the time to
 * write the file.
 */
int gModifiedBookmarks = 0;

/* These are the first and last nodes in the linked-list of remote
 * site information structures.
 */
BookmarkPtr gFirstRsi = NULL, gLastRsi = NULL;

/* If greater than zero, we will only save the most recent sites, up
 * to this number.
 */
int gMaxBookmarks = kNoBookmarkLimit;

extern string gEmailAddress, gAnonPassword;
extern int gPreferredDataPortMode;
extern string gOurDirectoryPath;
extern longstring gRemoteCWD;

static
int BookmarkSortProc(const BookmarkPtr *a, const BookmarkPtr *b)
{
	return (ISTRCMP((**a).bookmarkName, (**b).bookmarkName));	
}	/* BookmarkSortProc */



static
int BookmarkSortTimeProc(const BookmarkPtr *a, const BookmarkPtr *b)
{
	return ((**b).lastCall - (**a).lastCall);	
}	/* BookmarkSortTimeProc */



static
int BookmarkSearchProc(char *key, const BookmarkPtr *b)
{
	return (ISTRCMP(key, (**b).bookmarkName));	
}	/* BookmarkSearchProc */





void SortBookmarks(void)
{
	int i;
	BookmarkPtr p;

	if (gBookmarks != (BookmarkPtrList) 0)
		free(gBookmarks);
	gBookmarks = (BookmarkPtrList) malloc(
		sizeof(BookmarkPtr) * (gNumBookmarks + 1)
	);	
	if (gBookmarks == (BookmarkPtrList) 0)
		OutOfMemory();

	for (p = gFirstRsi, i=0; p != NULL; i++, p = p->next) {
		gBookmarks[i] = p;
	}
	gBookmarks[gNumBookmarks] = NULL;

	QSORT(gBookmarks,
		gNumBookmarks, sizeof(BookmarkPtr), BookmarkSortProc);
	
	for (i=0; i<gNumBookmarks; i++) {
		p = gBookmarks[i];
		p->index = i;
	}
}	/* SortBookmarks */




void UpdateBookmarkPtr(BookmarkPtr dst, BookmarkPtr src)
{
	BookmarkPtr next, prev;
	int idx;
	
	/* Need to preserve dst's links, but copy all of src's stuff. */
	next = dst->next;
	prev = dst->prev;
	idx = dst->index;
	*dst = *src;
	dst->next = next;
	dst->prev = prev;
	dst->index = idx;
}	/* UpdateBookmarkPtr */





BookmarkPtr AddBookmarkPtr(BookmarkPtr buf)
{
	BookmarkPtr newRsip;

	newRsip = (BookmarkPtr) malloc(sizeof(Bookmark));
	if (newRsip != NULL) {
		memcpy(newRsip, buf, sizeof(Bookmark));
		newRsip->next = NULL;
		if (gFirstRsi == NULL) {
			gFirstRsi = gLastRsi = newRsip;
			newRsip->prev = NULL;
		} else {
			newRsip->prev = gLastRsi;
			gLastRsi->next = newRsip;
			gLastRsi = newRsip;
		}
		++gNumBookmarks;
		/* Just need to know if we should write out the host file later. */
		gModifiedBookmarks++;
	} else {
		OutOfMemory();
	}
	return newRsip;
}	/* AddBookmarkPtr */





BookmarkPtr RemoveBookmarkPtr(BookmarkPtr killMe)
{
	BookmarkPtr nextRsi, prevRsi;
	
	nextRsi = killMe->next;	
	prevRsi = killMe->prev;	
	
	if (gFirstRsi == killMe)
		gFirstRsi = nextRsi;
	if (gLastRsi == killMe)
		gLastRsi = prevRsi;

	if (nextRsi != NULL)
		nextRsi->prev = prevRsi;
	if (prevRsi != NULL)
		prevRsi->next = nextRsi;

	PTRZERO(killMe, sizeof(Bookmark));
	free(killMe);
	--gNumBookmarks;
	++gModifiedBookmarks;
	return (nextRsi);
}	/* RemoveBookmarkPtr */





void MakeBookmarkUnique(char *dst, size_t siz)
{
	int i;
	string s2, s3;
	BookmarkPtr *bmpp;
	char *cp;

	/* Make sure we can concat 3 more characters if necessary. */
	Strncpy(s2, dst, siz - 3);
	for (cp = s2 + strlen(s2) - 1; cp > s2; ) {
		if (isdigit(*cp))
			*cp-- = '\0';
		else
			break;
	}

	/* Make a copy of the original. */
	STRNCPY(s3, dst);
	
	for (i=1; i<=999; i++) {
		if (i > 1)
			sprintf(dst, "%s%d", s2, i);
		else
			Strncpy(dst, s3, siz);
	
		/* See if there is already a nickname by this name. */
		if (gNumBookmarks == 0)
			break;
		bmpp = (BookmarkPtr *) BSEARCH(dst, gBookmarks, gNumBookmarks,
			sizeof(BookmarkPtr), BookmarkSearchProc);
		if (bmpp == NULL)
			break;
	}
}	/* MakeBookmarkUnique */




void MakeUpABookmarkName(char *dst, size_t siz, char *src)
{
	string str;
	char *token;
	char *cp;

	STRNCPY(str, src);
	
	/* Pick the first "significant" part of the hostname.  Usually
	 * this is the first word in the name, but if it's something like
	 * ftp.unl.edu, we would want to choose "unl" and not "ftp."
	 */
	token = str;
	if ((token = strtok(token, ".")) == NULL)
		token = "misc";
	else if (ISTREQ(token, "ftp")) {
		if ((token = strtok(NULL, ".")) == NULL)
			token = "misc";
	}
	for (cp = token; ; cp++) {
		if (*cp == '\0') {
			/* Token was all digits, like an IP address perhaps. */
			token = "misc";
		}
		if (!isdigit(*cp))
			break;
	}
	Strncpy(dst, token, siz);
	MakeBookmarkUnique(dst, siz);
}	/* MakeUpABookmarkName */




void SetBookmarkDefaults(BookmarkPtr bmp)
{
	PTRZERO(bmp, sizeof(Bookmark));

	bmp->xferType = 'I';
	bmp->xferMode = 'S';	/* Use FTP protocol default as ours too. */
	bmp->port = kPortUnset;
	bmp->hasSIZE = 1;	/* Assume we have it until proven otherwise. */
	bmp->hasMDTM = 1;	/* Assume we have it until proven otherwise. */
	if (gPreferredDataPortMode >= kPassiveMode) {
		/* Assume we have it until proven otherwise. */
		bmp->hasPASV = 1;
	} else {
		/* If default is PORT, then make the user explicitly set this. */
		bmp->hasPASV = 0;
	}	
	bmp->isUnix = 1;
	bmp->lastCall = (time_t) 0;
}	/* SetBookmarkDefaults */



void SetNewBookmarkDefaults(BookmarkPtr bmp)
{
	/* Return a pointer to a new entry, initialized to
	 * all the defaults, except for name and nickname.
	 */
	SetBookmarkDefaults(bmp);
	STRNCPY(bmp->name, "foobar.snafu.gov");
	STRNCPY(bmp->bookmarkName, "NEW");

	/* That will make a unique "NEW" nickname. */
	MakeBookmarkUnique(bmp->bookmarkName, sizeof(bmp->bookmarkName));
}	/* SetNewBookmarkDefaults */




int GetBookmark(char *host, size_t siz)
{
	BookmarkPtr *bmpp;
	int i;
	size_t len;
	
	if (gNumBookmarks == 0)
		bmpp = NULL;
	else {
		bmpp = (BookmarkPtr *)
			BSEARCH(host, gBookmarks, gNumBookmarks,
				sizeof(BookmarkPtr), BookmarkSearchProc);
		if (bmpp == NULL) {
			/* No exact match, but the user doesn't have to type the
			 * whole nickname, just the first few letters of it.
			 */
			/* This could probably be done in a bsearch proc too... */
			len = strlen(host);
			for (i=0; i<gNumBookmarks; i++) {
				if (ISTRNEQ(gBookmarks[i]->bookmarkName, host, len)) {
					bmpp = &gBookmarks[i];
					break;
				}
			}
		}
		if ((bmpp == NULL) && (strchr(host, '.') != NULL)) {
			/* If thing we were given looks like a full hostname (has at
			 * least one period), see if we have an exact match on the
			 * hostname.
			 *
			 * This is actually not recommended -- you should try to use
			 * the nicknames only since they are unique.  We can have more
			 * than one entry for the same hostname!
			 */
			for (i=0; i<gNumBookmarks; i++) {
				if (ISTREQ(gBookmarks[i]->name, host)) {
					bmpp = &gBookmarks[i];
					break;
				}
			}
		}
	}

	gWantRmtInfoSaved = 0;
	if (bmpp != NULL) {
		gRmtInfo = **bmpp;
		
		/* So we know that this isn't in the list, but just a copy
		 * of someone else's data.
		 */
		gRmtInfo.next = gRmtInfo.prev = NULL;
		
		gRmtInfoIsNew = 0;
		/* gHost needs to be set here, since the caller wasn't using
		 * a real host name.
		 */
		Strncpy(host, gRmtInfo.name, siz);
		return (1);
	}
	
	SetNewBookmarkDefaults(&gRmtInfo);	
	STRNCPY(gRmtInfo.name, host);
	MakeUpABookmarkName(gRmtInfo.bookmarkName, sizeof(gRmtInfo.bookmarkName), host);
	
	gRmtInfoIsNew = 1;
	return (0);
}	/* GetBookmark */




int ParseHostLine(char *line, BookmarkPtr bmp)
{
	string token;
	char *s, *d;
	char *tokenend;
	long L;
	int i;
	int result;

	SetBookmarkDefaults(bmp);
	s = line;
	tokenend = token + sizeof(token) - 1;
	result = -1;
	for (i=0; ; i++) {
		if (*s == '\0')
			break;
		/* Some tokens may need to have a comma in them.  Since this is a
		 * field delimiter, these fields use \, to represent a comma, and
		 * \\ for a backslash.  This chunk gets the next token, paying
		 * attention to the escaped stuff.
		 */
		for (d = token; *s != '\0'; ) {
			if ((*s == '\\') && (s[1] != '\0')) {
				if (d < tokenend)
					*d++ = s[1];
				s += 2;
			} else if (*s == ',') {
				++s;
				break;
			} else {
				if (d < tokenend)
					*d++ = *s;
				++s;
			}
		}
		*d = '\0';
		switch(i) {
			case 0: (void) STRNCPY(bmp->bookmarkName, token); break;
			case 1: (void) STRNCPY(bmp->name, token); break;
			case 2: (void) STRNCPY(bmp->user, token); break;
			case 3: (void) STRNCPY(bmp->pass, token); break;
			case 4: (void) STRNCPY(bmp->acct, token); break;
			case 5: (void) STRNCPY(bmp->dir, token);
					result = 0;		/* Good enough to have these fields. */
					break;
			case 6: bmp->xferType = token[0]; break;
			case 7:
				/* Most of the time, we won't have a port. */
				if (token[0] == '\0')
					bmp->port = (unsigned int) kDefaultFTPPort;
				else
					bmp->port = (unsigned int) atoi(token);
				break;
			case 8:
				sscanf(token, "%lx", &L);
				bmp->lastCall = (time_t) L;
				break;
			case 9: bmp->hasSIZE = atoi(token); break;
			case 10: bmp->hasMDTM = atoi(token); break;
			case 11: bmp->hasPASV = atoi(token); break;
			case 12: bmp->isUnix = atoi(token);
					result = 3;		/* Version 3 had all fields to here. */
					break;
			case 13: (void) STRNCPY(bmp->lastIP, token); break;
			case 14: (void) STRNCPY(bmp->comment, token); break;
			case 15: sscanf(token, "%ld", &bmp->xferKbytes); break;
			case 16: sscanf(token, "%ld", &bmp->xferHSeconds);
					result = 4;		/* Version 4 had all fields up to here. */
					break;
			case 17: bmp->nCalls = atoi(token);
					result = 5;		/* Version 5 has all fields to here. */
					break;
			case 18: bmp->noSaveDir = atoi(token);
					result = 6;		/* Version 6 has all fields to here. */
					break;
			case 19: bmp->xferMode = token[0];
					result = 7;		/* Version 7 has all fields to here. */
					break;
			default:
					result = 99;	/* Version >7 ? */
					goto done;
		}
	}
done:
	return (result);
}	/* ParseHostLine */




void ReadBookmarkFile(void)
{
	string pathName;
	string path2;
	FILE *fp;
	longstring line;
	int version;
	Bookmark newRsi;

	if (gOurDirectoryPath[0] == '\0')
		return;		/* Don't create in root directory. */
	OurDirectoryPath(pathName, sizeof(pathName), kBookmarkFileName);
	fp = fopen(pathName, "r");
	if (fp == NULL) {
		OurDirectoryPath(path2, sizeof(path2), kOldBookmarkFileName);
		if (rename(path2, pathName) == 0) {
			/* Rename succeeded, now open it. */
			fp = fopen(pathName, "r");
			if (fp == NULL)
				return;
		}
		return;		/* Okay to not have one yet. */
	}

	if (FGets(line, sizeof(line), fp) == NULL)
		goto badFmt;
	
	/* Sample line we're looking for:
	 * "NcFTP bookmark-file version: 2"
	 */
	version = -1;
	(void) sscanf(line, "%*s %*s %*s %d", &version);
	if (version < kBookmarkMinVersion) {
		if (version < 0)
			goto badFmt;
		STRNCPY(path2, pathName);
		sprintf(line, ".v%d", version);
		STRNCAT(path2, line);
		(void) rename(pathName, path2);
		Error(kDontPerror, "%s: old version.\n", pathName);
		fclose(fp);
		return;
	}
		
	if (FGets(line, sizeof(line), fp) == NULL)
		goto badFmt;
	
	/* Sample line we're looking for:
	 * "Number of entries: 28"
	 */
	gNumBookmarks = -1;
	
	/* At the moment, we don't really care about the number stored in the
	 * file.  It's there for future use.
	 */
	(void) sscanf(line, "%*s %*s %*s %d", &gNumBookmarks);
	if (gNumBookmarks < 0)
		goto badFmt;
	
	gHosts = (BookmarkPtr) 0;
	gBookmarks = (BookmarkPtrList) 0;
	gNumBookmarks = 0;
	
	while (FGets(line, sizeof(line), fp) != NULL) {
		if (ParseHostLine(line, &newRsi) >= 0) {
			AddBookmarkPtr(&newRsi);
		}
	}
	fclose(fp);

	SortBookmarks();
	DebugMsg("Read %d entries from %s.\n", gNumBookmarks, pathName);
	return;
	
badFmt:
	Error(kDontPerror, "%s: invalid format.\n", pathName);
	fclose(fp);
}	/* ReadBookmarkFile */




BookmarkPtr DuplicateBookmark(BookmarkPtr origbmp)
{
	Bookmark newRsi;
	BookmarkPtr newRsip;
	string str;

	STRNCPY(str, origbmp->bookmarkName);
	MakeBookmarkUnique(str, sizeof(origbmp->bookmarkName));

	newRsi = *origbmp;
	STRNCPY(newRsi.bookmarkName, str);
	newRsip = AddBookmarkPtr(&newRsi);

	/* Have to re-sort now so our bsearches will work. */
	SortBookmarks();
	return (newRsip);
}	/* DuplicateBookmark */




void DeleteBookmark(BookmarkPtr bmp)
{
	if (gNumBookmarks < 1)
		return;
	
	RemoveBookmarkPtr(bmp);
	SortBookmarks();
}	/* DuplicateBookmark */




void SaveBookmark(char *asNick)
{
	BookmarkPtr *bmpp;
	Bookmark rm;

	memcpy(&rm, &gRmtInfo, sizeof(rm));
	STRNCPY(rm.bookmarkName, asNick);
	STRNCPY(rm.dir, gRemoteCWD);
	rm.xferKbytes = 0L;
	rm.xferHSeconds = 0L;
	rm.nCalls = 0;

	/* Don't update dir if you move around the next time you use it. */
	rm.noSaveDir = 1;

	if ((gNumBookmarks == 0) || (gBookmarks == NULL))
		bmpp = NULL;
	else
		bmpp = (BookmarkPtr *) BSEARCH(asNick,
			gBookmarks, gNumBookmarks,
			sizeof(BookmarkPtr), BookmarkSearchProc);
	if (bmpp == NULL) {
		/* Add a new entry. */
		rm.comment[0] = '\0';
		(void) AddBookmarkPtr(&rm);

		/* Have to re-sort now so our bsearches will work. */
		SortBookmarks();
		PrintF("Saving new bookmark named \"%s\" in your host file, pointing\nto <URL:ftp://%s/%s/>.\n",
			rm.bookmarkName,
			rm.name,
			rm.dir
		);
	} else {
		/* Copy over an existing one. */
		UpdateBookmarkPtr(*bmpp, &rm);
		PrintF("Updated bookmark named \"%s\" in your host file, so it now points\nto <URL:ftp://%s/%s/>.\n",
			rm.bookmarkName,
			rm.name,
			rm.dir
		);
	}

	/* Just need to know if we should write out the host file later. */
	gModifiedBookmarks++;
}	/* SaveBookmark */




void SaveCurHostBookmark(char *asNick)
{
	BookmarkPtr *bmpp;

	if (gRmtInfoIsNew) {
		(void) AddBookmarkPtr(&gRmtInfo);
		PrintF("Saving new bookmark named \"%s\" in your host file, pointing\nto <URL:ftp://%s/%s/>.\n",
			gRmtInfo.bookmarkName,
			gRmtInfo.name,
			gRmtInfo.dir
		);

		/* Have to re-sort now so our bsearches will work. */
		SortBookmarks();
	} else {
		/* We were working with an existing entry.
		 * If the nickname given to us as the parameter is different
		 * from the existing bookmarkName, then we're supposed to save
		 * this as a new entry.
		 */
		if ((asNick == NULL) || ISTREQ(asNick, gRmtInfo.bookmarkName)) {
			/* Save over old entry. */
			bmpp = (BookmarkPtr *) BSEARCH(gRmtInfo.bookmarkName,
				gBookmarks, gNumBookmarks,
				sizeof(BookmarkPtr), BookmarkSearchProc);
			/* This had better be in there, since we did this before
			 * and it was in there.
			 */
			if (bmpp == NULL) {
				Error(kDontPerror,
				"Programmer's error: couldn't re-find host info entry.\n");
				return;
			}
			/* Copy over the old stuff. */
			UpdateBookmarkPtr(*bmpp, &gRmtInfo);

			/* Just need to know if we should write out the host file later. */
			gModifiedBookmarks++;
		} else {
			/* Add a new entry. */
			STRNCPY(gRmtInfo.bookmarkName, asNick);
			MakeBookmarkUnique(gRmtInfo.bookmarkName, sizeof(gRmtInfo.bookmarkName));
			(void) AddBookmarkPtr(&gRmtInfo);

			/* Have to re-sort now so our bsearches will work. */
			SortBookmarks();
		}
	}

	gRmtInfoIsNew = 0;
}	/* SaveCurHostBookmark */



static
void EscapeString(char *d, char *s)
{
	if (s != NULL) {
		while (*s != '\0') {
			if (*s == ',' || *s == '\\')
				*d++ = '\\';
			*d++ = *s++;
		}
	}
	*d = '\0';
}	/* EscapeString */




void WriteBookmarkFile(void)
{
	string pathName;
	string bupPathName;
	longstring escapedStr;
	FILE *fp;
	char portStr[16];
	int i;
	int nPasswds;
	BookmarkPtr bmp;

	if (!gModifiedBookmarks)
		return;

	OurDirectoryPath(pathName, sizeof(pathName), kBookmarkFileName);

	if ((gMaxBookmarks != kNoBookmarkLimit) && (gNumBookmarks > gMaxBookmarks)) {
		DebugMsg("Purging %d old remote sites from %s.\n",
			gNumBookmarks - gMaxBookmarks,
			pathName
		);

		/* Sort sites by last time we called.  We want the older sites to
		 * float to the bottom.
		 */
		QSORT(gBookmarks,
			gNumBookmarks, sizeof(BookmarkPtr), BookmarkSortTimeProc);

		gNumBookmarks = gMaxBookmarks;
	}
	
	/* See if we can move the existing file to a new name, in case
	 * something happens while we write this out.  Host files are
	 * valuable enough that people would be pissed off if their
	 * host file got nuked.
	 */
	OurDirectoryPath(bupPathName, sizeof(bupPathName), kBookmarkBupFileName);
	(void) UNLINK(bupPathName);
	(void) rename(pathName, bupPathName);
	
	fp = fopen(pathName, "w");
	if (fp == NULL)
		goto err;
	
	if (fprintf(fp, "NcFTP bookmark-file version: %d\nNumber of entries: %d\n",
		kBookmarkVersion,
		gNumBookmarks
	) < 0)
		goto err;
	if (fflush(fp) < 0)
		goto err;

	for (i=0, nPasswds=0; i<gNumBookmarks; i++) {
		*portStr = '\0';
		bmp = gBookmarks[i];
		if (bmp->port != kDefaultFTPPort)
			sprintf(portStr, "%u", bmp->port);
		if ((bmp->pass[0] != '\0')
			&& (!STREQ(bmp->pass, gEmailAddress))
			&& (!STREQ(bmp->pass, gAnonPassword)))
			nPasswds++;
		if (bmp->acct[0] != '\0')
			nPasswds++;		/* Don't publicize accounts, either. */

		/* Insert the quote character '\' for strings that can have
		 * commas or backslashes in them.
		 */
		EscapeString(escapedStr, bmp->pass);
		if (fprintf(fp, "%s,%s,%s,%s,%s,",
			bmp->bookmarkName,
			bmp->name,
			bmp->user,
			escapedStr,
			bmp->acct
		) < 0)
			goto err;

		EscapeString(escapedStr, bmp->dir);
		if (fprintf(fp, "%s,%c,%s,%lx,%d,%d,%d,%d,",
			escapedStr,
			bmp->xferType,
			portStr,
			(unsigned long) bmp->lastCall,
			bmp->hasSIZE,
			bmp->hasMDTM,
			bmp->hasPASV,
			bmp->isUnix
		) < 0)
			goto err;

		EscapeString(escapedStr, bmp->comment);
		if (fprintf(fp, "%s,%s,%ld,%ld,%d,%d,%c\n",
			bmp->lastIP,
			escapedStr,
			bmp->xferKbytes,
			bmp->xferHSeconds,
			bmp->nCalls,
			bmp->noSaveDir,
			bmp->xferMode
		) < 0)
			goto err;
	}
	if (fclose(fp) < 0) {
		fp = NULL;
		goto err;
	}
	(void) UNLINK(bupPathName);
	if (nPasswds > 0) {
		/* Set permissions so other users can't see the passwords.
		 * Of course this isn't really secure, which is why the program
		 * won't save passwords entered at the password prompt.  You must
		 * explicitly set them from the host editor.
		 */
		(void) chmod(pathName, 0600);	/* Set it to -rw------- */
	}
	return;

err:
	if (access(bupPathName, F_OK) < 0) {
		Error(kDoPerror, "Could not write to %s.\n", pathName);
	} else {
		/* Move backup file back to the original. */
		rename(bupPathName, pathName);
		Error(kDoPerror, "Could not update %s.\n", pathName);
	}
	if (fp != NULL)
		fclose(fp);
}	/* WriteBookmarkFile */
