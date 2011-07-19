require "imap4flags"; 
require "fileinto";

/*
 * When keep/fileinto is used multiple times in a script and duplicate 
 * message elimination is performed, the last flag list value MUST win.
 */

setflag "IMPLICIT";

fileinto :flags "\\Seen \\Draft" "INBOX.Junk";
fileinto :flags "NONSENSE" "INBOX.Junk";

keep;
keep :flags "\\Seen";

fileinto :flags "\\Seen" "Inbox.Nonsense";
fileinto "Inbox.Nonsense";
