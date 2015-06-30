//
//  UtilPath.inline.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/8/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

#include <sys/stat.h>

BEGIN_UTIL_NAMESPACE

std::string Path::basename(const char* path) {
	size_t length = strlen(path);

	/*
	 * case: ""
	 * case: "/"
	 * case: [any-single-character-paths]
	 */
	if (length < 2)
		return std::string(path);

	char temp[PATH_MAX];
	char* temp_cursor = &temp[PATH_MAX - 1];
	char* temp_end = temp_cursor;
	*temp_end = 0; // NULL terminate

	const char* path_cursor = &path[length-1];

	while (path_cursor >= path) {
		if (*path_cursor == '/') {
			// If we have copied one or more chars, we're done
			if (temp_cursor != temp_end)
				return std::string(temp_cursor);
		} else {
			*(--temp_cursor) = *path_cursor;
		}

		// Is the temp buffer full?
		if (temp_cursor == temp)
			return std::string(temp);

		--path_cursor;
	}

	if (path[0] == '/' && temp_cursor == temp_end) {
		*(--temp_cursor) = '/';
	}

	return std::string(temp_cursor);
}

std::string Path::basename(std::string& path) {
	return basename(path.c_str());
}

bool Path::exists(const char *path) {
	struct stat statinfo;
	return lstat(path, &statinfo) == 0;
}

bool Path::exists(std::string& path) {
	return exists(path.c_str());
}

bool Path::is_file(const char* path, bool should_resolve_symlinks) {
	struct stat statinfo;
	if (should_resolve_symlinks) {
		if (stat(path, &statinfo) == 0) {
			if (S_ISREG(statinfo.st_mode)) {
				return true;
			}
		}
	} else {
		if (lstat(path, &statinfo) == 0) {
			if (S_ISREG(statinfo.st_mode)) {
				return true;
			}
		}
	}

	return false;
}

bool Path::is_file(std::string& path, bool should_resolve_symlinks) {
	return is_file(path.c_str(), should_resolve_symlinks);
}

END_UTIL_NAMESPACE
