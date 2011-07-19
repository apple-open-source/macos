

#include <security/checkpw.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *prompt = "checkpw test prompt:";

int
main(int argv, char *argc[])
{
	char *uname;
	int retval = 0;
	struct passwd *pw = NULL;

	uname = (char*)getenv("USER");
	if ( NULL == uname)
	{
		uid_t uid = getuid();
		struct passwd *pw = getpwuid(uid);
		uname = pw->pw_name;
	}

	retval = checkpw(uname, getpass(prompt));
	if (0 == retval)
	{
		printf("Password is okay.\n");
	} else {
		printf("Incorrect password.\n");
	}

	return retval;
}
