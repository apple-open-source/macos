//
//  gen_libpcap_version.c
//
//  Created by Vincent Lubet on 2/2/17.
//
//

#include <stdio.h>

#include "pcap_version.h"

int main(int argc, const char * argv[]) {
	// insert code here...
	printf("%s\n", pcap_version_string);
	return 0;
}
