

#include <security/checkpw.h>
#include <stdio.h>

int
main(int argv, char *argc[])
{
	char *uname = "local";
	char *pass = "local";
	int retval = 0, i = 0;

	for(i=0; i < 1024; i++)
	{
		retval = checkpw(uname, pass);
		if (0 != retval)
		{
			printf("Incorrect password.\n");
			break;
		}
	}

	return retval;
}
