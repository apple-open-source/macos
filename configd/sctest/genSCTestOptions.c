/*
 * Copyright (c) 2016-2023 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <mach/boolean.h>
#include <getopt.h>

typedef struct {
	const char *scope; // Either "global" or for a particular command
	const char *optionString; // The option
	const char *optionKey; // String representation of the key to be used to access this option's value from the "options" dictionary.
	int hasArg; // no_argument, required_argument, optional_argument
	const char *usageString; // help string
} SCTestOption;

static const char licenseString[] = "\
/*\n\
* Copyright (c) 2016-2022 Apple Inc. All rights reserved.\n\
*\n\
* @APPLE_LICENSE_HEADER_START@\n\
*\n\
* This file contains Original Code and/or Modifications of Original Code\n\
* as defined in and that are subject to the Apple Public Source License\n\
* Version 2.0 (the 'License'). You may not use this file except in\n\
* compliance with the License. Please obtain a copy of the License at\n\
* http://www.opensource.apple.com/apsl/ and read it before using this\n\
* file.\n\
*\n\
* The Original Code and all software distributed under the License are\n\
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER\n\
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,\n\
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,\n\
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.\n\
* Please see the License for the specific language governing rights and\n\
* limitations under the License.\n\
*\n\
* @APPLE_LICENSE_HEADER_END@\n\
*/\n";

//	Add the options below and then once finished, go to ${SRCROOT}/sctest/ in Terminal and type "make".
//	The options should then be ready to use, through the "options" dictionary.

// Tests available on all platforms
static SCTestOption testOptionsAllPlatforms[] = {
	{"global", "cpu", "kSCTestGlobalOptionCPU", no_argument,
		"Prints the CPU usage after the test completes"},
	{"global", "help", "kSCTestGlobalOptionHelp", no_argument,
		"Prints this very useful help!"},
	{"global", "time", "kSCTestGlobalOptionTime", no_argument,
		"Prints the time elapsed since the test was launched"},
	{"global", "verbose", "kSCTestGlobalOptionVerbose", no_argument,
		"Enables verbose mode"},
	{"global", "wait", "kSCTestGlobalOptionWait", no_argument,
		"Results in a wait for 'sctest'"},

	{"unit_test", "command", "kSCTestUnitTestCommand", required_argument,
		"Run a unit test for a specific command. If this option is not specified, unit-tests for all commands will be run"},
	{"unit_test", "list_tests", "kSCTestUnitTestListTests", no_argument,
		"List the test commands in a JSON format. This is for NPT compliance"},
	{"unit_test", "test_method", "kSCTestUnitTestTestMethod", required_argument,
		"Runs a specific unit test. List can be obtained by using the 'test_method_list' option"},
	{"unit_test", "test_method_list", "kSCTestUnitTestTestMethodList", no_argument,
		"Lists all the unit tests. A specific one can be run using the 'test_method' option"},

	{"dynamic_store", "dns", "kSCTestDynamicStoreOptionDNS", no_argument,
		"Prints the global DNS information from the SCDynamicStore"},
	{"dynamic_store", "ipv4", "kSCTestDynamicStoreOptionIPv4", no_argument,
		"Prints the global IPv4 information from the SCDynamicStore"},
	{"dynamic_store", "ipv6", "kSCTestDynamicStoreOptionIPv6", no_argument,
		"Prints the global IPv6 information from the SCDynamicStore"},
	{"dynamic_store", "proxies", "kSCTestDynamicStoreOptionProxies", no_argument,
		"Prints the global Proxy information from the SCDynamicStore"}
};

// Tests available on non-bridgeOS platforms
static SCTestOption testOptionsNonBridgeOS[] = {
	{"preferences", "service_list", "kSCTestPreferencesServiceList", no_argument,
		"Prints the Network Services list from the preferences"},
	{"preferences", "service_order", "kSCTestPreferencesServiceOrder", no_argument,
		"Prints the Network Service order from the preferences"},

	{"config_agent", "dns_domain", "kSCTestConfigAgentDNSDomains", required_argument,
		"Configures the DNS Servers for certain domains. A comma-separated list of domains can be specified. Default is 'apple.com'"},
	{"config_agent", "dns_servers", "kSCTestConfigAgentDNSServers", required_argument,
		"Configures the specified DNS Servers. A comma-separated list of IP Addresses can be specified"},
	{"config_agent", "remove_dns", "kSCTestConfigAgentRemoveDNS", no_argument,
		"Remove a dns configuration, previously configured via 'sctest'"},

	{"config_agent", "ftp_proxy", "kSCTestConfigAgentFTPProxy", required_argument,
		"Add a proxy agent with FTP proxy. Format of the argument is 'server:port'"},
	{"config_agent", "gopher_proxy", "kSCTestConfigAgentGopherProxy", required_argument,
		"Add a proxy agent with Gopher proxy. Format of the argument is 'server:port'"},
	{"config_agent", "http_proxy", "kSCTestConfigAgentHTTPProxy", required_argument,
		"Add a proxy agent with HTTP proxy. Format of the argument is 'server:port'"},
	{"config_agent", "https_proxy", "kSCTestConfigAgentHTTPSProxy", required_argument,
		"Add a proxy agent with HTTPS proxy. Format of the argument is 'server:port'"},
	{"config_agent", "proxy_match_domain", "kSCTestConfigAgentProxyMatchDomain", required_argument,
		"Configures the Proxy server for certain domains. A comma-separated list of domains can be specified. Default is 'apple.com'"},
	{"config_agent", "remove_proxy", "kSCTestConfigAgentRemoveProxy", no_argument,
		"Remove a proxy configuration, previously configured via 'sctest'"},
	{"config_agent", "socks_proxy", "kSCTestConfigAgentSOCKSProxy", required_argument,
		"Add a proxy agent with SOCKS proxy. Format of the argument is 'server:port'"},

	{"reachability", "address", "kSCTestReachabilityAddress", required_argument,
		"Determine reachability to this address"},
	{"reachability", "host", "kSCTestReachabilityHost", required_argument,
		"Determine reachability to this host"},
	{"reachability", "interface", "kSCTestReachabilityInterface", required_argument,
		"Determine reachability when scoped to this interface"},
	{"reachability", "watch", "kSCTestReachabilityWatch", no_argument,
		"Watch for reachability changes"},

	{"rank_assertion", "interface_rank", "kSCTestInterfaceRankAssertion", no_argument,
		"Runs a unit test for interface rank assertion"},
	{"rank_assertion", "service_rank", "kSCTestServiceRankAssertion", no_argument,
		"Runs a unit test for interface rank assertion"}
};

static int testOptionsAllPlatformsCount = (sizeof(testOptionsAllPlatforms) / sizeof(testOptionsAllPlatforms[0]));
static int testOptionsNonBridgeOSCount = (sizeof(testOptionsNonBridgeOS) / sizeof(testOptionsNonBridgeOS[0]));

static void
printDeclarations(SCTestOption* testOptions, int testOptionsCount)
{
	// Prints to SCTestOptions.h
	const char *keyTemplate = "extern const NSString * const";
	for (int i = 0; i < testOptionsCount; i++) {
		char buffer[256] = {0};
		snprintf(buffer, sizeof(buffer), "%s %s;", keyTemplate, testOptions[i].optionKey);
		printf("%s\n", buffer);
	}
}

static void
printOptionEntries(SCTestOption* testOptions, int testOptionsCount)
{
	// Prints to SCTestOptions.h
	for (int i = 0; i < testOptionsCount; i++) {
		printf("\t\t{\"%s\", %d, NULL, 0}, \\", testOptions[i].optionString, testOptions[i].hasArg);
		printf("\n");
	}
}

static void
printUsage(SCTestOption* testOptions, int testOptionsCount)
{
	// Prints to SCTestOptions.h
	const char *last = "";
	for (int i = 0; i < testOptionsCount; i++) {
		if (strcmp(last, testOptions[i].scope)) {
			last = testOptions[i].scope;
			printf("\t\t\"\\n============== %s options =============\\n\"\\\n", testOptions[i].scope);
		}

		printf("\t\t\"-%-20s: %s\\n\"\\\n", testOptions[i].optionString, testOptions[i].usageString);
	}
}

static void
printDefinitions(SCTestOption* testOptions, int testOptionsCount)
{
	// Prints to SCTestOptions.m
	const char *definitionTemplate = "const NSString * const";
	for (int i = 0; i < testOptionsCount; i++) {
		char buffer[256] = {0};
		snprintf(buffer, sizeof(buffer), "%s %-50s= @\"%s_Str\";", definitionTemplate, testOptions[i].optionKey, testOptions[i].optionString);
		printf("%s\n", buffer);
	}
}

static void
printWithTargetConditionals(void (*f)(SCTestOption*, int),
			    const char * const _Nullable beginningString1,
			    const char * const _Nullable endingString1,
			    const char * const _Nullable beginningString2,
			    const char * const _Nullable endingString2)
{
	// Function necessary because we can't have target conditionals within macro definitions
	char targetConditionalBuffer[256];
	memset(targetConditionalBuffer, 0, sizeof(targetConditionalBuffer));

	// Prints beginning string if any
	if (beginningString1 != NULL && strcmp(beginningString1, "") != 0) {
		char stringBuffer[256];
		memset(stringBuffer, 0, sizeof(stringBuffer));
		snprintf(stringBuffer, sizeof(stringBuffer), "%s", beginningString1);
		printf("%s \\\n", stringBuffer);
	}

	// Prints tests available on all platforms
	f(testOptionsAllPlatforms, testOptionsAllPlatformsCount);

	// Prints ending string if any
	if (endingString1 != NULL && strcmp(endingString1, "") != 0) {
		char stringBuffer[256];
		memset(stringBuffer, 0, sizeof(stringBuffer));
		snprintf(stringBuffer, sizeof(stringBuffer), "%s", endingString1);
		printf("%s", stringBuffer);
	}

	// Prints tests available on all platforms and ones that are non-bridgeOS specific
	snprintf(targetConditionalBuffer, sizeof(targetConditionalBuffer), "#if !TARGET_OS_BRIDGE");
	printf("%s\n", targetConditionalBuffer);
	memset(targetConditionalBuffer, 0, sizeof(targetConditionalBuffer));

	// Prints beginning string if any
	if (beginningString2 != NULL && strcmp(beginningString2, "") != 0) {
		char stringBuffer[256];
		memset(stringBuffer, 0, sizeof(stringBuffer));
		snprintf(stringBuffer, sizeof(stringBuffer), "%s", beginningString2);
		printf("%s \\\n", stringBuffer);
	}

	// Prints tests available on non-bridgeOS platforms
	f(testOptionsNonBridgeOS, testOptionsNonBridgeOSCount);

	// Prints ending string if any
	if (endingString2 != NULL && strcmp(endingString2, "") != 0) {
		char stringBuffer[256];
		memset(stringBuffer, 0, sizeof(stringBuffer));
		snprintf(stringBuffer, sizeof(stringBuffer), "%s", endingString2);
		printf("%s", stringBuffer);
	}

	snprintf(targetConditionalBuffer, sizeof(targetConditionalBuffer), "#endif // !TARGET_OS_BRIDGE");
	printf("%s\n", targetConditionalBuffer);
}

int
main(int argc, char * argv[])
{
	char * type = "";

	if (argc >= 2) {
		type = argv[1];
	}

	// License
	printf("%s\n", licenseString);

	if (strcmp(type, "header") == 0) {
		// Preamble
		printf("//\n");
		printf("// This file is automatically generated. DO NOT EDIT!\n");
		printf("// To add options, see genSCTestOptions.c\n");
		printf("//\n\n");

		// Import header files
		printf("#import <TargetConditionals.h>\n");
		printf("#import <Foundation/Foundation.h>\n");
		printf("#import <getopt.h>");
		printf("\n\n");

		// Print the declarations
		printWithTargetConditionals(printDeclarations, NULL, NULL, NULL, NULL);
		printf("\n");

		// Print the option entries
		printWithTargetConditionals(printOptionEntries,
					    "#define kSCTestOptionEntriesAllPlatforms",
					    "\n",
					    "#define kSCTestOptionEntriesNonBridgeOS",
					    "\n");
		printf("\n");

		// Print the usage
		printWithTargetConditionals(printUsage,
					    "#define kSCTestOptionHelpAllPlatforms",
					    "\n",
					    "#define kSCTestOptionHelpNonBridgeOS",
					    "\n");
	} else if (strcmp(type, "mfile") == 0) {
		printf("//\n");
		printf("// This file is automatically generated. DO NOT EDIT!\n");
		printf("// To add options, see genSCTestOptions.c\n");
		printf("//\n");

		// Import header files
		printf("#import <TargetConditionals.h>\n");
		printf("#import \"SCTestOptions.h\"");

		printf("\n\n");
		printWithTargetConditionals(printDefinitions, NULL, NULL, NULL, NULL);
	}
	return 0;
}
