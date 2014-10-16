//
// Test driver program for cdsa_client library
//
#include "csptests.h"
#include "dltests.h"

#include <security_cdsa_client/cssmclient.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace CssmClient;
extern "C" void malloc_debug(int);

static void usage();

static const char *progname;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ main
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
int main(int argc, char *argv[])
{    
	extern char *optarg;
	extern int optind;
	bool didWork = false;
	bool autoCommit = true;
	bool printSchema = false;
	int ch;
	
	progname = strrchr(argv[0], '/');
	progname = progname ? progname + 1 : argv[0];

	try
	{
		while ((ch = getopt(argc, argv, "?haAbcdM:D:smwg:")) != -1)
		{
			switch(ch)
			{
			case 'a':
				autoCommit=true;
				break;
			case 'A':
				autoCommit=false;
				break;
			case 'b':
				setbuf(stdout, NULL);
				break;
			case 'c':
				csptests();
				didWork = true;
				break;
			case 'm':
				testmac();
				didWork = true;
				break;
			case 'w':
				testwrap();
				didWork = true;
				break;
			case 'd':
				dltests(autoCommit);
				didWork = true;
				break;
			case 's':
				printSchema = true;
				break;
			case 'g':
				if (strcmp (optarg, "AppleFileDL") == 0)
				{
					gSelectedFileGuid = &gGuidAppleFileDL;
				}
				else if (strcmp (optarg, "LDAPDL") == 0)
				{
					gSelectedFileGuid = &gGuidAppleLDAPDL;
				}
				else
				{
					didWork = false;
				}
				break;
			case 'D':
				dumpDb(optarg, printSchema);
				didWork = true;
				break;
			case 'M':
				malloc_debug(atoi(optarg));
				break;
			case '?':
			case 'h':
			default:
				usage();
			}
		}
	
		if (argc != optind)
			usage();
	
		if (!didWork)
			usage();

		Cssm::standard()->terminate();
	}
	catch (CommonError &error)
	{
		cssmPerror("Tester abort", error.osStatus());
	}

	return 0;
}    

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ usage
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
static void
usage()
{
	printf("usage: %s [-M<malloc_debug>] [-b] [-c] [[-a|-A] -d] [[-s ] [-g AppleFileDL | LDAPDL] -D <db_to_dump>]\n", progname);
	printf("        -M debug_level  Call malloc_debug(debug_level) to enable malloc debugging.\n");
	printf("        -b              turn off stdout buffering.\n");
	printf("        -c              run csp (rotty) tests.\n");
	printf("        -m              Test Mac\n");
	printf("        -w              Test Wrap\n");
	printf("        -d              run dl tests.\n");
	printf("        -a              Enable AutoCommit for dl modifications (default).\n");
	printf("        -A              Disable AutoCommit for dl modifications.\n");
	printf("        -D dbname       Dump a db into a human readable format.\n");
	printf("        -s              Dump out schema info (use with -D).\n");
    exit(1);
}

