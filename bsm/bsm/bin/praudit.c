
/* 
 * Tool used to parse audit records conforming to the BSM structure
 */   

/*
 * praudit [-lrs] [-ddel] [filenames]
 */   

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libbsm.h>

extern char *optarg;
extern int optind, optopt, opterr,optreset;

static char *del = ","; /* default delimiter */
static int oneline = 0;
static int raw = 0;
static int shortfrm = 0;
static int partial = 0;

static void usage()
{
	printf("Usage: praudit [-lrs] [-ddel] [filenames]\n");
	exit(1);
}
/*
 * token printing for each token type 
 */
static int print_tokens(FILE *fp)
{
	u_char *buf;
	tokenstr_t tok;
	int reclen;
   	int bytesread;

	/* allow tail -f | praudit to work */
        if (partial) {
            u_char type = 0;
            /* record must begin with a header token */
            do {
                type = fgetc(fp);
            } while(type != AU_HEADER_32_TOKEN);
            ungetc(type, fp);
        }

	while((reclen = au_read_rec(fp, &buf)) != -1) {

		bytesread = 0;

		while (bytesread < reclen) {

			if(-1 == au_fetch_tok(&tok, buf + bytesread, reclen - bytesread)) {
				/* is this an incomplete record ? */
				break;
			}

			au_print_tok(stdout, &tok, del, raw, shortfrm);
			bytesread += tok.len;

			if(oneline) {
				printf("%s", del);
			}
			else {
				printf("\n");
			}
		}

		free(buf);

		if(oneline) {
			printf("\n");
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	char ch;
	int i;
	FILE  *fp;
	
	while((ch = getopt(argc, argv, "lprsd:")) != -1) {
		switch(ch) {

			case 'l': 
					oneline = 1;
					break;

			case 'r':
					if(shortfrm)
						usage(); /* exclusive from shortfrm */
					raw = 1;
					break;

			case 's':
					if(raw)
						usage(); /* exclusive from raw */
					shortfrm = 1;
					break;	

			case 'd': 
					del = optarg;
					break;

                        case 'p':
                                        partial = 1;
                                        break;

			case '?':
			default :
					usage();
		}
	}

	/* For each of the files passed as arguments dump the contents */
	if(optind == argc) {
		print_tokens(stdin);
		return 1;
	}
	for (i = optind; i < argc; i++) {
		fp = fopen(argv[i], "r");
		if((fp == NULL) || (-1 == print_tokens(fp))) {
			perror(argv[i]);
		}
		if(fp != NULL)
			fclose(fp);	
	}
	return 1;
}
