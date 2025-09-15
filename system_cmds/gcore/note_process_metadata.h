/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#ifndef _NOTE_PROCESS_METADATA_H
#define _NOTE_PROCESS_METADATA_H

#include <stdint.h>

// This note, its semantics, and how to construct it is documented at:
// https://stashweb.sd.apple.com/users/jmolenda/repos/coreutils/browse

/*
 * The payload for this LC_NOTE is in JSON. The top level is a dictionary.
 * It may have a key called 'threads'. 'threads' is an array, and it must have
 * the same number of entries (even if empty) as the number of LC_THREADs
 * in the corefile.
 *
 * Each thread's entry in threads is a dictionary, and currently the one
 * recognized key is thread_id which has a base-10 (numbers in JSON are
 * base-10) thread ID to be used for that thread. 'threads' entries which do
 * not have a thread_id will be assigned an arbitrary number by lldb.
 *
 * The JSON string is the size of this LC_NOTE, and a nul byte '\0' is not
 * required at the end of the string. Consumer must append a nul byte if
 * they want to treat it as a C-string.
 */

/* (No current structure -- it's just an array of JSON data) */

#endif /* _NOTE_PROCESS_METADATA_H */
