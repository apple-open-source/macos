#! /usr/bin/env python2.2

# Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

"""distcc test suite, using comfychair

This script is called with $PATH pointing to the appropriate location
for the built (or installed) programs to be tested.

Copyright (C) 2002, 2003 by Martin Pool
"""


# There are pretty strong hierarchies of test cases: ones to do with
# running a daemon, compiling a file and so on.  This nicely maps onto
# a hierarchy of object classes.

# It seems to work best if an instance of the class corresponds to an
# invocation of a test: this means each method runs just once and so
# object state is not very useful, but nevermind.

# Having a complicated pattens of up and down-calls within the class
# methods seems to make things more complicated.  It may be better if
# abstract superclasses just provide methods that can be called,
# rather than establishing default behaviour.

# TODO: Run the server in a different directory from the clients

# TODO: Compile locally to .s, and assemble remotely; check that
# works.

# TODO: Some kind of direct test of the host selection algorithm.

# TODO: Optionally run all discc tests under Valgrind, ElectricFence
# or something similar.

# TODO: Test that ccache correctly caches compilations through distcc:
# make up a random file so it won't hit, then compile once and compile
# twice and examine the log file to make sure we got a hit.  Also
# check that the binary works properly.

# TODO: Test cpp from stdin

# TODO: Do all this with malloc debugging on.

# TODO: Redirect daemon output to a file so that we can more easily
# check it.  Is there a straightforward way to test that it's also OK
# when send through syslogd?

# TODO: Check behaviour when children are killed off.

# TODO: Test compiling over IPv6

# TODO: Argument scanning tests should be run with various hostspecs,
# because that makes a big difference to how the client handles them.

# TODO: Test --no-fork mode

# TODO: Test --inetd mode by creating a socketpair ourselves

# TODO: Test that ccache gets hits when calling distcc.  Presumably
# this is skipped if we can't find ccache.  Need to parse `ccache -s`.

# TODO: Set TMPDIR to be inside the working directory, and perhaps
# also set DISTCC_SAVE_TEMPS.  Might help for debugging.

# Check that without DISTCC_SAVE_TEMPS temporary files are cleaned up.

# TODO: Test compiling a really large source file that produces a
# large object file.  Perhaps need to generate it at run time -- just
# one big array?

# TODO: Perhaps redirect stdout, stderr to a temporary file while
# running?  Use os.open(), os.dup2().

# TODO: Test "distcc gcc -c foo.c bar.c".  gcc would actually compile
# both of them.  We could split it into multiple compiler invocations,
# but this is so rare that it's probably not worth the complexity.  So
# at the moment is just handled locally.

# TODO: Test crazy option arguments like "distcc -o -output -c foo.c"

# TODO: Test attempt to compile a nonexistent file.

# TODO: Add test harnesses that just exercise the bulk file transfer
# routines.

# TODO: Test -MD, -MMD, -M, etc.

# TODO: Test using '.include' in an assembly file, and make sure that
# it is resolved on the client, not on the server.

# TODO: Run "sleep" as a compiler, then kill the client and make sure
# that the server and "sleep" promptly terminate.

# TODO: Set umask 0, then check that the files are created with mode
# 0644.

# TODO: Perhaps have a little compiler that crashes.  Check that the
# signal gets properly reported back.

# TODO: Have a little compiler that takes a very long time to run.
# Try interrupting the connection and see if the compiler is cleaned
# up in a reasonable time.

# TODO: Try to build a nonexistent source file.  Check that we only
# get one error message -- if there were two, we would incorrectly
# have tried to build the program both remotely and locally.

# TODO: Test compiling a 0-byte source file.  This should be allowed.

# TODO: Test a compiler that sleeps for a long time; try killing the
# server and make sure it goes away.

# TODO: Set LANG=C before running all tests, to try to make sure that
# localizations don't break attempts to parse error messages.  Is
# setting LANG enough, or do we also need LC_*?  (Thanks to Oscar
# Esteban.)

# TODO: Test scheduler.  Perhaps run really slow jobs to make things
# deterministic, and test that they're dispatched in a reasonable way.

# TODO: Test generating dependencies with -MD.

# TODO: Test a nasty cpp that always writes to stdout regardless of
# -o.

# TODO: Test giving up privilege using --user.  Difficult -- we may
# need root privileges to run meaningful tests.

# TODO: Test that recursion safeguard works.

# TODO: Test using h_parsemask

# TODO: Check using daemon --allow option for both allowed and denied
# cases.  Check with multiple addresses.  Perhaps count only on
# 127.0.0.1 - this ought to be enough.


import time, sys, string, os, types, re, popen2, pprint
import signal, os.path, string
import comfychair

from stat import *                      # this is safe

EXIT_DISTCC_FAILED           = 100
EXIT_BAD_ARGUMENTS           = 101
EXIT_BIND_FAILED             = 102
EXIT_CONNECT_FAILED          = 103
EXIT_COMPILER_CRASHED        = 104
EXIT_OUT_OF_MEMORY           = 105
EXIT_BAD_HOSTSPEC            = 106
EXIT_COMPILER_MISSING        = 110


class SimpleDistCC_Case(comfychair.TestCase):
    '''Abstract base class for distcc tests'''
    def setup(self):
        self.stripEnvironment()

    def stripEnvironment(self):
        """Remove all DISTCC variables from the environment, so that
        the test is not affected by the development environment."""
        for key in os.environ.keys():
            if key[:7] == 'DISTCC_':
                # NOTE: This only works properly on Python 2.2: on
                # earlier versions, it does not call unsetenv() and so
                # subprocesses may get confused.
                del os.environ[key]
        os.environ['TMPDIR'] = self.tmpdir


class WithDaemon_Case(SimpleDistCC_Case):
    """Start the daemon, and then run a command locally against it.

The daemon doesn't detach until it has bound the network interface, so
as soon as that happens we can go ahead and start the client."""

    def setup(self):
        import random
        SimpleDistCC_Case.setup(self)
        self.daemon_pidfile = os.path.join(os.getcwd(), "daemonpid.tmp")
        self.daemon_logfile = os.path.join(os.getcwd(), "distccd.log")
        self.server_port = random.randint(42000, 43000)
        self.startDaemon()
        self.setupEnv()

    def setupEnv(self):
        os.environ['DISTCC_HOSTS'] = '127.0.0.1:%d' % self.server_port
        os.environ['DISTCC_LOG'] = os.path.join(os.getcwd(), 'distcc.log')
        os.environ['DISTCC_VERBOSE'] = '1'


    def teardown(self):
        SimpleDistCC_Case.teardown(self)


    def killDaemon(self):
        import signal, time
        try:
            pid = int(open(self.daemon_pidfile, 'rt').read())
        except IOError:
            # the daemon probably already exited, perhaps because of a timeout
            return
        os.kill(pid, signal.SIGTERM)

        # We can't wait on it, because it detached.  So just keep
        # pinging until it goes away.
        while 1:
            try:
                os.kill(pid, 0)
            except OSError:
                break
            time.sleep(0.2)


    def daemon_command(self):
        """Return command to start the daemon"""
        return ("distccd --verbose --lifetime=%d --daemon --log-file %s "
                "--pid-file %s --port %d --allow 127.0.0.1"
                % (self.daemon_lifetime(),
                   self.daemon_logfile, self.daemon_pidfile, self.server_port))

    def daemon_lifetime(self):
        # Enough for most tests, even on a fairly loaded machine.
        # Might need more for long-running tests.
        return 10

    def startDaemon(self):
        """Start a daemon in the background, return its pid"""
        # The daemon detaches once it has successfully bound the
        # socket, so if something goes wrong at startup we ought to
        # find out straight away.  If it starts successfully, then we
        # can go ahead and try to connect.
        
        cmd = self.daemon_command()
        self.runcmd(cmd)
        self.add_cleanup(self.killDaemon)




class StartStopDaemon_Case(WithDaemon_Case):
    def runtest(self):
        pass


class VersionOption_Case(SimpleDistCC_Case):
    """Test that --version returns some kind of version string.

    This is also a good test that the programs were built properly and are
    executable."""
    def runtest(self):
        import string
        for prog in 'distcc', 'distccd':
            out, err = self.runcmd("%s --version" % prog)
            assert out[-1] == '\n'
            out = out[:-1]
            line1,trash = string.split(out, '\n')
            self.assert_re_match(r'^%s [\w.-]+ [.\w-]+ \(protocol 1\) \(default port 3632\)$'
                                 % prog, line1)


class HelpOption_Case(SimpleDistCC_Case):
    """Test --help is reasonable."""
    def runtest(self):
        for prog in 'distcc', 'distccd':
            out, err = self.runcmd(prog + " --help")
            self.assert_re_search("Usage:", out)


class BogusOption_Case(SimpleDistCC_Case):
    """Test handling of --bogus-option.

    Now that we support implicit compilers, this is passed to gcc, which returns 1."""
    def runtest(self):
        self.runcmd("distcc --bogus-option", 1)
        self.runcmd("distccd --bogus-option", EXIT_BAD_ARGUMENTS)


class GccOptionsPassed_Case(SimpleDistCC_Case):
    """Test that options following the compiler name are passed to the compiler."""
    def runtest(self):
        out, err = self.runcmd("DISTCC_HOSTS=localhost distcc gcc --help")
        if re.search('distcc', out):
            raise ("gcc help contains \"distcc\": \"%s\"" % out)
        self.assert_re_match(r"^Usage: gcc", out)


class StripArgs_Case(SimpleDistCC_Case):
    """Test -D and -I arguments are removed"""
    def runtest(self):
        cases = (("gcc -c hello.c", "gcc -c hello.c"),
                 ("cc -Dhello hello.c -c", "cc hello.c -c"),
                 ("gcc -g -O2 -W -Wall -Wshadow -Wpointer-arith -Wcast-align -c -o h_strip.o h_strip.c",
                  "gcc -g -O2 -W -Wall -Wshadow -Wpointer-arith -Wcast-align -c -o h_strip.o h_strip.c"),
                 # invalid but should work
                 ("cc -c hello.c -D", "cc -c hello.c"),
                 ("cc -c hello.c -D -D", "cc -c hello.c"),
                 ("cc -c hello.c -I ../include", "cc -c hello.c"),
                 ("cc -c -I ../include  hello.c", "cc -c hello.c"),
                 ("cc -c -I. -I.. -I../include -I/home/mbp/garnome/include -c -o foo.o foo.c",
                  "cc -c -c -o foo.o foo.c"),
                 ("cc -c -DDEBUG -DFOO=23 -D BAR -c -o foo.o foo.c",
                  "cc -c -c -o foo.o foo.c"),

                 # New options stripped in 0.11
                 ("cc -o nsinstall.o -c -DOSTYPE=\"Linux2.4\" -DOSARCH=\"Linux\" -DOJI -D_BSD_SOURCE -I../dist/include -I../dist/include -I/home/mbp/work/mozilla/mozilla-1.1/dist/include/nspr -I/usr/X11R6/include -fPIC -I/usr/X11R6/include -Wall -W -Wno-unused -Wpointer-arith -Wcast-align -pedantic -Wno-long-long -pthread -pipe -DDEBUG -D_DEBUG -DDEBUG_mbp -DTRACING -g -I/usr/X11R6/include -include ../config-defs.h -DMOZILLA_CLIENT -Wp,-MD,.deps/nsinstall.pp nsinstall.c",
                  "cc -o nsinstall.o -c -fPIC -Wall -W -Wno-unused -Wpointer-arith -Wcast-align -pedantic -Wno-long-long -pthread -pipe -g nsinstall.c"),
                 )
        for cmd, expect in cases:
            o, err = self.runcmd("h_strip %s" % cmd)
            if o[-1] == '\n': o = o[:-1]
            self.assert_equal(o, expect)


class IsSource_Case(SimpleDistCC_Case):
    def runtest(self):
        """Test distcc's method for working out whether a file is source"""
        cases = (( "hello.c",          "source",       "not-preprocessed" ),
                 ( "hello.cpp",        "source",       "not-preprocessed" ),
                 ( "hello.2.4.4.i",    "source",       "preprocessed" ),
                 ( ".foo",             "not-source",   "not-preprocessed" ),
                 ( "gcc",              "not-source",   "not-preprocessed" ),
                 ( "hello.ii",         "source",       "preprocessed" ),
                 ( "hello.c++",        "source",       "not-preprocessed" ),
                 ( "boot.s",           "not-source",   "not-preprocessed" ),
                 ( "boot.S",           "not-source",   "not-preprocessed" ))
        for f, issrc, iscpp in cases:
            o, err = self.runcmd("h_issource '%s'" % f)
            expected = ("%s %s\n" % (issrc, iscpp))
            if o != expected:
                raise AssertionError("issource %s gave %s, expected %s" %
                                     (f, `o`, `expected`))



class ScanArgs_Case(SimpleDistCC_Case):
    '''Test understanding of gcc command lines'''
    def runtest(self):
        cases = [("gcc -c hello.c", "distribute", "hello.c", "hello.o"),
                 ("gcc hello.c", "local"),
                 ("gcc -o /tmp/hello.o -c ../src/hello.c", "distribute", "../src/hello.c", "/tmp/hello.o"),
                 ("gcc -DMYNAME=quasibar.c bar.c -c -o bar.o", "distribute", "bar.c", "bar.o"),
                 ("gcc -ohello.o -c hello.c", "distribute", "hello.c", "hello.o"),
                 ("ccache gcc -c hello.c", "distribute", "hello.c", "hello.o"),
                 ("gcc hello.o", "local"),
                 ("gcc -o hello.o hello.c", "local"),
                 ("gcc -o hello.o -c hello.s", "local"),
                 ("gcc -o hello.o -c hello.S", "local"),
                 ("gcc -fprofile-arcs -ftest-coverage -c hello.c", "local", "hello.c", "hello.o"),
                 ("gcc -S hello.c", "distribute", "hello.c", "hello.s"),
                 ("gcc -c -S hello.c", "distribute", "hello.c", "hello.s"),
                 ("gcc -S -c hello.c", "distribute", "hello.c", "hello.s"),
                 ("gcc -M hello.c", "local"),
                 ("gcc -ME hello.c", "local"),
                 
                 ("gcc -MD -c hello.c", "distribute", "hello.c", "hello.o"),
                 ("gcc -MMD -c hello.c", "distribute", "hello.c", "hello.o"),

                 # Assemble to stdout (thanks Alexandre).  
                 ("gcc -S foo.c -o -", "local"),
                 ("-S -o - foo.c", "local"),
                 ("-c -S -o - foo.c", "local"),
                 ("-S -c -o - foo.c", "local"),
                 ]
        for tup in cases:
            apply(self.checkScanArgs, tup)

    def checkScanArgs(self, ccmd, mode, input=None, output=None):
        o, err = self.runcmd("h_scanargs %s" % ccmd)
        o = o[:-1]                      # trim \n
        os = string.split(o)
        if mode != os[0]:
            self.fail("h_scanargs %s gave %s mode, expected %s" %
                      (ccmd, os[0], mode))
        if mode == 'distribute':
            if os[1] <> input:
                self.fail("h_scanargs %s gave %s input, expected %s" %
                          (ccmd, os[1], input))
            if os[2] <> output:
                self.fail("h_scanargs %s gave %s output, expected %s" %
                          (ccmd, os[2], output))



class ImplicitCompilerScan_Case(ScanArgs_Case):
    '''Test understanding of commands with no compiler'''
    def runtest(self):
        cases = [("-c hello.c",            "distribute", "hello.c", "hello.o"),
                 ("hello.c -c",            "distribute", "hello.c", "hello.o"),
                 ("-o hello.o -c hello.c", "distribute", "hello.c", "hello.o"),
                 ]
        for tup in cases:
            # NB use "apply" rather than new syntax for compatibility with
            # venerable Pythons.
            apply(self.checkScanArgs, tup)
            

class ExtractExtension_Case(SimpleDistCC_Case):
    def runtest(self):
        """Test extracting extensions from filenames"""
        for f, e in (("hello.c", ".c"),
                     ("hello.cpp", ".cpp"),
                     ("hello.2.4.4.4.c", ".c"),
                     (".foo", ".foo"),
                     ("gcc", "(NULL)")):
            out, err = self.runcmd("h_exten '%s'" % f)
            assert out == e


class DaemonBadPort_Case(SimpleDistCC_Case):
    def runtest(self):
        """Test daemon invoked with invalid port number"""
        self.runcmd("distccd --log-file=distccd.log --lifetime=10 --port 80000",
                    EXIT_BIND_FAILED)
        self.assert_no_file("daemonpid.tmp")


class InvalidHostSpec_Case(SimpleDistCC_Case):
    def runtest(self):
        """Test various invalid DISTCC_HOSTS
        
        See also test_parse_host_spec, which tests valid specifications."""
        for spec in ["", "    ", "\t", "  @ ", ":", "mbp@", "angry::", ":4200"]:
            self.runcmd("DISTCC_HOSTS=\"%s\" h_hosts -v" % spec,
                         EXIT_BAD_HOSTSPEC)


class ParseHostSpec_Case(SimpleDistCC_Case):
    def runtest(self):
        """Check operation of dcc_parse_hosts_env.

        Passes complex environment variables to h_hosts, which is a C wrapper
        that calls the appropriate tests."""
        spec="""localhost 127.0.0.1 @angry   ted@angry
        \t@angry:/home/mbp/bin/distccd  angry:4204
        ipv4-localhost
        """
        expected="""7
   1 LOCAL
   4 TCP 127.0.0.1 3632
   4 SSH (no-user) angry (no-command)
   4 SSH ted angry (no-command)
   4 SSH (no-user) angry /home/mbp/bin/distccd
   4 TCP angry 4204
   4 TCP ipv4-localhost 3632
"""
        out, err = self.runcmd("DISTCC_HOSTS=\"%s\" h_hosts" % spec)
        assert out == expected, "expected %s\ngot %s" % (`expected`, `out`)


class Compilation_Case(WithDaemon_Case):
    '''Test distcc by actually compiling a file'''
    def setup(self):
        WithDaemon_Case.setup(self)
        self.createSource()

    def runtest(self):
        self.compile()
        self.link()
        self.checkBuiltProgram()

    def createSource(self):
        filename = self.sourceFilename()
        f = open(filename, 'w')
        f.write(self.source())
        f.close()

    def sourceFilename(self):
        return "testtmp.c"              # default
    
    def compile(self):
        cmd = self.compileCmd()
        out, err = self.runcmd(cmd)
        if out != '':
            self.fail("compiler command %s produced output:\n%s" % (`cmd`, out))
        if err != '':
            self.fail("compiler command %s produced error:\n%s" % (`cmd`, err))

    def link(self):
        cmd = self.linkCmd()
        out, err = self.runcmd(cmd)
        if out != '':
            self.fail("command %s produced output:\n%s" % (`cmd`, `out`))
        if err != '':
            self.fail("command %s produced error:\n%s" % (`cmd`, `err`))

    def compileCmd(self):
        """Return command to compile source and run tests"""
        return "distcc cc -o testtmp.o -c %s" % (self.sourceFilename())

    def linkCmd(self):
        return "distcc cc -o testtmp testtmp.o"

    def checkCompileMsgs(self, msgs):
        if msgs <> '':
            self.fail("expected no compiler messages, got \"%s\""
                      % msgs)

    def checkBuiltProgram(self):
        '''Check compile results.  By default, just try to execute.'''
        msgs, errs = self.runcmd("./testtmp")
        self.checkBuiltProgramMsgs(msgs)
        self.assert_equal(errs, '')

    def checkBuiltProgramMsgs(self, msgs):
        pass


class CompileHello_Case(Compilation_Case):
    """Test the simple case of building a program that works properly"""
    def source(self):
        return """
#include <stdio.h>

int main(void) {
    puts("hello world");
    return 0;
}
"""

    def checkBuiltProgramMsgs(self, msgs):
        self.assert_equal(msgs, "hello world\n")




class MultipleCompile_Case(Compilation_Case):
    """Test compiling several files from one line"""
    def setup(self):
        WithDaemon_Case.setup(self)
        open("test1.c", "w").write("const char *msg = \"hello foreigner\";")
        open("test2.c", "w").write("""#include <stdio.h>

int main(void) {
   extern const char *msg;
   puts(msg);
   return 0;
}
""")
        
    def runtest(self):
        self.runcmd("distcc cc -c test1.c test2.c")
        self.runcmd("distcc cc -o test test1.o test2.o")
        


class CppError_Case(CompileHello_Case):
    """Test failure of cpp"""
    def source(self):
        return '#error "not tonight dear"\n'

    def runtest(self):
        cmd = "distcc -c testtmp.c"
        msgs, errs = self.runcmd(cmd, expectedResult=1)
        self.assert_re_search("not tonight dear", errs)
        self.assert_equal(msgs, '')
    

class BadInclude_Case(Compilation_Case):
    """Handling of error running cpp"""
    def source(self):
        return """#include <nosuchfilehere.h>
"""

    def runtest(self):
        self.runcmd("distcc cc -o testtmp.o -c testtmp.c", 1)


class PreprocessPlainText_Case(Compilation_Case):
    """Try using cpp on something that's not C at all"""
    def setup(self):
        self.stripEnvironment()
        self.createSource()

    def source(self):
        return """#define FOO 3
#if FOO < 10
small foo!
#else
large foo!
#endif
/* comment ca? */
"""

    def runtest(self):
        # -P means not to emit linemarkers
        self.runcmd("distcc gcc -E testtmp.c -o testtmp.out")
        out = open("testtmp.out").read()
        # It's a bit hard to know the exact value, because different versions of
        # GNU cpp seem to handle the whitespace differently.
        self.assert_re_search("small foo!", out)

    def teardown(self):
        # no daemon is run for this test
        pass
        

class NoDetachDaemon_Case(CompileHello_Case):
    """Check that without --no-detach the server goes into the background and can be stopped."""
    def startDaemon(self):
        cmd = ("distccd --no-detach --verbose --log-file %s --pid-file %s --port %d --allow 127.0.0.1" %
               (self.daemon_logfile, self.daemon_pidfile, self.server_port))
        self.pid = self.runcmd_background(cmd)
        self.add_cleanup(self.killDaemon)
        time.sleep(.5)             # give it a bit of time to bind port

    def killDaemon(self):
        import signal
        os.kill(self.pid, signal.SIGTERM)
        pid, ret = os.wait()
        self.assert_equal(self.pid, pid)
        self.assert_equal(ret, 0)

        

class ImplicitCompiler_Case(CompileHello_Case):
    """Test giving no compiler works"""
    def compileCmd(self):
        return "distcc -c testtmp.c"

    def linkCmd(self):
        # FIXME: Mozilla uses something like "distcc testtmp.o -o testtmp",
        # but that's broken at the moment.
        return "distcc -o testtmp testtmp.o "
        

class DashD_Case(Compilation_Case):
    """Test preprocessor arguments"""
    def source(self):
        return """
#include <stdio.h>

int main(void) {
    printf("%s\\n", MESSAGE);
    return 0;
}
"""

    def compileCmd(self):
        # quoting is hairy because this goes through the shell
        return "distcc cc -c -o testtmp.o '-DMESSAGE=\"hello world\"' testtmp.c"

    def checkBuiltProgramMsgs(self, msgs):
        self.assert_equal(msgs, "hello world\n")


class ThousandFold_Case(CompileHello_Case):
    """Try repeated simple compilations"""
    def daemon_lifetime(self):
        return 120
    
    def runtest(self):
        # may take about a minute or so
        for i in xrange(1000):
            self.runcmd("distcc cc -o testtmp.o -c testtmp.c")


class Concurrent_Case(CompileHello_Case):
    """Try many compilations at the same time"""
    def daemon_lifetime(self):
        return 120
    
    def runtest(self):
        # may take about a minute or so
        pids = {}
        for i in xrange(100):
            kid = self.runcmd_background("distcc cc -o testtmp.o -c testtmp.c")
            pids[kid] = kid
        while len(pids):
            pid, status = os.wait()
            if status:
                self.fail("child %d failed with status %#x" % (pid, status))
            del pids[pid]


class BigAssFile_Case(Compilation_Case):
    """Test compilation of a really big C file

    This will take a while to run"""
    def createSource(self):
        """Create source file"""
        f = open("testtmp.c", 'wt')

        # We want a file of many, which will be a few megabytes of
        # source.  Picking the size is kind of hard -- something that
        # will properly exercise distcc may be too big for small/old
        # machines.
        
        f.write("int main() {}\n")
        for i in xrange(200000):
            f.write("int i%06d = %d;\n" % (i, i))
        f.close()

    def runtest(self):
        self.runcmd("distcc cc -c %s" % "testtmp.c")
        self.runcmd("distcc cc -o testtmp testtmp.o")



class BinFalse_Case(Compilation_Case):
    """Compiler that fails without reading input.

    This is an interesting case when the server is using fifos,
    because it has to cope with the open() on the fifo being
    interrupted.

    distcc doesn't know that 'false' is not a compiler, but it does
    need a command line that looks like a compiler invocation.

    We have to use a .i file so that distcc does not try to preprocess it.
    """
    def createSource(self):
        open("testtmp.i", "wt").write("int main() {}")
        
    def runtest(self):
        self.runcmd("distcc false -c testtmp.i", 1)


class BinTrue_Case(Compilation_Case):
    """Compiler that succeeds without reading input.

    This is an interesting case when the server is using fifos,
    because it has to cope with the open() on the fifo being
    interrupted.

    distcc doesn't know that 'true' is not a compiler, but it does
    need a command line that looks like a compiler invocation.

    We have to use a .i file so that distcc does not try to preprocess it.
    """
    def createSource(self):
        open("testtmp.i", "wt").write("int main() {}")
        
    def runtest(self):
        self.runcmd("distcc true -c testtmp.i", 0)


class SBeatsC_Case(CompileHello_Case):
    """-S overrides -c in gcc.

    If both options are given, we have to make sure we imply the
    output filename in the same way as gcc."""
    # XXX: Are other compilers the same?
    def runtest(self):
        self.runcmd("distcc gcc -c -S testtmp.c")
        if os.path.exists("testtmp.o"):
            self.fail("created testtmp.o but should not have")
        if not os.path.exists("testtmp.s"):
            self.fail("did not create testtmp.s but should have")

    
class NoServer_Case(CompileHello_Case):
    """Invalid server name"""
    def setup(self):
        self.stripEnvironment()
        os.environ['DISTCC_HOSTS'] = 'no.such.host.here'
        self.distcc_log = 'distcc.log'
        os.environ['DISTCC_LOG'] = self.distcc_log
        self.createSource()
    
    def runtest(self):
        self.runcmd("distcc gcc -c -o testtmp.o testtmp.c")
        msgs = open(self.distcc_log, 'r').read()
        self.assert_re_search(r'failed to distribute.*running locally instead',
                              msgs)            
        
        
class ImpliedOutput_Case(CompileHello_Case):
    """Test handling absence of -o"""
    def compileCmd(self):
        return "distcc cc -c testtmp.c"


class SyntaxError_Case(Compilation_Case):
    """Test building a program containing syntax errors, so it won't build
    properly."""
    def source(self):
        return """not C source at all
"""

    def compile(self):
        rc, msgs, errs = self.runcmd_unchecked(self.compileCmd())
        self.assert_notequal(rc, 0)
        # XXX: Need to also handle "syntax error" from gcc-2.95.3
        self.assert_re_match(r'testtmp.c:1: .*error', errs)
        self.assert_equal(msgs, '')

    def runtest(self):
        self.compile()

        if os.path.exists("testtmp") or os.path.exists("testtmp.o"):
            self.fail("compiler produced output, but should not have done so")


class NoHosts_Case(CompileHello_Case):
    """Test running with no hosts defined.

    We expect compilation to succeed, but with a warning that it was
    run locally."""
    def runtest(self):
        import os
        
        # WithDaemon_Case sets this to point to the local host, but we
        # don't want that.  Note that you cannot delete environment
        # keys in Python1.5, so we need to just set them to the empty
        # string.
        os.environ['DISTCC_HOSTS'] = ''
        os.environ['DISTCC_LOG'] = ''
        self.runcmd('printenv')
        msgs, errs = self.runcmd(self.compileCmd())

        # We expect only one message, a warning from distcc
        self.assert_re_search(r"Warning.*\$DISTCC_HOSTS.*can't distribute work",
                              errs)


class MissingCompiler_Case(CompileHello_Case):
    """Test compiler missing from server."""
    # Another way to test this would be to break the server's PATH
    def sourceFilename(self):
        # must be preprocessed, so that we don't need to run the compiler
        # on the client
        return "testtmp.i"

    def source(self):
        return """int foo;"""

    def runtest(self):
        msgs, errs = self.runcmd("distcc nosuchcc -c testtmp.i",
                                 expectedResult=EXIT_COMPILER_MISSING)
        self.assert_re_search(r'failed to exec.*No such file or directory', errs)
        


class RemoteAssemble_Case(CompileHello_Case):
    """Test remote assembly of a .s file."""

    # We have a rather tricky method for testing assembly code when we
    # don't know what platform we're on.  I think this one will work
    # everywhere, though perhaps not.
    asm_source = """
        .file	"foo.c"
	.version	"01.01"
gcc2_compiled.:
.globl msg
.section	.rodata
.LC0:
	.string	"hello world"
.data
	.align 4
	.type	 msg,@object
	.size	 msg,4
msg:
	.long .LC0
	.ident	"GCC: (GNU) 2.95.4 20011002 (Debian prerelease)"
"""

    def source(self):
            return """
#include <stdio.h>

extern const char *msg;

int main(void) {
    puts(msg);
    return 0;
}
"""
    asm_filename = 'test2.s'

    def setup(self):
        CompileHello_Case.setup(self)
        open(self.asm_filename, 'wt').write(self.asm_source)
        
        # TODO: Delete temp files

    def compile(self):
        # Need to build both the C file and the assembly file
        self.runcmd("distcc cc -o test2.o -c test2.s")
        self.runcmd("distcc cc -o testtmp.o -c testtmp.c")
        self.runcmd(self.linkCmd())

    def asmCmd(self):
        return "distcc cc -o test2.o -c %s" % (self.asm_filename)

    def linkCmd(self):
        return "distcc cc -o testtmp testtmp.o test2.o"


class PreprocessAsm_Case(RemoteAssemble_Case):
    """Run preprocessor locally on assembly, then compile locally."""
    asm_source = """
#define MSG "hello world"
gcc2_compiled.:
.globl msg
.section	.rodata
.LC0:
	.string	 MSG
.data
	.align 4
	.type	 msg,@object
	.size	 msg,4
msg:
	.long .LC0
"""
    asm_filename = 'test2.S'
    
    def compile(self):
        self.runcmd("distcc cc -o test2.o -c test2.S")
        self.runcmd("distcc cc -o testtmp.o -c testtmp.c")
        self.runcmd("distcc cc -o testtmp testtmp.o test2.o")


class ModeBits_Case(CompileHello_Case):
    """Check distcc obeys umask"""
    def runtest(self):
        self.runcmd("umask 0; distcc -c testtmp.c")
        self.assert_equal(S_IMODE(os.stat("testtmp.o")[ST_MODE]), 0666)


class CheckRoot_Case(comfychair.TestCase):
    """Stub case that checks this is run by root.  Not used by default."""
    def setup(self):
        self.require_root()


class EmptySource_Case(Compilation_Case):
    """Check compilation of empty source file

    It must be treated as preprocessed source, otherwise cpp will
    insert a # line, which will give a false pass.  """
    
    def source(self):
        return ''

    def runtest(self):
        self.compile()

    def compile(self):
        self.runcmd("distcc cc -c %s" % self.sourceFilename())

    def sourceFilename(self):
        return "testtmp.i"

class BadLogFile_Case(comfychair.TestCase):
    def runtest(self):
        self.runcmd("touch distcc.log")
        self.runcmd("chmod 0 distcc.log")
        msgs, errs = self.runcmd("DISTCC_LOG=distcc.log distcc -c foo.c",
                                expectedResult=1)
        self.assert_re_search("failed to open logfile", errs)


# All the tests defined in this suite
tests = [BadLogFile_Case,
         ScanArgs_Case,
         ImplicitCompilerScan_Case,
         StripArgs_Case,
         StartStopDaemon_Case,
         CompileHello_Case,
         CppError_Case,
         BadInclude_Case,
         PreprocessPlainText_Case,
         NoDetachDaemon_Case,
         SBeatsC_Case,
         DashD_Case,
         BinFalse_Case,
         BinTrue_Case,
         VersionOption_Case,
         HelpOption_Case,
         BogusOption_Case,
         MultipleCompile_Case,
         GccOptionsPassed_Case,
         IsSource_Case,
         ExtractExtension_Case,
         ImplicitCompiler_Case,
         DaemonBadPort_Case,
         NoServer_Case,
         InvalidHostSpec_Case,
         ParseHostSpec_Case,
         ImpliedOutput_Case,
         SyntaxError_Case,
         NoHosts_Case,
         MissingCompiler_Case,
         RemoteAssemble_Case,
         PreprocessAsm_Case,
         ModeBits_Case,
         EmptySource_Case,
         Concurrent_Case,
         ThousandFold_Case,
         BigAssFile_Case]


if __name__ == '__main__':
    comfychair.main(tests)
