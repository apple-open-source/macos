/* Cmdlist.c */

#include "Sys.h"
#include "Curses.h"

#include "Util.h"
#include "Main.h"
#include "Open.h"
#include "Cmds.h"
#include "Glob.h"
#include "List.h"
#include "Get.h"
#include "Put.h"
#include "Hostwin.h"
#include "Prefs.h"
#include "Cmdline.h"
#include "Complete.h"

/* The command list.  I suppose they don't really need to be in alphabetical
 * order, but some order is better than total chaos.
 */
Command gCommands[] = {
	{ "?",
		kCmdHidden,
		HelpCmd,
		kNoMin, kNoMax,
		"[optional commands]",
		"shows commands, or detailed help on specified commands",
		kCompleteCmd
	},
	{ "!",
		0,
		ShellCmd,
		kNoMin, kNoMax,
		"[command]",
		"Executes specified command, or if none given, runs a shell",
		kCompleteNone
	},
	{ "ascii",
		kCmdMustBeConnected | kCmdHidden | kCmdWaitMsg,
		TypeCmd,
		0, 0,
		"",
		"sets the file transfer type to ASCII text",
		kCompleteNone
	},
	{ "binary",
		kCmdMustBeConnected | kCmdHidden | kCmdWaitMsg,
		TypeCmd,
		0, 0,
		"",
		"sets the file transfer type to binary/image",
		kCompleteNone
	},
	{ "bookmark",
		kCmdMustBeConnected,
		BookmarkCmd,
		1, 1,
		"<bookmark-name-to-save-as>",
		"Creates or updates a bookmark using the current host and directory",
		kCompleteHost
	},
	{ "bookmarks",
		kCmdMustBeDisconnected,
		(CmdProc) HostsCmd,
		kNoMin, kNoMax,
		"",
		"lets you edit the settings for each bookmark",
		kCompleteNone
	},
	{ "bye",
		kCmdHidden | kCmdWaitMsg,
		(CmdProc) QuitCmd,
		0, 0,
		"",
		"exits NcFTP",
		kCompleteNone
	},
	{ "cat",
		kCmdMustBeConnected,
		CatCmd,
		1, kNoMax,
		"file1 [file2...]",
		"views a file from the remote host",
		kCompleteFile
	},
	{ "cd",
		kCmdMustBeConnected | kCmdWaitMsg,
		ChdirCmd,
		1, 1,
		"<directory>",
		"changes remote working directory",
		kCompleteDir
	},
	{ "close",
		kCmdMustBeConnected | kCmdWaitMsg,
		(CmdProc) CloseCmd,
		0, 0,
		"",
		"closes the connection to the remote host",
		kCompleteNone
	},
	{ "create",
		kCmdMustBeConnected | kCmdWaitMsg,
		CreateCmd,
		1, 1,
		"empty-file-name",
		"creates an empty file on the remote host",
		kCompleteNone
	},
	{ "debug",
		0,
		DebugCmd,
		kNoMin, kNoMax,
		"[debug level]",
		"sets debug mode to level x",
		kCompleteNone
	},
	{ "delete",
		kCmdMustBeConnected | kCmdHidden | kCmdWaitMsg,
		DeleteCmd,
		1, kNoMax,
		"file1 [file2...]",
		"deletes files from the remote host",
		kCompleteFile
	},
	{ "dir",
		kCmdMustBeConnected | kCmdWaitMsg,
		ListCmd,
		kNoMin, kNoMax,
		"[items to list]",
		"prints a verbose directory listing",
		kCompleteFile
	},
	{ "echo",
		0,
		EchoCmd,
		kNoMin, kNoMax,
		"[items to echo]",
		"echos back to screen, expanding special % and @ sequences",
		kCompleteNone
	},
	{ "exit",
		kCmdHidden | kCmdWaitMsg,
		(CmdProc) QuitCmd,
		0, 0,
		"",
		"quits NcFTP",
		kCompleteNone
	},
	{ "get",
		kCmdMustBeConnected,
		GetCmd,
		1, kNoMax,
"[-flags] file1 [file2...]\n\
Flags:\n\
  -C   : Force continuation (reget).\n\
  -f   : Force overwrite.\n\
  -G   : Don't use wildcard matching.\n\
  -R   : Recursive.  Useful for fetching whole directories.\n\
  -n X : Get selected files only if X days old or newer.\n\
  -z   : Get the remote file X, and name it to Y.\n\
Examples:\n\
  get README\n\
  get README.*\n\
  get -G **Name.with.stars.in.it**\n\
  get -R new-files-directory\n\
  get -z WIN.INI ~/junk/windows-init-file\n",
		"fetches files from the remote host",
		kCompleteFile
	},
	{ "help",
		0,
		HelpCmd,
		kNoMin, kNoMax,
		"[optional commands]",
		"shows commands, or detailed help on specified commands",
		kCompleteCmd
	},
	{ "hosts",
		kCmdMustBeDisconnected | kCmdHidden,
		(CmdProc) HostsCmd,
		kNoMin, kNoMax,
		"",
		"lets you edit the settings for each remote host",
		kCompleteNone
	},
	{ "lcd",
		0,
		LocalChdirCmd,
		kNoMin, 1,
		"<directory>",
		"changes local working directory",
		kCompleteLocal
	},
	{ "less",
		kCmdMustBeConnected | kCmdNoRedirect | kCmdHidden,
		PageCmd,
		1, kNoMax,
		"file1 [file2...]",
		"views a file from the remote host one page at a time.",
		kCompleteFile
	},
	{ "lls",
		0,
		LocalListCmd,
		kNoMin, kNoMax,
		"[items to list]",
		"prints a local directory listing",
		kCompleteLocal
	},
	{ "lookup",
		0,
		LookupCmd,
		1, kNoMax,
		"<host or IP number> [<more hosts or IP numbers>]",
		"looks up information in the host database",
		kCompleteNone
	},
	{ "lpage",
		0,
		LocalPageCmd,
		1, kNoMax,
		"[-b | -p] localFiles...",
		"views a local file with your pager (-p) or built-in pager (-b)",
		kCompleteLocal
	},
	{ "lpwd",
		0,
		(CmdProc) LocalPwdCmd,
		0, 0,
		"",
		"Prints the current local working directory",
		kCompleteNone
	},
	{ "ls",
		kCmdMustBeConnected | kCmdWaitMsg,
		ListCmd,
		kNoMin, kNoMax,
		"[items to list]",
		"prints a remote directory listing",
		kCompleteFile
	},
	{ "mget",
		kCmdMustBeConnected | kCmdHidden,
		GetCmd,
		1, kNoMax,
		"file1 [file2...]\n",
		"fetches files from the remote host",
		kCompleteFile
	},
	{ "mkdir",
		kCmdMustBeConnected | kCmdWaitMsg,
		MkdirCmd,
		1, kNoMax,
		"dir1 [dir2...]",
		"creates directories on the remote host",
		kCompleteFile
	},
	{ "mode",
		kCmdMustBeConnected | kCmdWaitMsg,
		ModeCmd,
		1, 1,
		"stream | block",
		"sets file transfer mode",
		kCompleteNone
	},
	{ "more",
		kCmdMustBeConnected | kCmdNoRedirect | kCmdHidden,
		PageCmd,
		1, kNoMax,
		"file1 [file2...]",
		"views a file from the remote host one page at a time.",
		kCompleteFile
	},
	{ "mput",
		kCmdMustBeConnected,
		PutCmd,
		1, kNoMax,
		"file1 [file2...]\n",
		"sends files to the remote host",
		kCompleteLocal
	},
	{ "open",
		kCmdWaitMsg,
		OpenCmd,
		kNoMin, kNoMax,
"[-flags] [sitename]\n\
Flags:\n\
  -a   : Open anonymously.\n\
  -u   : Open with username and password prompt.\n\
  -p X : Use port number X when opening.\n\
  -r   : Redial until connected.\n\
  -d X : Redial, delaying X seconds between tries.\n\
  -g X : Give up after X redials without connection.\n\
Examples:\n\
  open sphygmomanometer.unl.edu\n\
  open -u bowser.nintendo.co.jp\n\
  open -r -d 75 -g 10 sphygmomanometer.unl.edu\n",
		"connects to a remote host",
		kCompleteHost
	},
	{ "page",
		kCmdMustBeConnected | kCmdNoRedirect,
		PageCmd,
		1, kNoMax,
		"file1 [file2...]",
		"views a file from the remote host one page at a time",
		kCompleteFile
	},
	{ "pdir",
		kCmdMustBeConnected | kCmdNoRedirect | kCmdWaitMsg,
		ListCmd,
		kNoMin, kNoMax,
		"[items to list]",
		"views a directory listing through your pager",
		kCompleteFile
	},
	{ "pls",
		kCmdMustBeConnected | kCmdNoRedirect | kCmdWaitMsg,
		ListCmd,
		kNoMin, kNoMax,
		"[items to list]",
		"views a directory listing through your pager",
		kCompleteFile
	},
	{ "predir",
		kCmdMustBeConnected | kCmdNoRedirect,
		RedirCmd,
		kNoMin, kNoMax,
		"",
		"re-displays the last directory listing through your pager",
		kCompleteNone
	},
#ifdef USE_CURSES
	{ "prefs",
		0,
		(CmdProc) PrefsCmd,
		0, 0,
		"",
		"lets you configure the program's settings",
		kCompleteNone
	},
#endif	/* CURSES */
	{ "put",
		kCmdMustBeConnected,
		PutCmd,
		1, kNoMax,
"[-flags] file1 [file2...]\n\
Flags:\n\
  -z   : Send the local file X, and name the remote copy to Y.\n\
Examples:\n\
  put README\n\
  put -z ~/junk/windows-init-file WIN.INI\n",
		"sends a file to the remote host",
		kCompleteLocal
	},
	{ "pwd",
		kCmdMustBeConnected | kCmdWaitMsg,
		(CmdProc) PwdCmd,
		0, 0,
		"",
		"Prints the current remote working directory",
		kCompleteNone
	},
	{ "redir",
		kCmdMustBeConnected,
		RedirCmd,
		kNoMin, kNoMax,
		"",
		"re-displays the last directory listing",
		kCompleteNone
	},
	{ "rename",
		kCmdMustBeConnected | kCmdWaitMsg,
		RenameCmd,
		2, 2,
		"oldname newname",
		"changes the name of a file on the remote host",
		kCompleteFile
	},
	{ "rglob",
		kCmdMustBeConnected | kCmdHidden | kCmdWaitMsg,
		RGlobCmd,
		1, kNoMax,
		"regex",
		"tests remote filename wildcard matching",
		kCompleteFile
	},
	{ "q",
		kCmdHidden | kCmdWaitMsg,
		(CmdProc) QuitCmd,
		0, 0,
		"",
		"quits NcFTP",
		kCompleteNone
	},
	{ "quit",
		kCmdWaitMsg,
		(CmdProc) QuitCmd,
		0, 0,
		"",
		"take a wild guess",
		kCompleteNone
	},
	{ "quote",
		kCmdMustBeConnected | kCmdWaitMsg,
		QuoteCmd,
		1, kNoMax,
		"command-string",
		"sends an FTP command to the remote server",
		kCompleteFile
	},
	{ "rhelp",
		kCmdMustBeConnected | kCmdWaitMsg,
		RmtHelpCmd,
		kNoMin, kNoMax,
		"[help string]",
		"requests help from the remote server",
		kCompleteNone
	},
	{ "rm",
		kCmdMustBeConnected | kCmdWaitMsg,
		DeleteCmd,
		1, kNoMax,
		"file1 [file2...]",
		"deletes files from the remote host",
		kCompleteFile
	},
	{ "rmdir",
		kCmdMustBeConnected | kCmdWaitMsg,
		RmdirCmd,
		1, kNoMax,
		"dir1 [dir2...]",
		"deletes directories from the remote host",
		kCompleteDir
	},
	{ "set",
		0,
		SetCmd,
		0, 2,
		"[option [newvalue]] | all | help",
		"lets you configure a program setting from the command line",
		kCompleteOption
	},
	{ "site",
		kCmdMustBeConnected | kCmdWaitMsg,
		QuoteCmd,
		1, kNoMax,
		"command-string",
		"sends a host-specific FTP command to the remote server",
		kCompleteNone
	},
	{ "type",
		kCmdMustBeConnected | kCmdWaitMsg,
		TypeCmd,
		1, 1,
		"ascii | binary | image",
		"sets file transfer type (one of 'ascii' or 'binary')",
		kCompleteNone
	},
	{ "verbose",
		kCmdHidden,
		VerboseCmd,
		kNoMin, 1,
		"[verbosity level]",
		"sets verbosity mode to 0, 1, 2, or 3",
		kCompleteNone
	},
	{ "version",
		0,
		(CmdProc) VersionCmd,
		kNoMin, kNoMax,
		"",
		"prints version information",
		kCompleteNone
	},
	{ "x",
		kCmdHidden | kCmdWaitMsg,
		(CmdProc) QuitCmd,
		0, 0,
		"",
		"quits NcFTP",
		kCompleteNone
	},
};

int gNumCommands = ((int) (sizeof(gCommands) / sizeof(Command)));

/* eof */
