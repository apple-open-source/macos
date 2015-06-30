//
//  TraceCodes.h
//  KDBG
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef __kdprof__TraceCodes__
#define __kdprof__TraceCodes__

std::vector<std::string> default_trace_code_paths();
std::unordered_map<uint32_t, std::string> trace_codes_at_path(const char* path);

//
// Set output_fd to an open fd to print verbose output/warnings.
//
std::unordered_map<uint32_t, std::string> resolve_trace_codes(bool should_read_default_codes, int output_fd, std::vector<std::string>& additional_paths);

#endif /* defined(__kdprof__TraceCodes__) */
