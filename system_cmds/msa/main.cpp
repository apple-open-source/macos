//
//  main.cpp
//  msa
//
//  Created by James McIlree on 1/30/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include <CPPUtil/CPPUtil.h>

#include "global.h"

bool isVerbose = true;
bool shouldPrintVersion = true;
std::vector<std::string> procsOfInterest;
bool interestedInEverything = false;

__attribute__((noreturn)) void usage(const char *errorMsg) {
	if (errorMsg) {
		printf("%s\n", errorMsg);
		exit(1);
	}

	// const char* BOLD = "\033[1m";
	// const char* UNBOLD = "\033[0m";

	// printf("01234567890123456789012345678901234567890123456789012345678901234567890123456789\n");
	printf("msa [options]\n\n");
	printf("  GLOBAL OPTIONS\n\n");
	printf("    -h, --help                     Print this message\n");
	printf("        --verbose                  Print additional information\n");
	printf("        --version                  Print version info\n");
	printf("\n");
	printf("  TRACE COLLECTION OPTIONS\n\n");
	printf("    -i, --initialize #             Set the size of the kernel trace buffer\n");
        printf("        --no-voucher-contents      Disable collecting voucher contents\n");
	printf("    -L path                        Capture and save trace output to path\n");
	printf("\n");
	printf("  OUTPUT OPTIONS\n\n");
	printf("        --lifecycle all|user|none\n");
	printf("                                   Set filter level for lifecycle events\n");
	printf("        --mach-msg all|user|voucher|none\n");
	printf("                                   Set filter level for mach msg events\n");
	printf("    -o, --output path              Print output to path\n");
	printf("        --raw-timestamps           Print timestamps as raw values, not deltas\n");
	printf("        --mach-absolute-time       Print timestamps in mach absolute time\n");
	printf("        --event-index              Print the index of each event\n");
	printf("\n");
	exit(1);
}

template <typename SIZE>
static bool check_interest_name(const MachineProcess<SIZE>& process) {
	if (interestedInEverything)
		return true;

	const char* name = process.name();
	for (auto& proc : procsOfInterest) {
		if (strcmp(name, proc.c_str()) == 0)
			return true;
	}

	return false;
}

static std::unique_ptr<Action> create_read_trace_file_action(const char* trace_file_path) {
	if (Path::is_file(trace_file_path, true)) {
		char resolved_path[PATH_MAX];
		if (realpath(trace_file_path, resolved_path)) {
			return std::make_unique<ReadTraceFileAction>(resolved_path);
		}
	}
	char* errmsg = NULL;
	asprintf(&errmsg, "%s does not exist or is not a file", trace_file_path);
	usage(errmsg);
}

static std::vector<std::unique_ptr<Action>> parse_arguments(int argc, const char* argv[], Globals& globals) {
	int i = 1;

	std::vector<std::unique_ptr<Action>> actions;

	while (i < argc) {
		const char* arg = argv[i];
		if ((strcmp(arg, "-h") == 0) || (strcasecmp(arg, "--help") == 0)) {
			usage(NULL);
		} else if ((strcmp(arg, "-v") == 0) || strcasecmp(arg, "--verbose") == 0) {
			globals.set_is_verbose(true);
		} else if (strcasecmp(arg, "--version") == 0) {
			shouldPrintVersion = true;
		} else if ((strcmp(arg, "-i") == 0) || strcasecmp(arg, "--initialize") == 0) {
			if (++i >= argc)
				usage("--initialize requires an argument");

			arg = argv[i];
			char* endptr;
			uint32_t temp = (uint32_t)strtoul(arg, &endptr, 0);
			if (*endptr == 0) {
				globals.set_trace_buffer_size(temp);
			} else {
				usage("Unable to parse --initialize argument");
			}
		} else if (strcasecmp(arg, "--no-voucher-contents") == 0) {
			globals.set_should_trace_voucher_contents(false);
		} else if (strcasecmp(arg, "-L") == 0) {
			if (++i >= argc)
				usage("-L requires an argument");

			arg = argv[i];
			actions.push_back(std::make_unique<WriteTraceFileAction>(arg));
		} else if (strcasecmp(arg, "--lifecycle") == 0) {
			if (++i >= argc)
				usage("--lifecycle requires an argument");

			arg = argv[i];
			if (strcasecmp(arg, "all") == 0) {
				globals.set_lifecycle_filter(kLifecycleFilter::All);
			} else if (strcasecmp(arg, "user") == 0) {
				globals.set_lifecycle_filter(kLifecycleFilter::User);
			} else if (strcasecmp(arg, "none") == 0) {
				globals.set_lifecycle_filter(kLifecycleFilter::None);
			} else {
				usage("Unrecognized --lifecycle value");
			}
		} else if (strcasecmp(arg, "--mach-msg") == 0) {
			if (++i >= argc)
				usage("--mach-msg requires an argument");

			arg = argv[i];
			if (strcasecmp(arg, "all") == 0) {
				globals.set_mach_msg_filter(kMachMsgFilter::All);
			} else if (strcasecmp(arg, "user") == 0) {
				globals.set_mach_msg_filter(kMachMsgFilter::User);
			} else if (strcasecmp(arg, "voucher") == 0) {
				globals.set_mach_msg_filter(kMachMsgFilter::Voucher);
			}  else if (strcasecmp(arg, "none") == 0) {
				globals.set_mach_msg_filter(kMachMsgFilter::None);
			} else {
				usage("Unrecognized --mach-msg value");
			}
		} else if ((strcmp(arg, "-o") == 0) || strcasecmp(arg, "--output") == 0) {
			if (++i >= argc)
				usage("--output requires an argument");

			FileDescriptor desc(argv[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (!desc.is_open()) {
				char* errmsg = NULL;
				asprintf(&errmsg, "Unable to create output file at %s", argv[i]);
				usage(errmsg);
			}
			globals.set_output_fd(std::move(desc));
		} else if (strcasecmp(arg, "--raw-timestamps") == 0) {
			globals.set_should_zero_base_timestamps(false);
		} else if (strcasecmp(arg, "--mach-absolute-time") == 0) {
			globals.set_should_print_mach_absolute_timestamps(true);
		} else if (strcasecmp(arg, "--event-index") == 0) {
			globals.set_should_print_event_index(true);
		} else {
			//
			// Last attempts to divine argument type/intent.
			//
			std::string temp(arg);

			if (ends_with(temp, ".trace")) {
				actions.push_back(create_read_trace_file_action(argv[i]));
				goto no_error;
			}

			//
			// ERROR!
			//
			char error_buffer[PATH_MAX];
			snprintf(error_buffer, sizeof(error_buffer), "Unhandled argument: %s", arg);
			usage(error_buffer);
		}

	no_error:

		i++;
	}

	if (actions.empty()) {
		actions.push_back(std::make_unique<LiveTraceAction>());
	}

	return actions;
}

int main(int argc, const char * argv[])
{
	//
	// Use host values as defaults.
	// User overrides as needed via flags.
	//
	Globals globals;
	auto actions = parse_arguments(argc, argv, globals);
	
	interestedInEverything = procsOfInterest.empty();

	// globals.set_should_print_mach_absolute_timestamps(true);

	for (auto& action : actions) {
		action->execute(globals);
	}

	return 0;
}

