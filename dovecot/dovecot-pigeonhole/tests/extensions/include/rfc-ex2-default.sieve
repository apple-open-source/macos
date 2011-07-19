require ["variables", "include", "relational", "fileinto"];
global "test";
global "test-mailbox";

# The included script may contain repetitive code that is
# effectively a subroutine that can be factored out.
set "test" "$$";
include "rfc-ex2-spam_filter_script";

set "test" "Make money";
include "rfc-ex2-spam_filter_script";

# Message will be filed according to the test that matched last.
if string :count "eq" "${test_mailbox}" "1"
{
	fileinto "INBOX${test_mailbox}";
	stop;
}

# If nothing matched, the message is implicitly kept.

