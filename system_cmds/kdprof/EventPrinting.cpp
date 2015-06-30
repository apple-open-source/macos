//
//  EventPrinting.cpp
//  kdprof
//
//  Created by James McIlree on 6/6/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

void print_event_header(const Globals& globals, bool is_64_bit) {

	// Header is...
	//
	// [Index] Time  Type  Code arg1 arg2 arg3 arg4 thread cpu# command/IOP-name pid
	//      8    16     4  34   8/16 8/16 8/16 8/16     10    4 16               6


	if (globals.should_print_event_index())
		dprintf(globals.output_fd(), "%8s ", "Event#");

	// The character counting for "Time(µS)" is OBO, it treats the µ as two characters.
	// This means the %16s misaligns. We force it by making the input string 16 printable chars long,
	// which overflows the %16s to the correct actual output length.
	const char* time = globals.should_print_mach_absolute_timestamps() ? "Time(mach-abs)" : "        Time(µS)";

	if (is_64_bit)
		dprintf(globals.output_fd(), "%16s  %4s   %-34s %-16s %-16s %-16s %-16s %10s %4s %-16s %-6s\n", time, "Type", "Code", "arg1", "arg2", "arg3", "arg4", "thread", "cpu#", "command", "pid");
	else
		dprintf(globals.output_fd(), "%16s  %4s   %-34s %-8s %-8s %-8s %-8s %10s %4s %-16s %-6s\n", time, "Type", "Code", "arg1", "arg2", "arg3", "arg4", "thread", "cpu#", "command", "pid");
}
