/* Get.c */

#include "Sys.h"

#include <signal.h>

#ifdef HAVE_UTIME_H
#	include <utime.h>
#else
struct	utimbuf {time_t actime; time_t modtime;};
#endif 

#include "Util.h"
#include "RCmd.h"
#include "Xfer.h"
#include "Cmds.h"
#include "Glob.h"
#include "Get.h"
#include "DateSize.h"
#include "List.h"
#include "Getopt.h"

int gMayUTime = kDoUTime;	/* User variable. */

extern longstring gPager;
extern longstring gRemoteCWD;
extern longstring gLocalCWD;
extern int gTransferType, gXferAbortFlag;
extern int gStdout, gWinInit;
extern size_t gXferBufSize;
extern char *gOptArg, *gXferBuf;
extern int gOptInd;

int BinaryGet(XferSpecPtr xp)
{
	int result;

	/* This is supposed to be done previously, if you wanted accurate
	 * file sizes from GetDateAndSize.  We don't do a SETBINARY here.
	 * Instead, we set it to gTransferType, which we know is not
	 * ascii.  Most often this will mean binary mode, but perhaps we're
	 * dealing with a tenex machine.
	 */
	SetType(gTransferType);

	xp->xProc = StdFileReceive;
	/* xp->inStream = gDataSocket;  RDataCmd fills this in when it gets it. */

	/* Send the request and do the transfer. */
	result = RDataCmd(xp, "RETR %s", xp->remoteFileName);

	return (result);
}	/* BinaryGet */





int AsciiGet(XferSpecPtr xp)
{
	int result;

	/* This is supposed to be done previously, if you wanted accurate
	 * file sizes from GetDateAndSize.
	 */
	SETASCII;
	
	/* Setup the parameter block to give to RDataCmd. */
	xp->xProc = StdAsciiFileReceive;
	/* xp->inStream = gDataSocket;  RDataCmd fills this in when it gets it. */

	/* Send the request and do the transfer. */
	result = RDataCmd(xp, "RETR %s", xp->remoteFileName);

	return (result);
}	/* AsciiGet */



/* From the pathname given in remoteName, get a local filename for
 * the current local directory.  Then open the actual file for writing.
 */
static
void GetLocalName(GetOptionsPtr gopt, string localName)
{
	char *cp;

	if ((cp = gopt->lName) == NULL) {
		/* We're supposed to pick it. */
		cp = strrchr(gopt->rName, '/');
		if (cp == NULL)
			cp = gopt->rName;
		else
			cp++;
	}
	gopt->lName = Strncpy(localName, cp, sizeof(string));
}	/* GetLocalName */




void SetLocalFileTimes(int doUTime, time_t remoteModTime, char *lname)
{
	struct utimbuf ut;

	/* Restore the modifcation date of the new file to
	 * what it was on the remote host, if possible.
	 */
	if ((doUTime == kDoUTime) && (remoteModTime != kModTimeUnknown)) {
		time(&ut.actime);
		ut.modtime = remoteModTime;
		(void) utime(lname, &ut);
	}
}	/* SetLocalFileTimes */




int TruncReOpenReceiveFile(XferSpecPtr xp)
{
	int fd;
	
	close(xp->outStream);
	fd = open(xp->localFileName, O_WRONLY | O_TRUNC | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		/* Should never get here, since we were able to open this
		 * very file for appending earlier.
		 */
		Error(kDoPerror, "Can't re-open local file %s.\n", xp->localFileName);
		
		/* Try to give it something to write to anyway. */
		fd = open("/dev/null", O_WRONLY);
		xp->outStream = fd;
		
		if (fd < 0) {
			/* Don't core, please. */
			Exit(kExitPanic);
		}
		return (-1);
	}
	xp->outStream = fd;
	return (0);
}	/* TruncReOpenReceiveFile */




int DoGet(GetOptionsPtr gopt)
{
	int fd;
	int result;
	string local;
	long fileSize;
	time_t modifTime;
	int doReports;
	struct stat st;
	size_t restartPt;
	const char *mode = "w";
	time_t now;
	XferSpecPtr xp;

	if (gTransferType == 'A') {
		/* Have to set the type here, because GetDateAndSize() may
		 * use the SIZE command, and the result of that depends
		 * on the current transfer type setting.
		 */
		SETASCII;
	} else {
		SetType(gTransferType);
	}
	
	/* See if we can get some info about the file first. */
	fileSize = GetDateAndSize(gopt->rName, &modifTime);
	restartPt = SZ(0);
	doReports = 0;

	if (gopt->outputMode == kDumpToStdout) {
		fd = gStdout;
		STRNCPY(local, kLocalFileIsStdout);
		/* Don't have progress reports going if we're piping or
		 * dumping to the screen.
		 */
	} else {
		GetLocalName(gopt, local);
		if (stat(local, &st) == 0) {
			/* File exists on the local host.  We must decide whether
			 * we really want to fetch this file, since we might have
			 * it here already.  But when in doubt, we will go ahead
			 * and fetch the file.
			 */
			if (gopt->forceReget) {
				/* If the local file is smaller, then we
				 * should attempt to restart the transfer
				 * from where we left off.
				 */
				if ((st.st_size < fileSize) || (fileSize == kSizeUnknown)) {
					restartPt = SZ(st.st_size);
					mode = "a";
					DebugMsg("Manually continuing local file %s.", local);
				} else {
					PrintF("Already have %s with size %lu.\n", gopt->rName, fileSize);
					return (0);
				}
			} else if (!gopt->overwrite) {
				if (modifTime != kModTimeUnknown) {
					/* We know the date of the remote file. */
					DebugMsg("Local file %s has size %lu and is dated %s",
						local,
						(unsigned long) st.st_size,
						ctime(&st.st_mtime)
					);
					if (modifTime < st.st_mtime) {
						/* Remote file is older than existing local file. */
						PrintF("Already have %s.\n", gopt->rName);
						return (0);
					} else if (modifTime == st.st_mtime) {
						/* Remote file is same age. */
						if (fileSize != kSizeUnknown) {
							/* If the local file is smaller, then we
							 * should attempt to restart the transfer
							 * from where we left off, since we the remote
							 * file has the same date.
							 */
							if (st.st_size < fileSize) {
								restartPt = SZ(st.st_size);
								mode = "a";
							} else if (st.st_size == fileSize) {
								PrintF("Already have %s.\n", gopt->rName);
								return (0);
							} else {
								DebugMsg("Overwriting %s; local file has same date,\n",
									gopt->lName);
								DebugMsg("but local file is larger, so fetching remote version anyway.\n");
							}
						} else {
							DebugMsg("Overwriting %s; local file has same date,\n",
									gopt->lName);
							DebugMsg("but can't determine remote size, so fetching remote version anyway.\n");
						}
					} else {
						/* Remote file is more recent.  Fetch the
						 * whole file.
						 */
						DebugMsg("Overwriting %s; remote was newer.\n",
							gopt->lName);
					}
				} else {
					/* We don't know the date of the file.
					 * We won't be able to safely assume anything about
					 * the remote file.  It is legal to have a more
					 * recent remote file (which we don't know), with a
					 * smaller (or greater, or equal even) size.  We
					 * will just have to fetch it no matter what.
					 */
					DebugMsg("Overwriting %s; couldn't determine remote file date.\n",
						gopt->lName);
				}
			} else {
				DebugMsg("Explicitly overwriting %s.\n", gopt->lName);
			}
		} else {
			/* We don't have a local file with the same name as the remote,
			 * but we may also want to avoid doing the transfer of this
			 * file.  For example, this is where we check the remote
			 * file's date if we were told to only get files which are
			 * less than X days old.
			 */
			if (gopt->newer > 0) {
				time(&now);
				if (((unsigned long) now - (unsigned long) (gopt->newer * 86400)) > (unsigned long) modifTime) {
					DebugMsg("Skipping %s, older than %d days.\n",
						gopt->rName, gopt->newer);
					return (0);
				}
			}
		}
		if (*mode == 'w')
			fd = open(local, O_WRONLY | O_TRUNC | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		else
			fd = open(local, O_WRONLY | O_APPEND | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd < 0) {
			Error(kDoPerror, "Can't open local file %s.\n", local);
			return (-1);
		}
		doReports = gopt->doReports;
	}

	xp = InitXferSpec();
	xp->netMode = kNetReading;
	xp->outStream = fd;
	
	/* This group is needed for the progress reporting and logging stuff.
	 * Otherwise, it isn't that important.
	 */
	xp->doReports = doReports;
	xp->localFileName = local;
	xp->remoteFileName = gopt->rName;
	xp->expectedSize = fileSize;
	xp->startPoint = restartPt;
	xp->doUTime = gopt->doUTime;
	xp->remoteModTime = modifTime;
	
	if (gTransferType == 'A') {
		result = AsciiGet(xp);
	} else {		
		result = BinaryGet(xp);
	}

	if (fd != gStdout) {
		(void) close(fd);
		if ((result < 0) && (xp->bytesTransferred < 1L) && (*mode != 'a')) {
			/* An error occurred, and we didn't transfer anything,
			 * so remove empty file we just made.
			 */
			(void) UNLINK(local);
		} else {
			/* Restore the modifcation date of the new file to
			 * what it was on the remote host, if possible.
			 */
			SetLocalFileTimes(gopt->doUTime, modifTime, local);
		}
	}

	DoneWithXferSpec(xp);
	return (result);
}	/* DoGet */




void InitGetOutputMode(GetOptionsPtr gopt, int outputMode)
{
	gopt->outputMode = outputMode;
	if (outputMode == kSaveToDisk) {
		gopt->doUTime = gMayUTime;
		gopt->doReports = 1;
	} else {
		gopt->doUTime = kDontUTime;	
		gopt->doReports = 0;
	}
}	/* InitGetOutputMode */




void InitGetOptions(GetOptionsPtr gopt)
{
	PTRZERO(gopt, sizeof(GetOptions));
}	/* InitGetOptions */




int SetGetOption(GetOptionsPtr gopt, int opt, char *optArg)
{
	int i;

	switch (opt) {		
		case 'C':	/* Force continuation */
			gopt->forceReget = 1;
			break;
		case 'f':	/* Force overwrite (no reget, no newer) */
			gopt->overwrite = 1;
			break;
		case 'G':	/* No glob */
			gopt->noGlob = 1;
			break;
		case 'R':	/* Recursive */
			gopt->recursive = 1;
			break;
		case 'n':	/* Get files if not X days or newer.  */
			i = atoi(optArg);
			if (i <= 0) {
				EPrintF("Option to -n must be greater than zero.\n");
				return (kUsageErr);
			}
			gopt->newer = i;
			break;
		case 'z':	/* Get one file x, and save as y. */
			gopt->saveAs = 1;
			break;
		default:
			return (kUsageErr);
	}
	return (kNoErr);
}	/* SetGetOption */




int GetGetOptions(int argc, char **argv, GetOptionsPtr gopt)
{
	int opt;

	/* When this is called, we are always writing to disk.
	 * In other words, we have no colon-mode to worry about.
	 */
	InitGetOptions(gopt);
	InitGetOutputMode(gopt, kSaveToDisk);
	
	/* Tell Getopt() that we want to start over with a new command. */
	GetoptReset();
	while ((opt = Getopt(argc, argv, "CfGRn:z")) >= 0) {
		if (SetGetOption(gopt, opt, gOptArg) == kUsageErr)
			return (kUsageErr);
	}
	return (kNoErr);
}	/* GetGetOptions */




#ifdef HAVE_SYMLINK
static
int GetSymLinkInfo(char *dst, size_t siz, char *rLink)
{
	LineList fileList;
	char *cp;
	int result;

	result = -1;
	*dst = '\0';
	InitLineList(&fileList);
	ListToMemory(&fileList, "LIST", kListDirNamesOnlyMode, rLink);
	if (fileList.first != NULL) {
		cp = fileList.first->line;
		*cp++ = '\0';
		for (cp += strlen(cp) - 1; ; cp--) {
			if (*cp == '\0')
				goto done;
			if ((cp[0] == '>') && (cp[-1] == '-'))
				break;
		}
		(void) Strncpy(dst, cp + 2, siz);
		result = 0;
	}
done:
	DisposeLineListContents(&fileList);
	return (result);
}	/* GetSymLinkInfo */
#endif	/* HAVE_SYMLINK */



int GetDir(GetOptionsPtr gopt, char *dName, char *rRoot, char *lRoot)
{
	LineList dirFiles;
	LinePtr dirFile;
	char *rd;	/* Remote directory path. */
	char *ld;	/* Local directory path. */
	char *rf;	/* Complete remote pathname for an item. */
	char *lf;	/* Complete local pathname for an item. */ 
	char *sl;	/* What a symlink points to. */
	char *iName;
	int fType;

	rd = NULL;
	ld = NULL;
	rf = NULL;
	lf = NULL;
	
	if ((rd = StrDup(rRoot)) == NULL)
		goto fail;
	if ((rd = PtrCatSlash(rd, dName)) == NULL)
		goto fail;

	if ((ld = StrDup(lRoot)) == NULL)
		goto fail;
	if ((ld = PtrCatSlash(ld, dName)) == NULL)
		goto fail;
	
	/* Create this directory on the local host first. */
	if (MkDirs(ld)) {
		EPrintF("Could not create directory '%s.'\n", ld);
		goto fail;
	}
	
	/* Get the names of all files and subdirs. */
	InitLineList(&dirFiles);
	GetFileList(&dirFiles, rd);

	/* Get all the files first. */
	for (dirFile = dirFiles.first; dirFile != NULL; dirFile = dirFile->next) {
		fType = (int) dirFile->line[0];
		if ((fType == '-') || (fType == 'l')) {
			iName = dirFile->line + 1;
			if ((rf = StrDup(rd)) == NULL)
				goto fail;
			if ((rf = PtrCatSlash(rf, iName)) == NULL)
				goto fail;
			if ((lf = StrDup(ld)) == NULL)
				goto fail;
			if ((lf = PtrCatSlash(lf, iName)) == NULL)
				goto fail;
			if (fType == '-') {
				gopt->rName = rf;
				gopt->lName = lf;
				DoGet(gopt);
			} else {
#ifdef HAVE_SYMLINK
				sl = (char *) malloc(SZ(512));
				if (sl != NULL) {
					if (GetSymLinkInfo(sl, SZ(512), rf) == 0)
						(void) symlink(sl, lf);
					free(sl);
				}
#endif	/* HAVE_SYMLINK */
			}
			free(rf);
			free(lf);
			rf = NULL;
			lf = NULL;
		}
		if (gXferAbortFlag == SIGINT)
			break;	/* Don't get rest of files if you interrupted. */
	}
	
	/* Now get subdirectories. */
	for (dirFile = dirFiles.first; dirFile != NULL; dirFile = dirFile->next) {
		if (gXferAbortFlag == SIGINT)
			break;	/* Don't get rest of files if you interrupted. */
		fType = (int) dirFile->line[0];
		if (fType == 'd') {
			iName = dirFile->line + 1;
			if (GetDir(gopt, iName, rd, ld) < 0)
				break;
		}
	}

	free(ld);
	free(rd);
	DisposeLineListContents(&dirFiles);
	return (0);
	
fail:
	if (rd != NULL)
		free(rd);
	if (ld != NULL)
		free(ld);
	if (rf != NULL)
		free(rf);
	if (lf != NULL)
		free(lf);
	return (-1);
}	/* GetDir */




int RemoteFileType(char *fName)
{
	LineList fileList;
	char *cp;
	int result;
	int i;

	result = 0;
	InitLineList(&fileList);
	ListToMemory(&fileList, "LIST", kListDirNamesOnlyMode, fName);
	if (fileList.first != NULL) {
		cp = fileList.first->line;
		/* Do a quick check and see if it looks like a unix ls line. */
		for (i=1; i<=3; i++)
			if ((cp[i] != 'r') && (cp[i] != 'w') && (cp[i] != 'x') && (cp[i] != '-'))
				goto done;
		result = (int) cp[0];
	}
done:
	DisposeLineListContents(&fileList);
	return (result);
}	/* RemoteFileType */





int DoGetWithGlobbingAndRecursion(GetOptionsPtr gopt)
{
	int err;
	LineList globFiles;
	LinePtr globFile;
	char *cp;
	int fType;
	int result;
	longstring rcwd;
		
	err = 0;
	InitLineList(&globFiles);
	RemoteGlob(&globFiles, gopt->rName, kListNoFlags);
	
	for (globFile = globFiles.first; globFile != NULL;
		globFile = globFile->next)
	{
		if (gXferAbortFlag == SIGINT)
			break;	/* Don't get rest of files if you interrupted. */
		if (gopt->recursive) {
			fType = RemoteFileType(globFile->line);
			if (fType == 'd') {
				if ((cp = strrchr(globFile->line, '/')) != NULL) {
					/* If the user said something like
					 * "get -R /pub/a/b/c/d" we want to just write the
					 * contents of the 'd' as a subdirectory of the local
					 * directory, and not create ./pub, ./pub/a, etc.
					 */
					STRNCPY(rcwd, gRemoteCWD);
					*cp++ = '\0';
					if (DoChdir(globFile->line) == 0) {
						GetDir(gopt, cp, gRemoteCWD, gLocalCWD);
					}
					/* Restore the directory we were in before. */
					(void) DoChdir(rcwd);
				} else {
					/* Otherwise, the user gave a simple path, so it was
					 * something like "get -R pub"
					 */
					GetDir(gopt, globFile->line, gRemoteCWD, gLocalCWD);
				}
			} else if (fType == 'l') {
				EPrintF("Ignoring symbolic link '%s'\n",
					globFile->line);
			} else if (fType == '-') {
				goto regFile;
			}
		} else {
regFile:
			gopt->rName = globFile->line;
			gopt->lName = NULL;	/* Make it later. */
			result = DoGet(gopt);
			if (result < 0)
				err = -1;
		}
	}
	DisposeLineListContents(&globFiles);
	
	return (err);
}	/* DoGetWithGlobbingAndRecursion */




/* Fetch one or more remote files. */
int GetCmd(int argc, char **argv)
{
	int i, result, errs;
	GetOptions gopt;
	
	
	if (GetGetOptions(argc, argv, &gopt) == kUsageErr)
		return (kUsageErr);
		
	argv += gOptInd;
	argc -= gOptInd;
	errs = 0;

	if (gopt.noGlob || gopt.saveAs) {
		for (i=0; i<argc; i++) {
			gopt.rName = argv[i];
			if (gopt.saveAs) {
				if (++i < argc)
					gopt.lName = argv[i];	/* Use this name. */
				else
					return (kUsageErr);
			} else {
				gopt.lName = NULL;	/* Make it later. */
			}
			result = DoGet(&gopt);
			if (gXferAbortFlag == SIGINT)
				break;	/* Don't get rest of files if you interrupted. */
			if (result < 0)
				--errs;
		}
	} else {
		for (i=0; i<argc; i++) {
			gopt.rName = argv[i];
			errs += DoGetWithGlobbingAndRecursion(&gopt);
			if (gXferAbortFlag == SIGINT)
				break;	/* Don't get rest of files if you interrupted. */
		}
	}
	
	return (errs);
}	/* GetCmd */



int
CatFileToScreenProc(XferSpecPtr xp)
{
	int nread;
	int fd;
	char xbuf[256];
	char buf2[256];

	fd = xp->outStream;
	for (;;) {
		nread = BufferGets(xbuf, sizeof(xbuf), xp);
		if (nread <= 0)
			break;

		MakeStringPrintable(buf2, (unsigned char *) xbuf, sizeof(buf2));
		MultiLinePrintF("%s", buf2);
	}
	return (nread);	/* 0 or -1 */
}	/* CatFileToScreenProc */




/* Dump a remote file to the screen. */
int DoCat(char *remoteName)
{
	int result;
	XferSpecPtr xp;

	MultiLineInit();

	xp = InitXferSpec();
	xp->netMode = kNetReading;
	xp->outStream = gStdout;
	
	/* This group is needed for the progress reporting and logging stuff.
	 * Otherwise, it isn't that important.
	 */
	xp->doReports = kNoReports;
	xp->localFileName = kLocalFileIsStdout;
	xp->remoteFileName = remoteName;

	/* This is supposed to be done previously, if you wanted accurate
	 * file sizes from GetDateAndSize.
	 */
	SETASCII;
	
	/* Setup the parameter block to give to RDataCmd. */
	if (!isatty(xp->outStream))
		xp->xProc = StdAsciiFileReceive;	/* Faster */
	else
		xp->xProc = CatFileToScreenProc;
	/* xp->inStream = gDataSocket;  RDataCmd fills this in when it gets it. */

	/* Send the request and do the transfer. */
	result = RDataCmd(xp, "RETR %s", xp->remoteFileName);
	DoneWithXferSpec(xp);
	return (result);
}	/* DoCat */




/* We need to make something we can give to popen.  This is simple
 * if it is a plain file, but if they wanted to page a compressed
 * file we have to prepend the correct filter before the pager name.
 */
int MakePageCmdLine(char *cmd, size_t siz, char *remote_file)
{
	int useZCat;
	int useGZCat;
	int binaryPage;
	int len;
	
	useZCat = 0;
	useGZCat = 0;
	binaryPage = 0;
	
	len = (int) strlen(remote_file);

	if (len > 2) {
 		    if (remote_file[len - 2] == '.') {
			/* Check for .Z files. */
			if (remote_file[len-1] == 'Z')
				useZCat = 1;

			/* Check for .z (gzip) files. */
			if (remote_file[len - 1] == 'z')
				useGZCat = 1;
		}
	}

	if (len > 3) {
		/* Check for ".gz" (gzip) files. */
		if (STREQ(remote_file + len - 3, ".gz"))
			useGZCat = 1;
	}

	/* Run compressed remote files through zcat, then the pager.
	 * If GZCAT was defined, we also try paging gzipped files.
	 */	
	if (useGZCat) {
#ifdef GZCAT
		(void) Strncpy(cmd, GZCAT, siz);
		(void) Strncat(cmd, " | ", siz);
		(void) Strncat(cmd, gPager, siz);
#else
		PrintF("NcFTP wasn't configured to page gzipped files.\n");
#endif
	} else if (useZCat) {
#ifdef ZCAT
		(void) Strncpy(cmd, ZCAT, siz);
		(void) Strncat(cmd, " | ", siz);
		(void) Strncat(cmd, gPager, siz);
#else
#	ifdef GZCAT
		/* gzcat can do .Z's also. */
		(void) Strncpy(cmd, GZCAT, siz);
		(void) Strncat(cmd, " | ", siz);
		(void) Strncat(cmd, gPager, siz);
#	else
		PrintF("NcFTP wasn't configured to page compressed files.\n");
#	endif
#endif
	} else {
		(void) Strncpy(cmd, gPager, siz);
	}

	binaryPage = (useZCat || useGZCat);
	return (binaryPage);
}	/* MakePageCmdLine */




/* View a remote file through your pager. */
int DoPage(char *remoteName)
{
	FILE *fp;
	int result;
	longstring pageCmd;
	int binaryPage;
	XferSpecPtr xp;

	binaryPage = MakePageCmdLine(pageCmd, sizeof(pageCmd), remoteName);
	DebugMsg("%s page: %s\n",
		binaryPage ? "Binary" : "Ascii",
		pageCmd
	);

	fp = POpen(pageCmd, "w", 1);
	if (fp == NULL) {
		Error(kDoPerror, "Could not run %s.\n", pageCmd);
		return -1;
	}

	xp = InitXferSpec();
	xp->netMode = kNetReading;
	xp->outStream = fileno(fp);
	
	/* This group is needed for the progress reporting and logging stuff.
	 * Otherwise, it isn't that important.
	 */
	xp->doReports = kNoReports;
	xp->localFileName = kLocalFileIsStdout;
	xp->remoteFileName = remoteName;

	if (!binaryPage) {
		/* Try to use text mode for paging, so newlines get converted. */
		result = AsciiGet(xp);
	} else {
		/* Must use binary, or else zcat will complain about corrupted
		 * input files, since we'd be converting carriage-returns.
		 */
		result = BinaryGet(xp);
	}
	DoneWithXferSpec(xp);
	(void) PClose(fp);
	RestoreScreen(1);
	return (result);
}	/* DoPage */




/* View one or more remote files through your pager. */
int PageCmd(int argc, char **argv)
{
	int i, result, errs;
	LineList globFiles;
	LinePtr globFile;
	char *pagerProg;

	if (STREQ(argv[1], "-b") && (gWinInit > 0) && (argc > 2)) {
		/* A hack to let you use the built-in pager like you
		 * can with the lpage command.
		 */
		pagerProg = NULL;	/* Use built-in */
		i = 2;
	} else {
		if (gPager[0] == '\0') {
			EPrintF("You haven't specified a program to use as a pager.\n");
			EPrintF("You can set this from the preferences screen (prefs command).\n");
			return -1;
		}
		pagerProg = gPager;
		i = 1;
	}

	for (errs=0; i<argc; i++) {
		InitLineList(&globFiles);
		RemoteGlob(&globFiles, argv[i], kListNoFlags);
		for (globFile = globFiles.first; globFile != NULL;
			globFile = globFile->next)
		{
			if (pagerProg == NULL)
				result = DoCat(globFile->line);
			else
				result = DoPage(globFile->line);
			if (result < 0)
				--errs;
			if (gXferAbortFlag == SIGINT)
				break;	/* Don't get rest of files if you interrupted. */
		}
		DisposeLineListContents(&globFiles);
	}

	return (errs);
}	/* PageCmd */




/* View one or more remote files through your pager. */
int CatCmd(int argc, char **argv)
{
	int i, result, errs;
	LineList globFiles;
	LinePtr globFile;

	MultiLineInit();
	for (i=1, errs=0; i<argc; i++) {
		InitLineList(&globFiles);
		RemoteGlob(&globFiles, argv[i], kListNoFlags);
		for (globFile = globFiles.first; globFile != NULL;
			globFile = globFile->next)
		{
			result = DoCat(globFile->line);
			if (result < 0)
				--errs;
			if (gXferAbortFlag == SIGINT)
				break;	/* Don't get rest of files if you interrupted. */
			if (argc > 2)
				MultiLinePrintF("### End of file %s ###\n", globFile->line);
		}
		DisposeLineListContents(&globFiles);
	}

	return (errs);
}	/* CatCmd */
