/*
 * Copyright (c) 2007 - 2021 Apple Inc.
 *
 * This is the MIT license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/xattr.h>

static const char *attr_name = NULL;
static const char *attr_value = NULL;
static int cflag = 0; // clear all attributes
static int lflag = 0; // print long format
static int rflag = 0; // recursive
static int sflag = 0; // do not follow symbolic links
static int pflag = 0; // print attribute
static int wflag = 0; // write attribute
static int dflag = 0; // delete attribute
static int vflag = 0; // verbose
static int xflag = 0; // hexadecimal

static int nfiles = 0;
static int status = EXIT_SUCCESS;
static const char *prgname = NULL;

typedef void (*attribute_iterator) (int fd, const char *filename, const char *attr_name);

__attribute__((__format__ (__printf__, 2, 3)))
static void
usage(int exit_code, const char *error_format, ...)
{
	if (error_format != NULL) {
		va_list ap;
		va_start(ap, error_format);
		vprintf(error_format, ap);
		printf("\n\n");
		va_end(ap);
	}

	printf("usage: %s [-l] [-r] [-s] [-v] [-x] file [file ...]\n", prgname);
	printf("       %s -p [-l] [-r] [-s] [-v] [-x] attr_name file [file ...]\n", prgname);
	printf("       %s -w [-r] [-s] [-x] attr_name attr_value file [file ...]\n", prgname);
	printf("       %s -d [-r] [-s] attr_name file [file ...]\n", prgname);
	printf("       %s -c [-r] [-s] file [file ...]\n", prgname);
	printf("\n");
	printf("The first form lists the names of all xattrs on the given file(s).\n");
	printf("The second form (-p) prints the value of the xattr attr_name.\n");
	printf("The third form (-w) sets the value of the xattr attr_name to the string attr_value.\n");
	printf("The fourth form (-d) deletes the xattr attr_name.\n");
	printf("The fifth form (-c) deletes (clears) all xattrs.\n");
	printf("\n");
	printf("options:\n");
	printf("  -h: print this help\n");
	printf("  -l: print long format (attr_name: attr_value and hex output has offsets and\n");
	printf("      ascii representation)\n");
	printf("  -r: act recursively\n");
	printf("  -s: act on the symbolic link itself rather than what the link points to\n");
	printf("  -v: also print filename (automatic with -r and with multiple files)\n");
	printf("  -x: attr_value is represented as a hex string for input and output\n");

	exit(exit_code);
}

__attribute__((__format__ (__printf__, 2, 3)))
static void
print_error(const char *filename, const char *error_format, ...)
{
	status = EXIT_FAILURE;

	int access_flags = 0;
	if (sflag) {
		access_flags |= AT_SYMLINK_NOFOLLOW;
	}
	if (faccessat(AT_FDCWD, filename, F_OK, access_flags) != 0) {
		int saved_errno = errno;
		if (saved_errno == ENOENT) {
			fprintf(stderr, "%s: No such file: %s\n", prgname, filename);
			return;
		}
	}

	if (error_format != NULL) {
		va_list ap;
		va_start(ap, error_format);
		fprintf(stderr, "%s: ", prgname);
		vfprintf(stderr, error_format, ap);
		fprintf(stderr, "\n");
		va_end(ap);
	}
}

static void
print_errno(const char *filename, const char *attr_name, int err)
{
	if (attr_name && (err == ENOATTR)) {
		print_error(filename, "%s: No such xattr: %s", filename, attr_name);
	} else {
		print_error(filename, "[Errno %d] %s: '%s'", err, strerror(err), filename);
	}
}

static size_t
get_filename_prefix_length(const char *filename)
{
	// The extra two bytes hold ": ", see asprintf() in
	// get_filename_prefix() below
	if (vflag || rflag || nfiles > 1) {
		return strlen(filename) + 2;
	}
	return 0;
}

static char *
get_filename_prefix(const char *filename)
{
	char *filename_with_prefix = NULL;
	size_t len = get_filename_prefix_length(filename);
	if (len > 0) {
		int res = asprintf(&filename_with_prefix, "%s: ", filename);
		if (res == -1) {
			err(1, "Failed to allocate memory");
		}
		return filename_with_prefix;
	}

	filename_with_prefix = strdup("");
	if (!filename_with_prefix) {
		err(1, "Failed to allocate memory");
	}
	return filename_with_prefix;
}

static void
iterate_all_attributes(int fd, const char *filename, attribute_iterator iterator_func)
{
	ssize_t res = 0;
	int saved_errno = 0;
	char *buf = NULL, *cur = NULL;

	res = flistxattr(fd, NULL, 0, 0);
	if (res == -1) {
		saved_errno = errno;
		goto done;
	}

	buf = malloc(res);
	if (!buf) {
		err(1, "Failed to allocate memory");
	}

	res = flistxattr(fd, buf, res, 0);
	if (res == -1) {
		saved_errno = errno;
		goto done;
	}

	cur = buf;
	while (res > 0) {
		const char *attr_name = cur;
		size_t attr_len = strlen(cur) + 1;
		iterator_func(fd, filename, attr_name);
		cur += attr_len;
		res -= attr_len;
	}

done:
	if (saved_errno != 0) {
		print_errno(filename, NULL, saved_errno);
	}
	free(buf);
}

static char *
get_printable_segment(const char *segment, size_t segment_len)
{
	char *printable_segment = strndup(segment, segment_len);
	if (!printable_segment) {
		err(1, "Failed to allocate memory");
	}

	for (size_t i = 0; i < segment_len; i++) {
		if (!isprint(printable_segment[i])) {
			printable_segment[i] = '.';
		}
	}
	return printable_segment;
}

static void
print_one_xattr_hex(const char *attr_value, size_t attr_value_len)
{
	static int incr = 16;
	size_t bytes_left = attr_value_len;

	for (size_t i = 0; i < attr_value_len; i += incr) {
		const char *segment = attr_value + i;
		size_t segment_len = MIN(incr, bytes_left);

		char *segment_hex = malloc((3 * segment_len + 1) * sizeof(char));
		if (!segment_hex) {
			err(1, "Failed to allocate memory");
		}

		for (size_t j = 0; j < segment_len; j++) {
			sprintf(segment_hex + 3 * j, "%02X ", (unsigned char)segment[j]);
		}

		if (lflag) {
			char *printable_segment = get_printable_segment(segment, segment_len);
			printf("%08lX  %-*s |%s|\n", i, incr * 3, segment_hex, printable_segment);
			free(printable_segment);
		} else {
			printf("%s\n", segment_hex);
		}

		bytes_left -= segment_len;

		free(segment_hex);
	}
	if (lflag) {
		printf("%08lX\n", attr_value_len);
	}
}

static void
print_one_xattr(const char *filename, const char *attr_name, const char *attr_value, size_t attr_value_len)
{
	char *filename_with_prefix = get_filename_prefix(filename);

	if (lflag) {
		if (xflag) {
			printf("%s%s:\n", filename_with_prefix, attr_name);
			print_one_xattr_hex(attr_value, attr_value_len);
		} else {
			printf("%s%s: %s\n", filename_with_prefix, attr_name, attr_value);
		}
	} else {
		if (pflag) {
			if (xflag) {
				if (get_filename_prefix_length(filename) > 0) {
					printf("%s\n", filename_with_prefix);
				}
				print_one_xattr_hex(attr_value, attr_value_len);
			} else {
				printf("%s%s\n", filename_with_prefix, attr_value);
			}
		} else {
			printf("%s%s\n", filename_with_prefix, attr_name);
		}
	}

	free(filename_with_prefix);
}

static void
read_attribute(int fd, const char *filename, const char *name)
{
	ssize_t res = 0;
	int saved_errno = 0;
	char *buf = NULL;

	res = fgetxattr(fd, name, NULL, 0, 0, 0);
	if (res == -1) {
		saved_errno = errno;
		goto done;
	}

	buf = malloc(res + 1);
	if (!buf) {
		err(1, "Failed to allocate memory");
	}

	res = fgetxattr(fd, name, buf, res, 0, 0);
	if (res == -1) {
		saved_errno = errno;
		goto done;
	}
	buf[res] = '\0';
	print_one_xattr(filename, name, buf, (size_t) res);

done:
	if (saved_errno != 0) {
		print_errno(filename, name, saved_errno);
	}

	free(buf);
}

static void
list_all_attributes(int fd, const char *filename)
{
	iterate_all_attributes(fd, filename, read_attribute);
}

static void
delete_attribute(int fd, const char *filename, const char *name)
{
	if (fremovexattr(fd, name, 0) == -1) {
		int saved_errno = errno;
		// Don't print ENOATTR errors when deleting recursively
		if (!rflag || (saved_errno != ENOATTR)) {
			print_errno(filename, name, saved_errno);
		}
	}
}

static void
clear_all_attributes(int fd, const char *filename)
{
	iterate_all_attributes(fd, filename, delete_attribute);
}

static char *
hex_to_ascii_value(const char *hexstr, size_t *ascii_len)
{
	char *buf = NULL;
	size_t len = strlen(hexstr);
	size_t i = 0, j = 0;

	if ((len % 2) != 0) {
		len++;
	}

	buf = malloc(((len / 2) + 1) * sizeof(char));
	if (!buf) {
		err(1, "Failed to allocate memory");
	}

	// i points into hexstr, j points into the buffer
	while (hexstr[i] != '\0') {
		unsigned int hex = '\0';
		if (isspace(hexstr[i])) {
			i++;
			continue;
		}

		sscanf(hexstr + i, "%02X", &hex);
		buf[j++] = (char) hex;

		// skip two bytes
		i += 2;
	}

	if (ascii_len != NULL) {
		*ascii_len = j;
	}

	buf[j] = '\0';
	return buf;
}

static void
write_attribute(int fd, const char *filename, const char *name, const char *value)
{
	const char *actual_value = NULL;
	char *buf = NULL;
	size_t len = 0;

	if (xflag) {
		buf = hex_to_ascii_value(value, &len);
		actual_value = buf;
	} else {
		len = strlen(value);
		actual_value = value;
	}

	if (fsetxattr(fd, name, actual_value, len, 0, 0) == -1) {
		int saved_errno = errno;
		print_errno(filename, NULL, saved_errno);
	}
	free(buf);
}

static void
process_one_path(const char *filename, const char *name, const char *value)
{
	struct stat sb = { 0, };
	int is_link = 0;
	int saved_errno = 0;
	int fd = -1;
	DIR *dir = NULL;

	int oflags = O_RDONLY;
	if (sflag) {
		oflags |= O_SYMLINK;
	}

	// First, we lstat() the path to find out if it's a symlink
	if (lstat(filename, &sb) == -1) {
		saved_errno = errno;
		goto done;
	}

	is_link = S_ISLNK(sb.st_mode);

	// Note: this follows symlinks unless sflag = 1
	fd = open(filename, oflags);
	if (fd == -1) {
		saved_errno = errno;
		goto done;
	}

	if (fstat(fd, &sb) == -1) {
		saved_errno = errno;
		goto done;
	}

	if (rflag && !is_link && S_ISDIR(sb.st_mode)) {
		struct dirent *dirent = NULL;
		dir = fdopendir(fd);
		if (dir == NULL) {
			saved_errno = errno;
			goto done;
		}
		while ((dirent = readdir(dir)) != NULL) {
			if (strcmp(dirent->d_name, ".") == 0 ||
			    strcmp(dirent->d_name, "..") == 0) {
				continue;
			}

			char *dir_path = NULL;
			int res = asprintf(&dir_path, "%s/%s", filename, dirent->d_name);
			if (res == -1) {
				err(1, "Failed to allocate memory");
			}

			process_one_path(dir_path, name, value);
			free(dir_path);
		}
	}

	if (wflag) {
		write_attribute(fd, filename, name, value);
	} else if (dflag) {
		delete_attribute(fd, filename, name);
	} else if (cflag) {
		clear_all_attributes(fd, filename);
	} else if (pflag) {
		read_attribute(fd, filename, name);
	} else {
		list_all_attributes(fd, filename);
	}

done:
	if (dir != NULL) {
		(void)closedir(dir);
		dir = NULL;
		// closedir() also closes the underlying fd
		fd = -1;
	}
	if (fd != -1) {
		(void)close(fd);
	}
	if (saved_errno != 0) {
		print_errno(filename, NULL, saved_errno);
	}
}

int
main(int argc, const char * argv[])
{
	int ch, req_args = 0, main_opt = -1, argind = 0;
	const struct option long_opts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	prgname = basename((char *)argv[0]);
	if (!prgname) {
		prgname = argv[0];
	}

	while ((ch = getopt_long(argc, (char * const *) argv, "hclrspwdvx", long_opts, NULL)) != -1) {
		switch (ch) {
			case 'h':
				usage(EXIT_SUCCESS, NULL);
				break;
			case 'c':
				cflag = 1;
				if ((main_opt != -1) && (main_opt != ch))
					usage(EXIT_FAILURE, "Cannot specify -%c with -%c", ch, main_opt);
				main_opt = ch;
				req_args = 1;
				break;
			case 'l':
				lflag = 1;
				break;
			case 'r':
				rflag = 1;
				break;
			case 's':
				sflag = 1;
				break;
			case 'p':
				pflag = 1;
				if ((main_opt != -1) && (main_opt != ch))
					usage(EXIT_FAILURE, "Cannot specify -%c with -%c", ch, main_opt);
				main_opt = ch;
				req_args = 2;
				break;
			case 'w':
				wflag = 1;
				if ((main_opt != -1) && (main_opt != ch))
					usage(EXIT_FAILURE, "Cannot specify -%c with -%c", ch, main_opt);
				main_opt = ch;
				req_args = 3;
				break;
			case 'd':
				dflag = 1;
				if ((main_opt != -1) && (main_opt != ch))
					usage(EXIT_FAILURE, "Cannot specify -%c with -%c", ch, main_opt);
				main_opt = ch;
				req_args = 2;
				break;
			case 'v':
				vflag = 1;
				break;
			case 'x':
				xflag = 1;
				break;
			case '?':
				// getopt_long() will print an error message when it fails to parse
				usage(EXIT_FAILURE, "");
				break;
			default:
				break;
		}
	}
	argv += optind;
	argc -= optind;

	if (lflag && (wflag || dflag)) {
		usage(EXIT_FAILURE, "-l is not allowed with -w or -d");
	}

	if (main_opt == -1) {
		// Default mode is to list all attributes. We expect at least one file path
		req_args = 1;
	}

	if (argc < req_args) {
		usage(EXIT_FAILURE, "Not enough arguments for option -%c. Expected at least %d but got %d", main_opt, req_args, argc);
	}

	if (pflag || wflag || dflag) {
		attr_name = argv[argind++];
	}

	if (wflag) {
		attr_value = argv[argind++];
	}

	nfiles = (argc - argind);

	while (argind < argc) {
		const char *filename = argv[argind++];
		process_one_path(filename, attr_name, attr_value);
	}

	return status;
}
