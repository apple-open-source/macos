#include <TargetConditionals.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <xlocale.h>
#include <os/lock.h>
#include <sys/wait.h>

#include <darwintest.h>

#if TARGET_OS_OSX
T_DECL(locale_PR_23679075, "converts a cyrillic a to uppercase")
{
	locale_t loc = newlocale(LC_COLLATE_MASK|LC_CTYPE_MASK, "ru_RU", 0);
	T_ASSERT_NOTNULL(loc, "newlocale(LC_COLLATE_MASK|LC_CTYPE_MASK, \"ru_RU\", 0) should return a locale");

	T_ASSERT_EQ(towupper_l(0x0430, loc), 0x0410, NULL);
	freelocale(loc);
}

T_DECL(locale_PR_24165555, "swprintf with Russian chars")
{
    setlocale(LC_ALL, "ru_RU.UTF-8");

    wchar_t buffer[256];
    T_EXPECT_POSIX_SUCCESS(swprintf(buffer, 256, L"%ls", L"English: Hello World"), "English");
    T_EXPECT_POSIX_SUCCESS(swprintf(buffer, 256, L"%ls", L"Russian: ру́сский язы́к"), "Russian");

    setlocale(LC_ALL, "");
}

T_DECL(locale_PR_28774201, "return code on bad locale")
{
    T_EXPECT_NULL(newlocale(LC_COLLATE_MASK | LC_CTYPE_MASK, "foobar", NULL), NULL);
    T_EXPECT_EQ(errno, ENOENT, NULL);
}

static void *hammer_snprintf(void *hammer)
{
	while(*(bool *)hammer) {
		char buf[3];
		snprintf(buf, sizeof(buf), "%s%s", "a", "b");
	}

	return NULL;
}

#define HOW_MANY_FORKS 10000

T_DECL(locale_PR_91564505, "locale lock should be reset on fork")
{
	bool hammer = true;
	pthread_t thread;
	pthread_create(&thread, NULL, hammer_snprintf, &hammer);

	int i;
	for (i = 0; i < HOW_MANY_FORKS; i++) {
		pid_t pid = fork();
		if (pid < 0) {
			T_FAIL("fork() returned %d", pid);
		}

		if (pid) {
			int stat_loc;
			waitpid(pid, &stat_loc, 0);
			if(WIFSIGNALED(stat_loc)) {
				T_FAIL("child crashed post-fork pre-exec");
				break;
			}
		} else {
			exit(0);
		}
	}

	hammer = false;
	pthread_join(thread, NULL);

	T_EXPECT_EQ(i, HOW_MANY_FORKS, "We made it through %d forks without a post-fork pre-exec crash", i);
}
#endif
