#!/Applications/Server.app/Contents/ServerRoot/Library/CalendarServer/bin/python
#
# Author:: Apple Inc.
# Documentation:: Apple Inc.
# Copyright (c) 2013-2015 Apple Inc. All Rights Reserved.
#
# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.
# License:: All rights reserved.
#

"""
Tests for the C{xpostgres} tool.
"""

import os
import plistlib
import re

from twisted.trial.unittest import TestCase

from twisted.python.filepath import FilePath
from twisted.python.components import proxyForInterface
from twisted.python.failure import Failure

from twisted.internet.defer import succeed
from twisted.internet.base import ReactorBase
from twisted.internet.error import ProcessTerminated, ProcessDone
from twisted.internet.interfaces import IReactorCore

from twisted.test.proto_helpers import StringTransport

# Note: as a test API, this is private.  At some point in the future when
# Twisted is upgraded, we'll need to update the tests that use this.

# Keep an eye on <http://twistedmatrix.com/trac/ticket/6599>.

from twisted.runner.test.test_procmon import DummyProcessReactor as _Parent

import xpostgres
from xpostgres import XPostgres, ControlServer
from xpostgres import XPGCtl
from xpostgres import CtlStart
from xpostgres import NoDataDirectory
from xpostgres import DEFAULT_SOCKET_DIR
from xpostgres import RESTORE_ON_ABSENCE_FILE
from xpostgres import POSTGRES
from xpostgres import WAIT4PATH
from xpostgres import ARCHIVE_LOG_DIRECTORY_NAME
from xpostgres import InheritableFilesystemLock

class ReactorCoreExtras(ReactorBase, object):
    """
    Twisted ought to provide a more feature-complete test fixture for all
    reactor APIs, but in lieu of that let's have a fixture which is both
    realistic and does no I/O.
    """

    # Again, private API, but there's no other way to opt out of this behavior.
    _registerAsIOThread = False

    def installWaker(self):
        pass


    def run(self):
        """
        Non-blocking implementation of 'run', since 'startRunning' isn't
        exposed through IReactorCore.
        """
        self.startRunning()


    def stop(self):
        super(ReactorCoreExtras, self).stop()
        self.runUntilCurrent()


    def removeAll(self):
        """
        Stub implementation to model shutdown accurately.
        """
        return []



class DummyProcessReactor(_Parent,
                          proxyForInterface(IReactorCore, '_coreExtras')):

    def __init__(self):
        super(DummyProcessReactor, self).__init__()
        self._coreExtras = ReactorCoreExtras()


    def spawnProcess(self, *args, **kw):
        """
        The super implementation just invokes processEnded from its fixtures.
        We would rather invoke processExited first, since that is the more
        realistic behavior, and xpostgres generally cares about process
        termination and not stream closure.
        """
        proct = super(DummyProcessReactor, self).spawnProcess(*args, **kw)
        realEnded = proct.proto.processEnded
        def exitedBeforeEnded(reason):
            proct.proto.processExited(reason)
            realEnded(reason)
        proct.proto.processEnded = exitedBeforeEnded



sample_pidfile = """
42299
/path/to/a/cluster
1372403088
5432
/path/to/a/socket
localhost
  5432001   1900544
"""

class XPostgresTest(TestCase):

    def makePlist(self, args=()):
        """
        Make a temporary property list containing some arguments and return its
        name.
        """
        pltemp = self.mktemp()
        plistlib.writePlist(dict(ProgramArguments=list(args)), pltemp)
        return pltemp


    def test_parse_command_line(self):
        xpg = XPostgres(DummyProcessReactor())
        plname = self.makePlist()
        xpg.parse_command_line(["xpostgres", "-a", plname, "-D",
                                "data_directory", "-k", "socket_directory"],
                               {})
        self.assertEqual(xpg.plist_path, plname)
        self.assertEqual(xpg.data_directory, "data_directory")
        self.assertEqual(xpg.socket_directory, "socket_directory")
        self.assertEqual(xpg.archive_log_directory, ARCHIVE_LOG_DIRECTORY_NAME)


    def test_bequeathing_lock(self):
        """
        Bequeathing a lock squirrels away an environment variable via
        os.environ, in the form of a JSON dictionary mapping absolute path to
        the lock.
        """
        reactor = DummyProcessReactor()

        lockp1 = self.mktemp()
        lockp2 = self.mktemp()

        sharedEnviron = {}

        lock1 = InheritableFilesystemLock(lockp1, reactor)
        lock2 = InheritableFilesystemLock(lockp2, reactor)

        lock2.environ = lock1.environ = sharedEnviron

        lock1.lock()
        called1 = []
        called2 = []
        d1 = lock1.bequeath()
        d1.addCallback(called1.append)
        d2 = lock2.bequeath()
        d2.addErrback(called2.append)
        self.assertEquals(ValueError, called2[0].type)
        lock2.lock()

        lock1prime = InheritableFilesystemLock(lockp1, reactor)
        lock2prime = InheritableFilesystemLock(lockp2, reactor)

        lock2prime.environ = lock1prime.environ = sharedEnviron

        lock1prime.getpid = lambda: 54321
        lock2prime.getpid = lambda: 65432

        reactor.advance(10.)
        self.assertEquals(called1, [])
        self.assertEquals(lock1prime.lock(), True)
        self.assertEquals(lock1prime.locked, True)
        self.assertEquals(lock2prime.lock(), False)
        self.assertEquals(lock2prime.locked, False)

        # As these will normally be in distinct processes, there's no way to
        # know that the callbacks will be invoked; we have to let time
        # progress.
        self.assertEquals(called1, [])

        reactor.advance(10.)
        self.assertEquals(called1, [None])

        self.assertEquals(os.readlink(lock1prime.name), "54321")


    def test_command_line_excludes_plist_option(self):
        """
        The C{-a} option is specific to C{xpostgres} and won't be passed on to
        postgres.
        """
        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(["xpostgres", "-a", self.makePlist(), "-D",
                                "data_directory", "-k", "socket_directory"],
                               {})
        self.assertEqual(xpg.postgres_argv,
                         ["-D", "data_directory", "-k", "socket_directory"])


    def test_arguments_plist(self, option='-a'):
        """
        The C{-a} option extends the command line from a property list
        including the L{ProgramArguments} key.
        """
        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(
            [
                "xpostgres", option, self.makePlist(
                    ["-D", "data_directory", "-k", "socket_directory"]
                )
            ], {}
        )
        self.assertEqual(xpg.data_directory, "data_directory")
        self.assertEqual(xpg.socket_directory, "socket_directory")
        self.assertEqual(xpg.archive_log_directory, ARCHIVE_LOG_DIRECTORY_NAME)
        self.assertEqual(xpg.postgres_argv,
                         ["-D", "data_directory", "-k", "socket_directory"])


    def test_arguments_plist_long(self):
        """
        The C{--apple-configuration} option has the same effect as C{-a}.
        """
        self.test_arguments_plist("--apple-configuration")


    def test_command_line_postgres_overrides(self):
        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(["xpostgres",
                                "-c", "unix_socket_directories=socket_dir", "-D",
                                "data_directory"], {})
        self.assertEqual(xpg.socket_directory, "socket_dir")
        xpg.parse_command_line(["xpostgres",
                                "-c", "log_directory=log_directory"], {})
        self.assertEqual(xpg.log_directory, "log_directory")


    def test_parse_environment(self):
        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(["xpostgres"], dict(PGDATA="data_directory"))
        self.assertEqual(xpg.data_directory, "data_directory")


    def test_default_input_args(self):
        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(["xpostgres", "-D", "data_directory"], {})
        self.assertEqual(xpg.socket_directory, DEFAULT_SOCKET_DIR)


    def test_data_directory_is_defined(self):
        xpg = XPostgres(DummyProcessReactor())
        self.assertRaises(NoDataDirectory, xpg.parse_command_line,
                          ["xpostgres"], {})


    def test_data_directory_create(self):
        temp = self.mktemp()
        os.makedirs(temp)
        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(
            ["xpostgres", "-c", "unix_socket_directories=socket_dir",
             "-D", temp], {}
        )
        xpg.preflight()
        backup_dir = os.path.join(os.path.dirname(temp),
                                  ARCHIVE_LOG_DIRECTORY_NAME)
        self.assertEqual(True, os.path.isdir(backup_dir))
        self.assertEqual(0o0700, os.stat(backup_dir).st_mode & 0o0777)


    def test_set_restore_if_tinkle_missing(self):
        data_dir = self.mktemp()
        backup_dir = os.path.join(os.path.dirname(data_dir),
                                  ARCHIVE_LOG_DIRECTORY_NAME)
        os.mkdir(data_dir, 0o700)
        os.mkdir(backup_dir, 0o700)

        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line([
            "xpostgres", "-c", "unix_socket_directories=socket_dir",
            "-D", data_dir],
            {}
        )
        xpg.backup_zip_file.parent().createDirectory()
        xpg.backup_zip_file.touch()
        xpg.preflight()
        self.assertEqual(True, xpg.restore_before_run)


    def test_start_postgres_if_tinkle_missing_but_data_present(self):
        """
        If the tinkle file is missing but the data directory is present,
        L{XPostgres.do_everything} will continue to start postgres.
        """
        xpg = self.xpg_with_backup_dir(tinkle_exists=False)
        started = []
        def record_start():
            started.append(True)
            return succeed(None)
        self.reactor = DummyProcessReactor()
        data_dir = self.mktemp()
        backup_dir = os.path.join(os.path.dirname(data_dir),
                                  ARCHIVE_LOG_DIRECTORY_NAME)
        os.mkdir(data_dir, 0o700)
        os.mkdir(backup_dir, 0o700)
        xpg = XPostgres(self.reactor)
        xpg.do_everything([xpostgres.__file__, "-D", data_dir,
                           "-k", self.mktemp()], {})


    def test_do_everything_shutdown(self):
        """
        L{XPostgres.do_everything} adds some shutdown hooks; ensure that
        they're executed.
        """
        sktdir = self.mktemp()
        os.mkdir(sktdir)
        xpg = self.xpg_with_backup_dir()
        xpg.reactor.run()
        data_dir = self.mktemp()
        xpg.do_everything([xpostgres.__file__, "-D", data_dir, "-k", sktdir],
                          {})
        # Slightly artificial: in real life, we'd have both the UNIX socket and
        # the lockfile; in test, MemoryReactor's pretend UNIX socket doesn't
        # touch the filesystem.
        self.assertEquals(len(os.listdir(sktdir)), 1)
        xpg.reactor.stop()
        # Lock file should be removed.
        self.assertEquals(os.listdir(sktdir), [])


    def test_no_restore_if_tinkle_present(self):
        data_dir = self.mktemp()
        backup_dir = self.mktemp()
        os.mkdir(data_dir, 0o700)
        os.mkdir(backup_dir, 0o700)
        tinkle = os.path.join(data_dir, RESTORE_ON_ABSENCE_FILE)
        open(tinkle, "wb").close()

        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(
            ["xpostgres", "-c", "unix_socket_directories=socket_dir", "-D",
             data_dir], {}
        )
        xpg.preflight()
        self.assertEqual(False, xpg.restore_before_run)


    def archive_disk_has_size(self, xpg, total_kilobytes, available_kilobytes):
        """
        Simulate an archive disk of a certain size.  U{See
        <http://pubs.opengroup.org/onlinepubs/009695399/basedefs/sys/statvfs.h.html>}
        """
        class fake_statvfs(object):
            def __init__(self, path):
                self._path = path
                self.f_frsize = 1024
                self.f_bavail = available_kilobytes
                self.f_blocks = total_kilobytes
        xpg.statvfs = fake_statvfs


    def test_log_message(self):
        pass


    def test_disk_statistics(self):
        """
        L{XPostgres.archive_disk_capacity_gigabytes} returns the capacity of
        the disk storing its archive, in gigabytes;
        L{XPostgres.archive_disk_capacity_gigabytes}.
        """
        xpg = self.xpg_with_backup_dir()
        million = (1024 ** 2)
        self.archive_disk_has_size(xpg, 50 * million, 10 * million)
        self.assertEquals(xpg.archive_disk_capacity_gigabytes(), 50)
        self.assertEquals(xpg.archive_disk_available_gigabytes(), 10)


    def test_max_archive_gigabytes(self):
        """
        L{XPostgres.max_archive_gigabytes} should determine the maximum
        allowable backup size, in gigabytes, for the filesystem it's on (by
        using statvfs on its backup directory).  Once a backup greater than
        this size is detected, it will trigger a new base-backup and a WAL log
        cleanup.
        """
        megs = (1024 * 1024)
        xpg = self.xpg_with_backup_dir()
        # 50 GB or less: Max 5  GB
        for gigs in (45, 46, 49):
            self.archive_disk_has_size(xpg, gigs * megs, 4321)
            self.assertEquals(xpg.max_archive_gigabytes(), 5)
        #     50-100 GB: Max 10 GB
        for gigs in (51, 75, 90):
            self.archive_disk_has_size(xpg, gigs * megs, 4321)
            self.assertEquals(xpg.max_archive_gigabytes(), 10)
        #    100-200 GB: Max 20 GB
        for gigs in (105, 190):
            self.archive_disk_has_size(xpg, gigs * megs, 4321)
            self.assertEquals(xpg.max_archive_gigabytes(), 20)
        #       200+ GB: Max 30 GB
        for gigs in (200, 500, 1000, 3000):
            self.archive_disk_has_size(xpg, gigs * megs, 4321)
            self.assertEquals(xpg.max_archive_gigabytes(), 30)


    def test_archive_log_bytes(self):
        """
        L{XPostgres.archive_log_bytes} returns the amount of storage consumed
        by the C{archive_log_directory}.
        """
        xpg = self.xpg_with_backup_dir()
        d = FilePath(xpg.archive_log_directory)
        d.child("one").setContent("x" * 100)
        d.child("two").setContent("y" * 10)
        d.child("three").setContent("z" * 7)
        self.assertEquals(xpg.archive_log_bytes(), 117)


    def xpg_with_backup_dir(self, data_exists=True,
                            backup_exists=True,
                            tinkle_exists=True):
        """
        Create an L{XPostgres} object with a data directory.
        """
        self.reactor = DummyProcessReactor()
        data_dir = self.mktemp()
        socket_dir = self.mktemp()
        backup_dir = os.path.join(os.path.dirname(data_dir),
                                  ARCHIVE_LOG_DIRECTORY_NAME)
        if data_exists:
            os.mkdir(data_dir, 0o700)
        if backup_exists:
            os.mkdir(backup_dir, 0o700)
        self.tinkle = os.path.join(data_dir, RESTORE_ON_ABSENCE_FILE)
        if tinkle_exists:
            open(self.tinkle, "wb").close()
        xpg = XPostgres(self.reactor)
        xpg.parse_command_line([xpostgres.__file__, "-k", socket_dir,
                               "-D", data_dir], {})
        return xpg


    def test_touch_existing_dotfile(self):
        """
        L{XPostgres.touch_dotfile} will update an existing backup marker
        dotfile by updating its mtime.
        """
        xpg = self.xpg_with_backup_dir()
        self.reactor.advance(500)
        result = xpg.touch_dotfile()
        self.assertEquals(os.path.getmtime(self.tinkle), 500)
        done = []
        result.addCallback(done.append)
        self.assertEquals(done, [None])


    def test_touch_new_dotfile(self):
        """
        L{XPostgres.touch_dotfile} will create a new dotfile if one does not
        exist, then spawn C{tmutil} to add an exclusion for the dotfile.

        It returns a L{Deferred} which fires when C{tmutil} completes.
        """
        xpg = self.xpg_with_backup_dir(tinkle_exists=False)
        result = xpg.touch_dotfile()
        self.assertEquals(True, os.path.exists(self.tinkle))
        self.assertEqual(len(self.reactor.spawnedProcesses), 1)
        self.assertEqual(self.reactor.spawnedProcesses[0]._executable,
                         '/usr/bin/tmutil')
        self.assertEqual(list(self.reactor.spawnedProcesses[0]._args),
                         ['/usr/bin/tmutil', 'addexclusion', self.tinkle])
        done = []
        result.addCallback(done.append)
        self.assertEquals(done, [])
        out = err = ''
        status = 0
        self.reactor.spawnedProcesses[0].processEnded(status)
        self.assertEquals(done, [(out, err, status)])


    def test_basebackup_retry(self):
        """
        do_backup will re-try the basebackup command until it succeeds, and
        will not fire until it succeeds.
        """
        xpg = self.xpg_with_backup_dir(data_exists=True, backup_exists=False,
                                       tinkle_exists=False)
        xpg.preflight()
        results = []
        d = xpg.do_backup()
        d.addCallback(results.append)
        self.assertEquals(len(self.reactor.spawnedProcesses), 1)
        proc = self.reactor.spawnedProcesses.pop()
        self.assertTrue(proc._executable.endswith("/pg_basebackup"))
        self.assertEquals(results, [])
        # Partial failure; let's make sure that doesn't get in there.
        STDOUT = 1
        os.write(proc._childFDs[STDOUT], 'BAD')
        proc.proto.processExited(Failure(ProcessTerminated(status=1)))
        self.assertEquals(results, [])
        self.reactor.advance(2.0)
        self.assertEquals(len(self.reactor.spawnedProcesses), 1)
        proc = self.reactor.spawnedProcesses.pop()
        os.write(proc._childFDs[STDOUT], 'GOOD')
        proc.proto.processExited(Failure(ProcessDone(0)))
        self.assertEquals(results, [None])
        self.assertEquals(xpg.backup_zip_file.getContent(), 'GOOD')


    def system_shutting_down_test(self, output, result):
        """
        L{XPostgres.system_is_shutting_down} returns a L{Deferred} which fires
        with True if the system is shutting down.  (It uses 'notifyutil' to
        make this determination.)
        """
        xpg = self.xpg_with_backup_dir()
        d = xpg.system_is_shutting_down()
        self.assertEqual(len(self.reactor.spawnedProcesses), 1)
        self.assertEqual(self.reactor.spawnedProcesses[0]._executable,
                         '/usr/bin/notifyutil')
        self.assertEqual(list(self.reactor.spawnedProcesses[0]._args),
                         ['/usr/bin/notifyutil', '-g',
                          'com.apple.system.loginwindow.shutdownInitiated'])
        self.reactor.spawnedProcesses[0].proto.childDataReceived(
            1, output
        )
        shuttingDown = []
        d.addCallback(shuttingDown.append)
        self.assertEqual(shuttingDown, [])
        self.reactor.spawnedProcesses[0].processEnded(0)
        self.assertEqual(shuttingDown, [result])


    def test_wait_for_receivexlog(self):
        """
        L{XPostgres.start_receivexlog} returns a L{Deferred} that fires when an
        identifiable log message is emitted, and adds a system event trigger
        which delays shutdown until the pg_receivexlog process has exited.
        """
        # setup
        xpg = self.xpg_with_backup_dir()
        xpg.reactor.run()
        def shutted():
            shutted.down += 1
        shutted.down = 0
        xpg.reactor.addSystemEventTrigger('after', 'shutdown', shutted)
        def started(result):
            started.up += 1
            started.result = result
        started.up = 0

        # test
        d = xpg.start_receivexlog()
        d.addCallback(started)
        self.assertEquals(started.up, 0)
        self.assertEquals(len(xpg.reactor.spawnedProcesses), 1)
        proc = xpg.reactor.spawnedProcesses[0]
        prot = proc.proto
        prot.childDataReceived(2, "starting log streaming")
        self.assertEquals(started.up, 1)
        self.assertEquals(shutted.down, 0)
        xpg.reactor.stop()
        self.assertEquals(shutted.down, 0)
        prot.processExited(Failure(ValueError()))
        self.assertEquals(shutted.down, 1)


    def test_wait_for_stop_client(self):
        """
        L{XPostgres} responds to a 'decref' message.  If it is about to exit,
        it should delay emitting its response until shutdown is underway and
        almost completed; it should then further delay shutdown until the
        client has confirmed receipt of that response by disconnecting.

        (Note, not tested here: locks should be released in advance, so if this
        were to hang around for a long time it should not be catastrophic in
        the sense that it will not prevent future xpostgres launches.)
        """
        xpg = self.xpg_with_backup_dir()
        xpg.reactor.run()
        xpg.start_postgres()
        # wait4path
        self.assertEquals(len(xpg.reactor.spawnedProcesses), 1)
        xpg.reactor.spawnedProcesses.pop().processEnded(0)
        # And here's postgres, finally.
        self.assertEquals(len(xpg.reactor.spawnedProcesses), 1)
        xpg.start_receivexlog()
        def shutted():
            shutted.down += 1
        shutted.down = 0
        xpg.reactor.addSystemEventTrigger("during", "shutdown", shutted)

        def processNames():
            return [x._executable for x in xpg.reactor.spawnedProcesses
                    if x.pid is not None]

        self.assertEquals(len(xpg.reactor.spawnedProcesses), 2)

        # stop the whole thing
        control = ControlServer(xpg)
        control.makeConnection(StringTransport())
        d = control.decref()
        def responded(ok):
            responded.times += 1
            responded.result = ok
        d.addCallback(responded)
        responded.times = 0
        self.assertEquals(shutted.down, 0)
        # PSQL instruction to shut down
        xpg.reactor.spawnedProcesses.pop().processEnded(0)

        # We send postgres a signal, and DummyProcess._terminationDelay = 1.
        xpg.reactor.advance(2.0)
        # Postgres termination sends pg_receivexlog a signal.
        xpg.reactor.advance(2.0)
        # Sanity check: all processes should have exited now.
        self.assertEqual(processNames(), [])
        # OK... now that the subprocesses have all gone away, we should be
        # issuing our response to the client.
        self.assertEquals(responded.times, 1)
        self.assertEquals(shutted.down, 0)
        control.connectionLost(Failure(MemoryError()))
        self.assertEquals(shutted.down, 1)


    def test_system_shutting_down_yes(self):
        self.system_shutting_down_test(
            "com.apple.system.loginwindow.shutdownInitiated 0\n",
            False
        )


    def test_system_shutting_down_no(self):
        self.system_shutting_down_test(
            "com.apple.system.loginwindow.shutdownInitiated: Failed\n",
            True
        )


    def test_log_pruning(self):
        """
        L{XPostgres.prune_useless_archive_logs} will remove unhelpful log
        files.
        """
        xpg = self.xpg_with_backup_dir()
        folder = FilePath(xpg.archive_log_directory)
        folder.child("base_backup").createDirectory()
        folder.child("unknown").touch()
        folder.child("foo.bar.partial").touch()
        folder.child("foo.bar").touch()
        (folder.child("something").temporarySibling(".in-progress")
         .open().close())

        xpg.prune_useless_archive_logs()
        self.assertEquals(set([c.basename() for c in folder.children()]),
                          set(['base_backup', 'unknown', 'foo.bar']))


    def test_unpartialize(self):
        xpg = self.xpg_with_backup_dir()
        folder = FilePath(xpg.archive_log_directory)
        folder.child("1").touch()
        folder.child("2").touch()
        folder.child("3").touch()
        folder.child("4.partial").touch()
        xpg.unpartialize()
        self.assertEquals(set([c.basename() for c in folder.children()]),
                          set(['1', '2', '3', '4']))


    def test_start_postgres(self):
        """
        L{XPostgres.start_postgres} will wait for the data path to exist, clean
        up lock and socket files, and start postgres.
        """
        data_dir = self.mktemp()
        os.mkdir(data_dir, 0o700)
        dpr = DummyProcessReactor()
        xpg = XPostgres(dpr)
        sktd = self.mktemp()
        xpg.parse_command_line(
            ['xpostgres', '-k', sktd], {'PGDATA': data_dir})
        xpg.preflight()
        started = xpg.start_postgres()
        self.assertEqual(len(dpr.spawnedProcesses), 1)
        self.assertEquals(list(dpr.spawnedProcesses[0]._args),
                          [WAIT4PATH, os.path.abspath(xpg.data_directory)])
        # It waits for 'wait4path' to exit, then invokes PG ...
        dpr.spawnedProcesses[0].processEnded(0)
        self.assertEquals(len(dpr.spawnedProcesses), 2)
        self.assertEqual(dpr.spawnedProcesses[-1]._args,
                         [POSTGRES, '-k', sktd])
        self.assertEqual(dpr.spawnedProcesses[-1]._executable, POSTGRES)
        self.assertEqual(dpr.spawnedProcesses[-1]._environment,
                         {"PGDATA": data_dir})
        l = []
        started.addCallback(l.append)
        dpr.advance(5)
        self.assertEquals(l, [])
        sd = FilePath(sktd)
        dpr.advance(2)
        self.assertEquals(l, [])
        sd.child(".s.PGSQL.5432").touch()
        dpr.advance(2)
        self.assertEquals(l, [None])


    def test_toggle_wal_archive_logging(self):
        """
        L{XPostgres.toggle_wal_archive_logging} will update two postgres
        configuration files as needed to enable or disable WAL archiving.
        """
        hba_config_default = """\
# "local" is for Unix domain socket connections only
local   all             all                                     trust
# IPv4 local connections:
host    all             all             127.0.0.1/32            trust
# IPv6 local connections:
host    all             all             ::1/128                 trust
# Allow replication connections from localhost, by a user with the
# replication privilege.
#local   replication     _postgres                                trust
#host    replication     _postgres        127.0.0.1/32            trust
#host    replication     _postgres        ::1/128                 trust
"""
        postgres_config_default = """# -----------------------------
#wal_level = minimal			# minimal, archive, or hot_standby
                                # (change requires restart)
#archive_mode = off		# allows archiving to be done
                        # (change requires restart)
#archive_command = ''		# command to use to archive a logfile segment
                            # placeholders: %p = path of file to archive
                            #               %f = file name only
# e.g. 'test ! -f /mnt/server/archivedir/%f && cp %p /mnt/server/archivedir/%f'

#archive_timeout = 0		# force a logfile segment switch after this
                            # number of seconds; 0 disables
#max_wal_senders = 0		# max number of walsender processes
                            # (change requires restart)
#some random stuff that is not updated
"""
        data_dir = self.mktemp()
        os.mkdir(data_dir, 0o700)
        postgres_conf_path = os.path.join(data_dir, "postgresql.conf")
        config_file = open(postgres_conf_path, "wb")
        config_file.write(postgres_config_default)
        config_file.close()
        pg_hba_file = open(os.path.join(data_dir, "pg_hba.conf"), "wb")
        pg_hba_file.write(hba_config_default)
        pg_hba_file.close()

        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(["xpostgres", "-D", os.path.abspath(data_dir)],
                               {})
        xpg.toggle_wal_archive_logging(True)
        config_file = open(postgres_conf_path, "rb")
        self.assertEqual(True,
                         xpg.wal_archiving_is_enabled(config_file.readlines()))
        config_file.close()
        xpg.toggle_wal_archive_logging(False)
        config_file = open(postgres_conf_path, "rb")
        self.assertEqual(False,
                         xpg.wal_archiving_is_enabled(config_file.readlines()))
        config_file.close()


    def test_enable_connection_restriction(self):
        """
        L{XPostgres.enable_connection_restriction} will configure pg_hba.conf
        so that only local replication connections are accepted.  All others
        should be denied.
        """
        hba_config_default = """\
# "local" is for Unix domain socket connections only
local   all             all                                     trust
# IPv4 local connections:
host    all             all             127.0.0.1/32            trust
# IPv6 local connections:
host    all             all             ::1/128                 trust
# Allow replication connections from localhost, by a user with the
# replication privilege.
local   replication     _postgres                                trust
#host    replication     _postgres        127.0.0.1/32            trust
#host    replication     _postgres        ::1/128                 trust
"""
        data_dir = self.mktemp()
        os.mkdir(data_dir, 0o700)
        pg_hba_file_path = os.path.join(data_dir, "pg_hba.conf")
        pg_hba_file = open(pg_hba_file_path, "wb")
        pg_hba_file.write(hba_config_default)
        pg_hba_file.close()

        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(["xpostgres", "-D", os.path.abspath(data_dir)],
                               {})
        xpg.enable_connection_restriction()

        pg_hba_file = open(pg_hba_file_path, "rb")

        connections_are_restricted = True
        for line in pg_hba_file.readlines():
            matchobj = re.match(r"^#", line)
            if (matchobj):
                continue
            matchobj = re.match(r"^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*(\S*)$",
                                line)
            if (matchobj):
                (type, database, user) = matchobj.group(1, 2, 3)
                if matchobj.group(5):
                    (address, method) = matchobj.group(4, 5)
                # else:
                #     method = matchobj.group(4)

                if database != "replication":
                    connections_are_restricted = False
                    break
        pg_hba_file.close()
        self.assertEqual(True, connections_are_restricted)


    def test_disable_connection_restriction(self):
        """
        L{XPostgres.disable_connection_restriction} will configure pg_hba.conf
        so that any changes made by xpostgres to restrict connections are
        reverted.
        """
        hba_config = """\
# "local" is for Unix domain socket connections only
#local   all             all                                     trust \
# UPDATED BY xpostgres
# IPv4 local connections:
#host    all             all             127.0.0.1/32            trust   \
# UPDATED BY xpostgres
# IPv6 local connections:
#host    all             all             ::1/128                 trust     \
# UPDATED BY xpostgres
# Allow replication connections from localhost, by a user with the
# replication privilege.
local   replication     _postgres                                trust
#host    replication     _postgres        127.0.0.1/32            trust
#host    replication     _postgres        ::1/128                 trust
"""
        data_dir = self.mktemp()
        os.mkdir(data_dir, 0o700)
        pg_hba_file_path = os.path.join(data_dir, "pg_hba.conf")
        pg_hba_file = open(pg_hba_file_path, "wb")
        pg_hba_file.write(hba_config)
        pg_hba_file.close()

        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(["xpostgres", "-D", os.path.abspath(data_dir)],
                               {})
        xpg.disable_connection_restriction()

        pg_hba_file = open(pg_hba_file_path, "rb")

        found_our_cruft = False
        for line in pg_hba_file.readlines():
            matchobj = re.match(r".*UPDATED BY xpostgres.*", line)
            if (matchobj):
                found_our_cruft = True
        pg_hba_file.close()
        self.assertEqual(False, found_our_cruft)


    def test_sanitize_pid_file(self):
        """
        L{XPostgres.sanitize_pid_file} will strip unwanted shared memory addrs
        from the postgres lock file.
        """
        pid_file_content = """\
55487
/Library/Server/Calendar and Contacts/Data/Database.xpg/cluster.pg
1383604692
5432
/var/run/caldavd/ccs_postgres_3d403b3009fe0c830944d87bd751fbe9

        0    131074
"""
        data_dir = self.mktemp()
        os.mkdir(data_dir, 0o700)
        pid_file_path = os.path.join(data_dir, "postmaster.pid")
        pid_file = open(pid_file_path, "wb")
        pid_file.write(pid_file_content)
        pid_file.close()

        xpg = XPostgres(DummyProcessReactor())
        xpg.parse_command_line(["xpostgres", "-D", os.path.abspath(data_dir)],
                               {})
        xpg.sanitize_pid_file()

        pid_file = open(pid_file_path, "rb")
        found_shm_addr = False
        for line in pid_file.readlines():
            matchobj = re.match(r".*131074.*", line)
            if (matchobj):
                found_shm_addr = True
        pid_file.close()
        self.assertEqual(False, found_shm_addr)


class XPGCtlTest(TestCase):
    """
    Tests for C{xpg_ctl} utility.
    """
    def setUp(self):
        self.reactor = DummyProcessReactor()


    def test_parse_start(self):
        """
        Parse the 'start' command.
        """
        pgctl = XPGCtl(self.reactor)
        pgctl.parse_command_line(["xpg_ctl", "-w", "start"], {})
        cmdobj = pgctl.command_object
        self.assertIsInstance(cmdobj, CtlStart)
        self.assertEquals(cmdobj.xpg_ctl.wait, True)
