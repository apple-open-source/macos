require ["variables", "include"];
global ["test", "test_mailbox"];

if header :contains "Subject" "${test}"
{
	set "test_mailbox" "spam-${test}";
}

