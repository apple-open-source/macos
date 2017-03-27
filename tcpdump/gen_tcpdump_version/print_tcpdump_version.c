//
//  main.c
//  gen_tcpdump_version.sh
//
//  Created by Vincent Lubet on 2/7/17.
//
//

#include <stdio.h>

#include "tcpdump_version.h"

int main(int argc, const char * argv[]) {
	printf("%s\n", version);
	return 0;
}
