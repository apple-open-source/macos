//
//  SummaryPrinting.cpp
//  kdprof
//
//  Created by James McIlree on 4/19/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

constexpr const char* const SummaryLineData::indent_string[];

void print_summary_header(const Globals& globals) {
	// Header is...
	//                                                                                                                                                         Avg     Actual     Wanted   Actual   Wanted                                                                       Jetsam
	//                                                                              All CPU                       Thr Avg        Actual        Wanted  Concurrency  Processes     To Run  Threads   To Run  VMFault       VMFault       IO Wait     # IO    IO Bytes     Jetsam    Proc
	// [Time(mS)]        Name                               Run%    Idle%    Intr%    Idle%    #Intr      #CSW  On CPU/µS        CPU/mS        CPU/mS      (# CPU)        Ran  Processes      Ran  Threads    Count     Time (mS)     Time (mS)      Ops   Completed  Time (mS)   Count
	// 123456789abcdef0  123456789012345678901234567890  1234567  1234567  1234567  1234567  1234567  12345678  123456789  123456789abc  123456789abc  123456789ab  123456789  123456789  1234567  1234567  1234567  123456789abc  123456789abc  1234567  1234567890  123456789  123456
	//    1119100000.00                                    76.58    16.53     6.89     0.00      230       112   10000.00     100000.00     100000.00         1.55          2          3       12       13     2280        230.48       1998.22     3318   123.40 MB       0.00

	const char* time1 = "";
	const char* time2 = "";
	const char* time3 = "";
	char time_buffer1[32];
	char time_buffer2[32];
	char time_buffer3[32];

	// If we're printing the entire data set, don't print a timestamp.
	if (globals.is_summary_start_set() || globals.is_summary_stop_set() || globals.is_summary_step_set()) {
		sprintf(time_buffer1, "%16s  ", "");
		sprintf(time_buffer2, "%16s  ", "");
		sprintf(time_buffer3, "%-16s  ", globals.should_print_mach_absolute_timestamps() ? "Time(mach-abs)" : "Time(mS)");

		time1 = time_buffer1;
		time2 = time_buffer2;
		time3 = time_buffer3;
	}

	dprintf(globals.output_fd(), "%s%-30s  %7s  %7s  %7s  %7s  %7s  %8s  %9s  %12s  %12s  %11s  %9s  %9s  %7s  %7s  %7s  %12s  %12s  %7s  %10s  %9s  %6s\n", time1,     "",     "",      "",      "",        "",      "",     "",          "",       "",       "",         "Avg",    "Actual",    "Wanted",  "Actual",  "Wanted",        "",          "",          "",     "",          "",          "", "Jetsam");
	dprintf(globals.output_fd(), "%s%-30s  %7s  %7s  %7s  %7s  %7s  %8s  %9s  %12s  %12s  %11s  %9s  %9s  %7s  %7s  %7s  %12s  %12s  %7s  %10s  %9s  %6s\n", time2,     "",     "",      "",      "", "All-CPU",      "",     "",   "Thr Avg", "Actual", "Wanted", "Concurrency", "Processes",    "To Run", "Threads",  "To Run", "VMFault",   "VMFault",   "IO Wait", "# IO",  "IO Bytes",    "Jetsam",   "Proc");
	dprintf(globals.output_fd(), "%s%-30s  %7s  %7s  %7s  %7s  %7s  %8s  %9s  %12s  %12s  %11s  %9s  %9s  %7s  %7s  %7s  %12s  %12s  %7s  %10s  %9s  %6s\n", time3, "Name", "Run%", "Idle%", "Intr%",   "Idle%", "#Intr", "#CSW", "On CPU/µS", "CPU/mS", "CPU/mS",     "(# CPU)",       "Ran", "Processes",     "Ran", "Threads",   "Count", "Time (mS)", "Time (mS)",  "Ops", "Completed", "Time (mS)",  "Count");
}
