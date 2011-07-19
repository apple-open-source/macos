/* Generated from prof_err.et */

#include <stddef.h>
#include <Heimdal/com_err.h>
#include "prof_err.h"

#define N_(x) (x)

static const char *prof_error_strings[] = {
	/* 000 */ N_("Profile version 0.0"),
	/* 001 */ N_("Bad magic value in profile_node"),
	/* 002 */ N_("Profile section not found"),
	/* 003 */ N_("Profile relation not found"),
	/* 004 */ N_("Attempt to add a relation to node which is not a section"),
	/* 005 */ N_("A profile section header has a non-zero value"),
	/* 006 */ N_("Bad linked list in profile structures"),
	/* 007 */ N_("Bad group level in profile structures"),
	/* 008 */ N_("Bad parent pointer in profile structures"),
	/* 009 */ N_("Bad magic value in profile iterator"),
	/* 010 */ N_("Can't set value on section node"),
	/* 011 */ N_("Invalid argument passed to profile library"),
	/* 012 */ N_("Attempt to modify read-only profile"),
	/* 013 */ N_("Profile section header not at top level"),
	/* 014 */ N_("Syntax error in profile section header"),
	/* 015 */ N_("Syntax error in profile relation"),
	/* 016 */ N_("Extra closing brace in profile"),
	/* 017 */ N_("Missing open brace in profile"),
	/* 018 */ N_("Bad magic value in profile_t"),
	/* 019 */ N_("Bad magic value in profile_section_t"),
	/* 020 */ N_("Iteration through all top level section not supported"),
	/* 021 */ N_("Invalid profile_section object"),
	/* 022 */ N_("No more sections"),
	/* 023 */ N_("Bad nameset passed to query routine"),
	/* 024 */ N_("No profile file open"),
	/* 025 */ N_("Bad magic value in profile_file_t"),
	/* 026 */ N_("Couldn't open profile file"),
	/* 027 */ N_("Section already exists"),
	/* 028 */ N_("Invalid boolean value"),
	/* 029 */ N_("Invalid integer value"),
	/* 030 */ N_("Bad magic value in profile_file_data_t"),
	NULL
};

#define num_errors 31

void initialize_prof_error_table_r(struct et_list **list)
{
    initialize_error_table_r(list, prof_error_strings, num_errors, ERROR_TABLE_BASE_prof);
}

void initialize_prof_error_table(void)
{
    init_error_table(prof_error_strings, ERROR_TABLE_BASE_prof, num_errors);
}
