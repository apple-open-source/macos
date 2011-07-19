require "fileinto";

/* Three store actions */

if address :contains "to" "frop.example" {
	/* #1 */
	fileinto "INBOX.VB";
}

/* #2 */
fileinto "INBOX.backup";

/* #3 */
keep;

/* Duplicate of keep */
fileinto "INBOX";
