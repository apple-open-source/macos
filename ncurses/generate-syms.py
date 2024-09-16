#!/usr/bin/env python3

from json import dumps, loads
from os.path import basename, dirname, join, realpath
import re
from sys import stderr
from tempfile import TemporaryFile
from traceback import format_exc

known_versions = [5.4, 6.0]

# Current version that we're processing, bumped when a new ABI version is
# introduced.
current_abi_version = 6.0

# These ones follow the mv* naming convention, but they don't need any *move()
# wrapping because they actually execute the move.
move_symbols = ["mvcur", "mvderwin", "mvwin"]

nc_abi_preamble = "/* This file is @" + """generated */
/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * libncurses builds with NCURSES_WANT_BASEABI defined by default so that we
 * don't accidentally use new symbol versions in intermediate functions.  This
 * file, however, intentionally wants all of the newer prototypes.
 */
#undef NCURSES_WANT_BASEABI
#include <curses.priv.h>

unsigned int __thread _nc_abiver = NCURSES_DEFAULT_ABI;

"""

class ArglistParseException(Exception):
    """Raised when we can't parse the arglist of a function"""
    pass

class BadFilterVersionException(Exception):
    """Raised when a filter's version specification cannot be parsed"""
    pass

class BadSignatureException(Exception):
    """Raised when we can't deduce a proper signature for the function"""
    pass

class MissingMarkerException(Exception):
    """Raised when an output file is missing a marker to insert text at"""
    pass

class BadVersionException(Exception):
    """Raised when we can't determine the version of an NCURSES_EXPORT* macro."""
    pass

class UnknownVersionException(Exception):
    """Raised when we initialize the context with a version unknown to us."""
    pass

def parse_arglist(args):
    if args[0] != '(' or args[-1] != ')':
        raise ArglistParseException(args)
    args = args[1:-1]
    final_args = []
    eat_next = False
    composite_arg = ""
    # Some functions may take a function pointer as an argument; we'll split on
    # commas, but if we sense some unbalanced parentheses then we know that we
    # split just a little too far.
    for arg in args.split(","):
        if eat_next:
            composite_arg = composite_arg + "," + arg
            arg = composite_arg
        balanced = arg.count("(") == arg.count(")")
        if not balanced:
            composite_arg = arg
            eat_next = True
        else:
            final_args.append(arg.strip())
            eat_next = False
    return final_args

class SymbolFilter:
    def __init__(self, ctx, desc, version, patterns):
        self.desc = desc
        self.versions = []
        self.expressions = []
        self._parse_version_spec(ctx, version)

        assert(len(self.versions) > 0)

        if not isinstance(patterns, list):
            patterns = [patterns]

        # All patterns are anchored to the whole symbol
        for pat in patterns:
            self.expressions.append(re.compile("^" + pat + "$"))

    def applies(self, sym):
        if sym.version not in self.versions:
            return False
        for expr in self.expressions:
            if expr.match(sym.name):
                return True
        return False

    def _parse_version_spec(self, ctx, version):
        if isinstance(version, float):
            self.versions.append(version)
            return
        # Valid specs:
        # - 6.0
        # - 6.0+
        # - * (any version)
        if version == '*':
            self.versions.extend(ctx.known_versions)
            return

        spec = re.match(r"^([0-9]+.[0-9])\+?$", version)
        if spec:
            base_version = float(spec.group(1))
            if version[-1] == "+":
                for ver in known_versions:
                    if ver >= base_version:
                        self.versions.append(ver)
            else:
                if base_version not in known_versions:
                    raise BadFilterVersionException(f"Unknown version: {base_version} ({version})")
                self.versions.append(base_version)
        raise BadFilterVersionException(f"Bad version spec: {version}")

class Symbol:
    def __init__(self, ret, name, args, version, generated=False, special=False, versions = None):
        self.ret = ret
        self.name = name
        self.args = parse_arglist(args)
        self.has_retval = ret != "void"
        self.has_varargs = "..." in self.args
        self.versions = versions
        self.version = version
        self.version_major = int(version)
        self.version_minor = int((version * 10) % 10)
        self.generated = generated
        self.special = special

    def versioned_name(self):
        return f"_{self.name}_abi{self.version_major}{self.version_minor}"

    def __iter__(self):
        yield 'ret', self.ret
        yield 'args', self.args
        yield 'generated', self.generated
        yield 'special', self.special

    def __repr__(self):
        return f"Symbol<{self.ret}, {self.name}, {self.version}. {self.args}>"

class RewriteCtx:
    initial_version = known_versions[0]

    def __init__(self, scriptdir, version):
        if version not in known_versions:
            raise UnknownVersionException(version)
        self.filters = []
        self.scriptdir = scriptdir
        self.symbols = []
        self.symbol_versions = {}
        self.version = version

        # *_events symbols excluded because we don't build with that API
        # enabled (--enable-wgetch-events).
        self.excluded_syms = ["wgetch_events", "wgetnstr_events"]
        # We don't typically build --with-trace
        self.excluded_syms.extend(["_nc_viswbuf", "_nc_viswibuf"])
        # SP_FUNC in disguise
        self.excluded_syms.append("new_prescr")

        self.load_filters()
        self.load_state()

    def add_symbol(self, sym):
        self.symbols.append(sym)

    def add_symbols(self, symlist):
        self.symbols.extend(symlist)

    def exclude_symbol(self, sym):
        return sym in self.excluded_syms

    def get_symbol_versions(self, sym):
        if not isinstance(sym, str):
            sym = sym.name
        if sym not in self.symbol_versions:
            return None
        return list(self.symbol_versions[sym])

    def later_versions(self, earliest, include_earliest=True):
        earliest_idx = known_versions.index(float(earliest))
        if not include_earliest:
            earliest_idx = earliest_idx + 1
        return known_versions[earliest_idx:]

    def register_symbol(self, sym):
        if not self.symbol_filter(sym, exclude_verchk=True):
            return
        version_idx = str(sym.version)
        if sym.name not in self.symbol_versions:
            self.symbol_versions[sym.name] = {}
        elif version_idx in self.symbol_versions[sym.name]:
            return
        self.symbol_versions[sym.name][version_idx] = dict(sym)

    def symbol_filter(self, sym, *, exclude_verchk=False):
        if sym.version == 5.4 and not exclude_verchk:
            return False
        for filt in self.filters:
            if filt.applies(sym):
                return False
        return True

    def load_filters(self):
        with open(join(self.scriptdir, "generate-syms.filters")) as filterf:
            defined_filters = loads(filterf.read())
            for filt in defined_filters:
                self.filters.append(SymbolFilter(self, filt["desc"], filt["version"], filt["patterns"]))

    def load_state(self):
        try:
            with open(self.state_file()) as statef:
                saved_state = loads(statef.read())
                self.symbol_versions = saved_state['symbol_versions']
                # XXX At some point we should generate Symbols for each of the
                # legacy versions described in the state, because we can't just
                # assume they have the same signature when we re-run this
                # script.
        except:
            pass
    def state_file(self):
        return join(self.scriptdir, "generate-syms.state")

    def dump_state(self):
        state = {"latest_version": known_versions[-1]}
        state["symbol_versions"] = self.symbol_versions
        with open(self.state_file(), "w") as cfg:
            cfg.write(dumps(state, indent=True) + "\n")

def parse_version(verstr):
    # Version is specified as two digits, [major][minor] -- minor should
    # normally be 0, but we won't enforce it just in case.
    if len(verstr) != 2:
        raise BadVersionException(verstr)
    return int(verstr[0:1]) + (int(verstr[1:2]) / 10)

def get_version_range(ctx, sym_name):
    symvers = ctx.get_symbol_versions(sym_name)
    if not symvers:
        # Unversioned symbols are assumed to be of the current ABI.
        return (ctx.version, None)
    # XXX TODO: Concept of a final version for removed symbols?  That's the idea
    # with the second tuple member.
    return (symvers[0], None)

def get_flat_ver(sym_ver):
    # x.y -> xy for use in NCURSES_EXPORT_ABI macros
    return str(int(sym_ver * 10))

def get_export_macro(sym_ver):
    flat_ver = get_flat_ver(sym_ver)
    return f"NCURSES_EXPORT_ABI{flat_ver}"

def get_export_spec(sym, prefix):
    # If we have a prefix for our export macros, we should just lay out the
    # macro for the latest version; they won't have varargs versions because they
    # are generally for specialized one-version usage.
    if prefix != "":
        export_macro = get_export_macro(sym.version)
        return f"{prefix}{export_macro}({sym.name})"
    versions_desc = sym.versions[::-1]
    export_spec = ""
    paren = 0
    for ver in versions_desc:
        if ver == 5.4:
            break
        export_macro = get_export_macro(ver)
        paren = paren + 1
        export_spec = f"{export_spec}{export_macro}({sym.name}, "
    return export_spec + (")" * paren)

def strip_version(decl, strip_semicolon=False):
    if strip_semicolon:
        repl = ""
    else:
        repl = ";"
    if "NCURSES_EXPORT_ABI" not in decl:
        if strip_semicolon:
            return re.sub(";", "", decl)
        return decl
    return re.sub(" NCURSES_EXPORT_ABI[^;]+;", repl, decl)

def get_versioned_decl(ctx, first_sym, sym, decl, prefix=""):
    if prefix != "":
        # If it's getting a prefix (probably _), then we just need to write our
        # own annotating macro anew.
        decl = strip_version(decl)
    if "NCURSES_EXPORT_ABI" in decl:
        # We shouldn't see a prefixed version, those should only exist in
        # ifTAPI() declarations.  Even for symbols added after 5.4, we still
        # have a "base" version and then a suffixed version -- the suffixed
        # version does the appropriate version push/pop.
        assert("_NCURSES_EXPORT_ABI" not in decl)
        assert(prefix == "")
        # Just replace the ABI[major][minor] part with the version for this
        # symbol.
        return re.sub("ABI[0-9]+", "ABI" + get_flat_ver(sym.version), decl)
    else:
        export_spec = get_export_spec(sym, prefix)
        return re.sub(";", " " + export_spec + ";", decl)

def trim_comments(decl_line):
    return re.sub("/\*[^*]+\*/", "", decl_line).rstrip()

def finish_decl(ctx, file, declt, sym_ret, sym_name, sym_arglist, sym_verspec, sym_generated, sym_special):
    # declt should be a 2-tuple, line and continued line.
    # sym_verspec will also be a 2-tuple, first version and last version it
    # appeared in.

    # For now, we just leave special symbols alone.  sym_special is still
    # plumbed in down below so that the information is there if we decide to
    # handle them differently later.
    if sym_special:
        return

    # If it first appeared in an earlier version, we need to push a version
    # for each of the intermediate versions.
    versions = ctx.later_versions(sym_verspec[0])
    if sym_verspec[1] is not None:
        # sym_verspec[1] is the final version it appeared in, so we clip the
        # version array there.  It may be the case that the last version is also
        # the current version, which is OK -- we'll still do the right thing.
        last_idx = versions.index(sym_verspec[1])
        versions = versions[:last_idx + 1]
    adding = [Symbol(sym_ret, sym_name, sym_arglist, sym_ver, sym_generated, sym_special, versions) for sym_ver in versions]

    for sym in adding:
        ctx.register_symbol(sym)

    # Record the first symbol pre-filtering, because it may get filtered out if
    # the symbol first appeared in 5.4.  For 5.4 symbols, we don't need to
    # define an implementation.
    first_sym = adding[0]
    adding = [sym for sym in adding if ctx.symbol_filter(sym)]

    # We may have filtered out all versions of the symbol, in which case we have
    # nothing further to do.
    if len(adding) == 0:
        return

    # Reverse the symbols so that the default is first, we'll declare the rest
    # arbitrarily in descending order -- this is just a bit more aesthetically
    # pleasing than, e.g., defining the default as 8.0, then adding ifTAPI
    # declarations in order for 8.0, 6.0, 6.4.
    adding.reverse()
    ctx.add_symbols(adding)
    default_sym = adding[0]

    if declt[1] is None:
        # We may need a continuation if this is a generated symbol.  The sed/awk
        # script that generates the implementation for generated symbols can't
        # necessarily handle other annotations on the same line.
        if default_sym.generated:
            file.write(trim_comments(strip_version(declt[0], True)) + "\n")
            continued_line = "\t\t;\n"
            file.write(get_versioned_decl(ctx, first_sym, default_sym, continued_line))
        else:
            file.write(trim_comments(get_versioned_decl(ctx, first_sym, default_sym, declt[0])) + "\n")
    else:
        # If we have a continuation, the version goes on the second line.
        assert(";" not in declt[0])
        file.write(trim_comments(declt[0]) + "\n")
        file.write(get_versioned_decl(ctx, first_sym, default_sym, declt[1]))

    # Now write ifTAPI declarations for all of the others
    for sym in adding:
        file.write("ifTAPI(")
        if declt[1] is None:
            file.write(trim_comments(get_versioned_decl(ctx, first_sym, sym, declt[0], "_").replace(";", "")))
        else:
            # If we have a continuation, the version goes on the second line.
            assert(";" not in declt[0])
            file.write(trim_comments(declt[0]) + "\n")
            file.write(trim_comments(get_versioned_decl(ctx, first_sym, sym, declt[1], "_").replace(";", "")))
        file.write(");\n")

def process_header(ctx, versioned_file, hdr):
    # TAPI expressions will be excluded as we write our own versions.  This is
    # because TAPI needs to see *all* versions of the symbol, so we let it see
    # the normal NCURSES_EXPORT() declaration and add in an ifTAPI() expression
    # for each of the $suffixed versions that we're adding.
    tapi_expr = re.compile(r"^ifTAPI")
    func_expr = re.compile(r"^extern NCURSES_EXPORT\(")

    spfunc_expr = re.compile(r"^.+NCURSES_SP_NAME")

    funcsig_expr = re.compile("^extern NCURSES_EXPORT\(([^)]+)\) ([^ ]+) *(\([^)]*\).*;?)")

    try:
        with open(hdr) as f:
            continued = False
            eat_next = False
            symbols_seen = set()
            for line in f:
                if continued:
                    continued = False
                    if eat_next:
                        # ifTAPI line to eat
                        eat_next = False
                        continue
                    sym_verspec = get_version_range(ctx, sym_name)
                    finish_decl(ctx, versioned_file, (decl_line, line), sym_ret, sym_name, sym_arglist, sym_verspec, sym_generated, sym_special)
                elif tapi_expr.match(line):
                    # ifTAPI line to eat
                    eat_next = ";" not in line
                    continued = eat_next
                    pass
                elif func_expr.match(line) and not spfunc_expr.match(line):
                    # Function we need to implement
                    sig = funcsig_expr.match(line)
                    if not sig:
                       raise BadSignatureException(line)
                    sym_ret = sig.group(1)
                    sym_name = sig.group(2)
                    sym_arglist = sig.group(3)
                    if "NCURSES_EXPORT_ABI" in sym_arglist:
                        sym_arglist = re.sub("_?NCURSES_EXPORT_ABI.+$", "", sym_arglist).rstrip()
                    else:
                        sym_arglist = re.sub("\)[^)]+$", ")", sym_arglist).rstrip()
                    sym_generated = "* generated" in line
                    # We want to avoid generating definitions and whatnot for
                    # symbols we've already seen -- hopefully, the correct
                    # annotations were all on the first declaration found.
                    sym_special = "* special" in line or sym_name in symbols_seen or ctx.exclude_symbol(sym_name)
                    if sym_name not in symbols_seen:
                        symbols_seen.add(sym_name)
                    if ";" in line:
                        sym_verspec = get_version_range(ctx, sym_name)
                        finish_decl(ctx, versioned_file, (line, None), sym_ret, sym_name, sym_arglist, sym_verspec, sym_generated, sym_special)
                    else:
                        decl_line = line
                        continued = True
        return True
    except Exception as exc:
        print(f"Exception taken while processing {hdr}", file=stderr)
        print(format_exc(), file=stderr)
        return False

def process_symbol_args(sym):
    # We'll use the first window arg in some mv* functions; special case
    # Name assignment happens here; we'll just name them "arg" + # arg to make
    # it easier and avoid imposing arbitrary limits on number of args here.
    arg_names = []
    arg_decls = []

    # We track the first WINDOW argument both because we'll need to pass it on
    # if we're implemented with a vw* function, and because we may need to grab
    # the position arguments that typically come immediately after the WINDOW.
    winarg = None
    winidx = None

    for arg in sym.args:
        if arg != "void" and arg != "...":
            name = "arg" + str(len(arg_decls))
            if arg[0:6] == "WINDOW" and winarg is None:
                winarg = name
                winidx = len(arg_decls)
            arg_names.append(name)
            if "(" in arg:
                # Function pointer, insert name after * in the initial
                # set of parenthesis.
                arg = re.sub("\(\s*\*", f"(*{name}", arg, count=1)
            else:
                arg = f"{arg} {name}"
        arg_decls.append(arg)
    return arg_names, arg_decls, winarg, winidx

def write_one_symbol(abi_file, symbols_seen, symbols_generated, sym):
    arg_names, arg_decls, first_window_arg, first_window_idx = process_symbol_args(sym)

    versioned_name = sym.versioned_name()
    # Unconditionally exported for this one.
    versioned_macro = "_" + get_export_macro(sym.version)

    # Write out a declaration for this versioned name first; this lets us get
    # away with not relying on the headers to get the correct version.
    abi_file.write(f"\nNCURSES_EXPORT({sym.ret}) {versioned_name} (")
    abi_file.write(", ".join(arg_decls))
    abi_file.write(f") {versioned_macro}({sym.name});\n")

    abi_file.write(f"\nNCURSES_EXPORT({sym.ret}) {versioned_name} (")
    abi_file.write(", ".join(arg_decls))
    abi_file.write(")\n{\n")

    # Don't need a local to hold the return value if there is no return
    # value.  We otherwise need a local because we push/pop the ABI
    # version around a call to the underlying implementation.
    if sym.has_varargs:
        abi_file.write("\tva_list argp;\n")
    if sym.has_retval:
        abi_file.write(f"\t{sym.ret} ret;\n\n")
    func_name = sym.name
    func_generated = sym.generated
    abi_file.write(f"\tNCURSES_ABI_PUSH({sym.version_major}, {sym.version_minor});\n")
    if func_name[0:2] == "mv" and func_name not in move_symbols:
        # If we don't have a retval, we need to dig a little deeper because
        # other changes below are likely required.
        assert(sym.has_retval)
        window_arg = first_window_arg
        if func_name[2] != "w":
            # Non-windowed version, y and x are first and we operate on
            # stdscr
            window_arg = "stdscr"
            y_arg = 0
            x_arg = 1
        else:
            # This holds true for now, so we assume there's nothing
            # useful earlier than the first window idx.
            assert(first_window_idx == 0)
            y_arg = first_window_idx + 1
            x_arg = y_arg + 1
        abi_file.write(f"\tif ((ret = wmove({window_arg}, arg{y_arg}, arg{x_arg})) == ERR) {{\n")
        abi_file.write("\t\tNCURSES_ABI_POP();\n")
        abi_file.write("\t\treturn (ret);\n")
        abi_file.write("\t}\n")

        # Now we need to rewrite the terms of what we're invoking as
        # the implementation; remove the mv[w]? prefix, optionally add
        # _impl if that symbol is not generated
        arg_chop_idx = x_arg + 1
        chop_idx = 2
        if func_name[chop_idx] == "w":
            chop_idx = chop_idx + 1
        func_name = func_name[chop_idx:]
        func_generated = (func_name in symbols_generated)

        arg_names = arg_names[arg_chop_idx:]

    if sym.has_varargs:
        if func_name[0] != "w":
            func_name = "vw" + func_name
            scr = "stdscr"
            if first_window_arg is not None:
                scr = first_window_arg
            arg_names = [scr] + arg_names
        else:
            func_name = "v" + func_name
        last_arg = arg_names[-1]
        arg_names.append("argp")
        abi_file.write(f"\tva_start(argp, {last_arg});\n")

    abi_file.write("\t")
    if sym.has_retval:
        abi_file.write("ret = ")
    suffix = "_impl" if func_name in symbols_seen else ""
    abi_file.write(f"{func_name}{suffix}(" + ", ".join(arg_names) + ");\n")
    if sym.has_varargs:
        abi_file.write("\tva_end(argp);\n")
    abi_file.write("\tNCURSES_ABI_POP();\n")
    if sym.has_retval:
        abi_file.write("\n\treturn (ret);\n")

    abi_file.write("}\n")

def write_abi_impl(ctx, nc_abi_filename):
    with TemporaryFile(mode="w+", encoding="utf-8") as abi_file:
        abi_file.write(nc_abi_preamble)

        # Do a first pass and write out declarations for the base versions of
        # these symbols.  Also note which symbols are generated so that we don't
        # try and use _impl versions of those when the defined macros will
        # suffice.
        symbols_seen = set()
        symbols_generated = set()
        symbols_need_decl = []

        abi_file.write("/* Generated symbols with macros */\n")
        for sym in ctx.symbols:
            if sym.name in symbols_seen:
                continue
            symbols_seen.add(sym.name)
            if sym.generated:
                abi_file.write(f"#undef {sym.name}\n")
                symbols_generated.add(sym.name)

            # Should always have an arg in sym.args, even if that one arg is
            # simply 'void'.
            assert(len(sym.args) > 0)
            if not sym.has_varargs:
                symbols_need_decl.append(sym)

        abi_file.write("\n/* Base symbol declarations */\n")
        for sym in symbols_need_decl:
            abi_file.write(f"extern NCURSES_EXPORT({sym.ret}) ({sym.name}_impl) (")
            abi_file.write(", ".join(sym.args))
            abi_file.write(f") _NCURSES_EXPORT_ABI({sym.name});\n")

        # Now do a second pass and write out the implementations for the
        # versioned symbols
        for sym in ctx.symbols:
            write_one_symbol(abi_file, symbols_seen, symbols_generated, sym)

        abi_file.seek(0)
        with open(nc_abi_filename, "w") as nc_abi_file:
            nc_abi_file.write(abi_file.read())

def write_versioned_header(versioned_file, versioned_hdr):
        # Write versioned_file out as a block in curses.head, which will
        # eventually get rolled into <curses.h>.  We specifically want to avoid
        # curses.h.in as we'd like to be able to not have to account for these
        # declarations in the above logic.  We use an @@ABIDECLS@@ marker
        # similar to the various @MARKERS@ that ncurses already uses itself in
        # curses.h.in; this is just another processing step in the configuration
        # process.
        versioned_file.seek(0)
        abidecl_expr = re.compile(r"^@@ABIDECLS@@$", re.M)
        with open(versioned_hdr, "r+") as hdr_file:
            contents = hdr_file.read()
            pos = abidecl_expr.search(contents)
            if not pos:
                raise MissingMarkerException(f"{versioned_hdr} missing @@ABIDECLS@@ marker")
            # Removing the newline before and the newline after, just to keep
            # this block somewhat tidy.
            marker_start = pos.start() - 1
            marker_end = pos.end() + 1

            contents = contents[0:marker_start] + versioned_file.read() + contents[marker_end:]
            hdr_file.seek(0)
            hdr_file.truncate(0)
            hdr_file.write(contents)

def main(scriptdir):
    headers = [join(scriptdir, hdr) for hdr in ["ncurses/include/curses.h.in", "ncurses/include/curses.wide"]]
    versioned_hdr = join(scriptdir, "ncurses/include/curses.head")
    ctx = RewriteCtx(scriptdir=scriptdir, version=current_abi_version)
    with TemporaryFile(mode="w+", encoding="utf-8") as versioned_file:
        for hdr in headers:
            hdr_name = basename(hdr)
            wide_defs = re.match("curses.wide", hdr_name)
            if wide_defs:
                versioned_file.write("\n#if NCURSES_WIDECHAR")
            versioned_file.write(f"\n/* Declarations from {hdr_name} */\n")
            if not process_header(ctx, versioned_file, hdr):
                print(f"Failed to generate versioned symbols from '{hdr}'", file=stderr)
                return 1
            if wide_defs:
                versioned_file.write("#endif /* NCURSE_WIDECHAR */\n")

        try:
            write_versioned_header(versioned_file, versioned_hdr)
        except MissingMarkerException as exc:
            print("Error:", file=stderr)
            print(f"- {exc}", file=stderr)
            print("configure.sh run needed", file=stderr)
            return 1
    write_abi_impl(ctx, join(scriptdir, "ncurses/ncurses/base/nc_abi.c"))
    ctx.dump_state()
    return 0

if __name__ == "__main__":
    exit(main(dirname(realpath(__file__))))
