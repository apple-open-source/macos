#!/usr/bin/env python3
"""
test_fortify_source.py

This verifies that when _FORTIFY_SOURCE is enabled, the appropriate functions
get defined to a _chk or _ptrchk variant (and if necessary, that the size is
queried with the right kind of __builtin_object_size invocation).

IF YOU ARE ADDING A CHECKED VARIANT FOR A FUNCTION:
After adding your variant, run this script with -g to generate the new expected
normal. Replace expected values in the PARAMETERS arrays with what you get.

IF YOU ARE ADDING A NEW CONDITIONAL MODE:
If you conditionally add a checked variant based on a C version or POSIX
standard version, make sure that there is a TestParameters entry that covers the
case where you expect the builtin to be used and at least one case where it is
not. Add entries as needed.

Run the script at desk with:

% python3 fortify_source.py --verbose --check-syntax --include-dir $(DSTROOT)/usr/include fortify_source.c

Don't forget to test with each SDK variant (ie, Darwin userspace, DriverKit, etc).
"""
from argparse import ArgumentParser
from collections import namedtuple
import os
import re
import subprocess
import sys

BADFUNC = "BADFUNC"
UNDEFINED = "UNDEFINED"
BUILTIN_OBSZ = "BUILTIN_OBSZ"
BUILTIN_OBSZ0 = "BUILTIN_OBSZ0"
PTRCHECK_OBSZ = "PTRCHECK_OBSZ"
PTRCHECK_OBSZ0 = "PTRCHECK_OBSZ0"

TestParameters = namedtuple(typename="TestParameters", field_names=["posix_c_source", "darwin_c_level", "fbounds_safety", "expected_driverkit", "expected_userspace"])

PARAMETERS = [
	TestParameters(posix_c_source=None, darwin_c_level=0, fbounds_safety=None,
		expected_driverkit={"bcopy": UNDEFINED, "bzero": UNDEFINED, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": UNDEFINED, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": UNDEFINED, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": UNDEFINED, "strlcpy": UNDEFINED, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": UNDEFINED, "vsprintf": UNDEFINED, },
		expected_userspace={"bcopy": UNDEFINED, "bzero": UNDEFINED, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": UNDEFINED, "sprintf": BUILTIN_OBSZ0, "stpcpy": UNDEFINED, "stpncpy": UNDEFINED, "strcat": BUILTIN_OBSZ0, "strcpy": BUILTIN_OBSZ0, "strlcat": UNDEFINED, "strlcpy": UNDEFINED, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": UNDEFINED, "vsprintf": UNDEFINED, },),

	TestParameters(posix_c_source=None, darwin_c_level="200112", fbounds_safety=None,
		expected_driverkit={"bcopy": UNDEFINED, "bzero": UNDEFINED, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": UNDEFINED, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": UNDEFINED, "strlcpy": UNDEFINED, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": UNDEFINED, },
		expected_userspace={"bcopy": UNDEFINED, "bzero": UNDEFINED, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": BUILTIN_OBSZ0, "stpcpy": UNDEFINED, "stpncpy": UNDEFINED, "strcat": BUILTIN_OBSZ0, "strcpy": BUILTIN_OBSZ0, "strlcat": UNDEFINED, "strlcpy": UNDEFINED, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": BUILTIN_OBSZ0, },),

	TestParameters(posix_c_source=None, darwin_c_level="200809", fbounds_safety=None,
		expected_driverkit={"bcopy": UNDEFINED, "bzero": UNDEFINED, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": BUILTIN_OBSZ0, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": UNDEFINED, "strlcpy": UNDEFINED, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": UNDEFINED, },
		expected_userspace={"bcopy": UNDEFINED, "bzero": UNDEFINED, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": BUILTIN_OBSZ0, "stpcpy": BUILTIN_OBSZ0, "stpncpy": BUILTIN_OBSZ0, "strcat": BUILTIN_OBSZ0, "strcpy": BUILTIN_OBSZ0, "strlcat": UNDEFINED, "strlcpy": UNDEFINED, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": BUILTIN_OBSZ0, },),

	TestParameters(posix_c_source=None, darwin_c_level="__DARWIN_C_FULL", fbounds_safety=None,
		expected_driverkit={"bcopy": BUILTIN_OBSZ, "bzero": BUILTIN_OBSZ, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": BUILTIN_OBSZ0, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": BUILTIN_OBSZ0, "strlcpy": BUILTIN_OBSZ0, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": UNDEFINED, },
		expected_userspace={"bcopy": BUILTIN_OBSZ, "bzero": BUILTIN_OBSZ, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": BUILTIN_OBSZ0, "stpcpy": BUILTIN_OBSZ0, "stpncpy": BUILTIN_OBSZ0, "strcat": BUILTIN_OBSZ0, "strcpy": BUILTIN_OBSZ0, "strlcat": BUILTIN_OBSZ0, "strlcpy": BUILTIN_OBSZ0, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": BUILTIN_OBSZ0, },),

	TestParameters(posix_c_source="0", darwin_c_level="__DARWIN_C_FULL", fbounds_safety=None,
		expected_driverkit={"bcopy": BUILTIN_OBSZ, "bzero": BUILTIN_OBSZ, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": BUILTIN_OBSZ0, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": BUILTIN_OBSZ0, "strlcpy": BUILTIN_OBSZ0, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": UNDEFINED, },
		expected_userspace={"bcopy": BUILTIN_OBSZ, "bzero": BUILTIN_OBSZ, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": BUILTIN_OBSZ0, "stpcpy": BUILTIN_OBSZ0, "stpncpy": BUILTIN_OBSZ0, "strcat": BUILTIN_OBSZ0, "strcpy": BUILTIN_OBSZ0, "strlcat": BUILTIN_OBSZ0, "strlcpy": BUILTIN_OBSZ0, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": BUILTIN_OBSZ0, },),

	TestParameters(posix_c_source=None, darwin_c_level="__DARWIN_C_FULL", fbounds_safety=None,
		expected_driverkit={"bcopy": BUILTIN_OBSZ, "bzero": BUILTIN_OBSZ, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": BUILTIN_OBSZ0, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": BUILTIN_OBSZ0, "strlcpy": BUILTIN_OBSZ0, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": UNDEFINED, },
		expected_userspace={"bcopy": BUILTIN_OBSZ, "bzero": BUILTIN_OBSZ, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": BUILTIN_OBSZ0, "stpcpy": BUILTIN_OBSZ0, "stpncpy": BUILTIN_OBSZ0, "strcat": BUILTIN_OBSZ0, "strcpy": BUILTIN_OBSZ0, "strlcat": BUILTIN_OBSZ0, "strlcpy": BUILTIN_OBSZ0, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": BUILTIN_OBSZ0, },),

	TestParameters(posix_c_source=None, darwin_c_level="__DARWIN_C_FULL", fbounds_safety="on",
		expected_driverkit={"bcopy": BUILTIN_OBSZ, "bzero": BUILTIN_OBSZ, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": BUILTIN_OBSZ0, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": BUILTIN_OBSZ0, "strlcpy": BUILTIN_OBSZ0, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": UNDEFINED, },
		expected_userspace={"bcopy": BUILTIN_OBSZ, "bzero": BUILTIN_OBSZ, "memccpy": BUILTIN_OBSZ, "memmove": BUILTIN_OBSZ, "memcpy": BUILTIN_OBSZ, "memset": BUILTIN_OBSZ, "snprintf": BUILTIN_OBSZ0, "sprintf": BUILTIN_OBSZ0, "stpcpy": BUILTIN_OBSZ0, "stpncpy": BUILTIN_OBSZ0, "strcat": BUILTIN_OBSZ0, "strcpy": BUILTIN_OBSZ0, "strlcat": BUILTIN_OBSZ0, "strlcpy": BUILTIN_OBSZ0, "strncat": BUILTIN_OBSZ0, "strncpy": BUILTIN_OBSZ0, "vsnprintf": BUILTIN_OBSZ0, "vsprintf": BUILTIN_OBSZ0, },),

	TestParameters(posix_c_source=None, darwin_c_level="__DARWIN_C_FULL", fbounds_safety="on_attributes",
		expected_driverkit={"bcopy": PTRCHECK_OBSZ, "bzero": PTRCHECK_OBSZ, "memccpy": PTRCHECK_OBSZ, "memmove": PTRCHECK_OBSZ, "memcpy": PTRCHECK_OBSZ, "memset": PTRCHECK_OBSZ, "snprintf": PTRCHECK_OBSZ0, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": UNDEFINED, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": PTRCHECK_OBSZ, "strlcpy": PTRCHECK_OBSZ, "strncat": PTRCHECK_OBSZ, "strncpy": UNDEFINED, "vsnprintf": PTRCHECK_OBSZ0, "vsprintf": UNDEFINED, },
		expected_userspace={"bcopy": PTRCHECK_OBSZ, "bzero": PTRCHECK_OBSZ, "memccpy": PTRCHECK_OBSZ, "memmove": PTRCHECK_OBSZ, "memcpy": PTRCHECK_OBSZ, "memset": PTRCHECK_OBSZ, "snprintf": PTRCHECK_OBSZ0, "sprintf": UNDEFINED, "stpcpy": UNDEFINED, "stpncpy": UNDEFINED, "strcat": UNDEFINED, "strcpy": UNDEFINED, "strlcat": PTRCHECK_OBSZ, "strlcpy": PTRCHECK_OBSZ, "strncat": UNDEFINED, "strncpy": UNDEFINED, "vsnprintf": PTRCHECK_OBSZ0, "vsprintf": UNDEFINED, },),
]


def parse_substitution_dictionary(stream):
	test_line_re = re.compile(rb"^test_(\w+):\s*([^(;\s]*).*$")
	result = {}
	for line in stream:
		m = test_line_re.match(line)
		if not m:
			continue
		key = m.group(1).decode("ASCII")
		if m.group(2) == b"undefined":
			result[key] = UNDEFINED
			continue

		# for all substitutions except bcopy/bzero (which boil down to memcpy/memset),
		# we check that the result line contains the right function name
		colon = line.index(b":") + 1
		if line.find(b"_" + m.group(1) + b"_", colon) == -1:
			if key not in ["bcopy", "bzero"]:
				result[key] = BADFUNC
				continue

		if m.group(2) == "":
			if "__libc_ptrchk_strbuf_chk" in line:
				result[key] = PTRCHECK_OBSZ
		else:
			builtin = b"__builtin" in m.group(2)
			obsz = False
			idx = line.find(b"__builtin_object_size")
			if idx != -1:
				open = line.index(b",", idx) + 1
				close = line.find(b")", open)
				obsz = line[open:close].strip() != b"0"

			if (builtin, obsz) == (True, True):
				result[key] = BUILTIN_OBSZ0
			elif (builtin, obsz) == (True, False):
				result[key] = BUILTIN_OBSZ
			elif (builtin, obsz) == (False, True):
				result[key] = PTRCHECK_OBSZ0
			elif (builtin, obsz) == (False, False):
				result[key] = PTRCHECK_OBSZ
	return result


def print_substitution_dictionary(d):
	print("{", end="")
	for k in sorted(d.keys()):
		print('"%s": %s, ' % (k, d[k]), end="")
	print("},")


def main():
	parser = ArgumentParser(description="test that bounds-sensitive functions have _chk substitutions")
	parser.add_argument("test_file", default="./fortify_source.c", help="file to preprocess")
	parser.add_argument("--sdk", default=os.getenv("SDKROOT"), help="SDK to test for (defaults to $SDKROOT)")
	parser.add_argument("-g", "--generate", action="store_true", help="print observed output instead of comparing it with desired output")
	parser.add_argument("-I", "--include-dir", action="append", default=[], help="Additional include paths (you can point this to the usr/include of a libc root)")
	parser.add_argument("-v", "--verbose", action="store_true", help="Print clang commands that are executed")
	parser.add_argument("-s", "--check-syntax", action="store_true", help="Also run clang with -fsyntax-only to check that this actually builds")
	arguments = parser.parse_args()
	if arguments.sdk is None:
		sys.stderr.write("*** $SDKROOT is not set\n")
		sys.exit(1)

	is_driverkit = "driverkit" in arguments.sdk.lower()

	def verbose_print(value):
		if arguments.verbose:
			print(" ".join(value))
		return value

	failed = False
	for (posix_c_source, darwin_c_level, fbounds_safety, expected_dk, expected_us) in PARAMETERS:
		clang_args = [
			"xcrun", "--sdk", arguments.sdk, "clang", "-Werror", "-D_FORTIFY_SOURCE=2", arguments.test_file]
		for inc in arguments.include_dir:
			clang_args.append("-isystem")
			clang_args.append(inc)
		if posix_c_source is not None:
			clang_args.append("-D_TEST_POSIX_C_SOURCE=%s" % posix_c_source)
		if darwin_c_level is not None:
			clang_args.append("-D_TEST_DARWIN_C_LEVEL=%s" % darwin_c_level)
		if fbounds_safety == "on":
			clang_args.append("-fbounds-safety")
			clang_args.append("-DUNDEF_BOUNDS_SAFETY_ATTRIBUTES")
		elif fbounds_safety == "on_attributes":
			clang_args.append("-fbounds-safety")
			clang_args.append("-D__LIBC_STAGED_BOUNDS_SAFETY_ATTRIBUTES")

		if arguments.check_syntax:
			syntax_check = list(clang_args)
			syntax_check.extend(("-fsyntax-only", "-Wno-nullability-completeness"))
			subprocess.check_call(verbose_print(syntax_check))

		clang_args.extend(("-E", "-o", "-"))
		p = subprocess.Popen(verbose_print(clang_args), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE)

		d = parse_substitution_dictionary(p.stdout)
		p.wait()
		if arguments.generate:
			print_substitution_dictionary(d)
			continue

		expected = expected_dk if is_driverkit else expected_us
		if d == expected:
			continue

		failed = True
		params = []
		params.append("_POSIX_C_SOURCE=%r" % posix_c_source)
		params.append("__DARWIN_C_LEVEL=%r" % darwin_c_level)
		params.append("-f%sbounds-safety" % ("" if fbounds_safety else "no"))
		if fbounds_safety:
			params.append("-%s__LIBC_STAGED_BOUNDS_SAFETY_ATTRIBUTES" % ("D" if fbounds_safety == "on_attributes" else "U"))
		print("*** FAILED: " + ", ".join(params))
		for k in sorted(d.keys()):
			if k not in expected:
				print("***  %s is %s (missing in expected dictionary)" % (k, d[k]))
			elif d[k] != expected[k]:
				print("***  %s is %s (expected: %s)" % (k, d[k], expected[k]))
		for k in sorted(expected.keys()):
			if k not in d:
				print("***  %s is missing (expected: %s)" % (k, expected[k]))

	sys.exit(1 if failed else 0)


if __name__ == "__main__":
	main()
