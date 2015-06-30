//
//  TraceCodes.cpp
//  KDBG
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "KDebug.h"

std::vector<std::string> default_trace_code_paths() {
	// As of 4/17/2013, this is a single file.
	return { "/usr/share/misc/trace.codes" };
}

std::unordered_map<uint32_t, std::string> trace_codes_at_path(const char* path)
{
	std::unordered_map<uint32_t, std::string> codes;

	if (FILE* fp = fopen(path, "r")) {
		char line[PATH_MAX];

		while (fgets(line, sizeof(line), fp)) {
			int code;
			char name[128];
			if (sscanf(line, "%x%127s\n", &code, name) == 2) {
				ASSERT(code != 0, "Should never have a code equal to zero");
				ASSERT(strlen(name), "Invalid name");
				codes[code] = name;
			}
		}

		fclose(fp);
	}

	return codes;
}

std::unordered_map<uint32_t, std::string> resolve_trace_codes(bool should_read_default_codes, int output_fd, std::vector<std::string>& additional_paths) {
	std::unordered_map<uint32_t, std::string> codes;

	std::vector<std::string> paths;

	if (should_read_default_codes) {
		std::vector<std::string> default_paths = default_trace_code_paths();
		paths.insert(paths.end(), default_paths.begin(), default_paths.end());
	}

	paths.insert(paths.end(), additional_paths.begin(), additional_paths.end());
	
	for (auto& path : paths) {
		std::unordered_map<uint32_t, std::string> partial = trace_codes_at_path(path.c_str());
		
		if (output_fd > -1) {
			dprintf(output_fd, "Read %zd codes from %s\n", partial.size(), path.c_str());
		}
		
		if (codes.empty()) {
			codes = std::move(partial);
		} else {
			for (auto& map_pair : partial) {
				auto insert_it = codes.insert(map_pair);
				if (insert_it.second == false) {
					if (map_pair.second != codes[map_pair.first]) {
						dprintf(output_fd, "WARNING: code entry for 0x%x has multiple entries (%s, %s)\n", map_pair.first, map_pair.second.c_str(), codes[map_pair.first].c_str());
					}
				}
			}
		}
	}

	return codes;
}
