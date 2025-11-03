#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

static void
do_setuser(const char *userstr)
{
	const char *errstr;
	struct passwd *pwd;

	pwd = getpwnam(userstr);
	if (pwd == NULL) {
		uid_t uid;

		uid = strtonum(userstr, 0, UID_MAX, &errstr);
		if (errstr == NULL)
			errx(1, "invalid user '%s': %s", userstr, errstr);

		pwd = getpwuid(uid);
	}

	if (pwd == NULL)
		errx(1, "invalid user '%s'", userstr);

	if (seteuid(pwd->pw_uid) != 0)
		err(1, "seteuid %d", pwd->pw_uid);
}

int
main(int argc, char *argv[])
{
	const char *uid;

	if (argc < 3)
		errx(1, "usage: %s user command...\n", argv[0]);

	uid = argv[1];
	do_setuser(uid);

	/* Forward to the command. */
	argc -= 2;
	argv += 2;

	execvp(argv[0], argv);
	err(1, "execvp");
}
