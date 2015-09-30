#!/Applications/Server.app/Contents/ServerRoot/Library/CalendarServer/bin/python
# -*- test-case-name: test_xpostgres.py -*-
# xpostgres
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
# This tool is a wrapper for postgres.
# Its function is to launch a postgres process and manage WAL archiving.

from __future__ import print_function

import os
import re
import itertools
import sys
import getopt
import datetime
import json
import socket
import shutil
import errno
import psutil

from shlex import split as shell_split

from plistlib import readPlist

from subprocess import Popen, PIPE, STDOUT, call

from pwd import getpwnam, getpwuid

from twisted.internet.protocol import ProcessProtocol, Factory
from twisted.internet.utils import getProcessOutputAndValue
from twisted.internet.defer import (
    succeed, fail, inlineCallbacks, Deferred, maybeDeferred, DeferredList,
    returnValue, DeferredFilesystemLock, TimeoutError
)
from twisted.internet.task import LoopingCall, deferLater
from twisted.internet.error import ConnectError, ProcessDone, ProcessTerminated

from twisted.python.filepath import FilePath
from twisted.python.failure import Failure
from twisted.python.procutils import which
from twisted.internet.endpoints import UNIXClientEndpoint, UNIXServerEndpoint
from setproctitle import setproctitle

CURRENT_POSTGRES_MAJOR_VERSION = '9'
CURRENT_POSTGRES_MINOR_VERSION = '3'
CURRENT_POSTGRES_BINARIES_DIR = '/Applications/Server.app/Contents/ServerRoot/usr/bin'
POSTGRES_BINARIES_DIR_9_0 = '/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.0'
POSTGRES_BINARIES_DIR_9_1 = '/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.1'
POSTGRES_BINARIES_DIR_9_2 = '/Applications/Server.app/Contents/ServerRoot/usr/libexec/postgresql9.2'

TMUTIL = which('tmutil')[0]
NOTIFYUTIL = which('notifyutil')[0]
TAR = which('tar')[0]
WAIT4PATH = which('wait4path')[0]

PG_BASEBACKUP = (
    which('pg_basebackup') or
    ['/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_basebackup']
)[0]
POSTGRES = (
    os.environ.get('XPG_POSTGRES') or # for testing only
    (which('postgres_real') or
     ['/Applications/Server.app/Contents/ServerRoot/usr/bin/postgres_real'])[0]
)
PSQL = (
    os.environ.get('XPG_PSQL') or # for testing only
    (which('psql') or
     ['/Applications/Server.app/Contents/ServerRoot/usr/bin/psql'])[0]
)
PG_RECEIVEXLOG = (
    os.environ.get('XPG_RECEIVEXLOG') or # for testing only
    (which('pg_receivexlog') or
     ['/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_receivexlog'])
    [0]
)
PG_CTL = (
    os.environ.get('XPG_PG_CTL') or # for testing only
    (which('pg_ctl') or
     ['/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_ctl'])[0]
)
PG_DUMPALL = (
    which('pg_dumpall') or
    ['/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_dumpall']
)[0]
INITDB = (
    which('initdb') or
    ['/Applications/Server.app/Contents/ServerRoot/usr/bin/initdb']
)[0]
PG_CONTROLDATA = (
    which('pg_controldata') or
    ['/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_controldata']
)[0]

XPOSTGRES = os.path.realpath(sys.argv[0])

DEFAULT_SOCKET_DIR      = "/var/pgsql_socket"
RESTORE_ON_ABSENCE_FILE = ".NoRestoreNeeded"
MAX_WAL_SENDERS         = "2"  # for postgresql.conf, value for
                               # 'max_wal_senders' preference
ARCHIVE_TIMEOUT         = "0"  # for postgresql.conf, seconds, value for
                               # 'archive_timeout'.  0 because we are using
                               # pg_receivexlog.
GIGS                    = 1024 ** 3 # bytes per gigabyte
MAX_WAL_FILES_GIGS      = 1
DO_NOT_BACKUP_FILE      = ".DoNotBackup"
ARCHIVE_LOG_DIRECTORY_NAME = "backup"
BACKUP_DIRECTORY_NAME   = "base_backup"
BACKUP_ZIP_FILE_NAME    = "base_complete.tar.gz"
MIN_BACKUP_THRESHOLD_SECS = 900
BACKUP_TEMP_FILE_NAME   = "base.tar.gz"
MAINTAINED_LOG_COUNT    = 4
HEARTBEAT_SECS          = 10
XPG_SOCKET_NAME = ".xpg.skt"
TEMP_EXT = '.in-progress'
WAL_FILE_SIZE_BYTES     = 16777216 # NOTE: This default may be changed by rebuilding
                                   # postgres, and that would require changing this value.


# From postgres itself:
LOCK_FILE_LINE_SOCKET_DIR = 5


class NoDataDirectory(Exception):
    """
    No postgres data directory was provided in input.
    """



class NoFiles(Exception):
    """
    Timed out waiting for files to exist.
    """



class Waiter(ProcessProtocol, object):
    def __init__(self):
        self.waiting = []
        self.exit_status = -1


    def wait(self):
        if self.exit_status != -1:
            # process already exited
            returnValue(self.exit_status)

        d = Deferred()
        self.waiting.append(d)
        return d


    def processExited(self, reason):
        if reason.check(ProcessDone):
            value = 0
        elif reason.check(ProcessTerminated):
            value = reason.value.exitCode
        else:
            value = 255
        if self.waiting:
            self.waiting.pop().callback(value)
        else:
            self.exit_status = value
        while self.waiting:
            if reason.check(ProcessDone):
                value = 0
            elif reason.check(ProcessTerminated):
                value = reason.value.exitCode
            else:
                value = 255
            self.waiting.pop().callback(value)



def timestamp():
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")



def log_message(msg):
    """
    Log a message to stdout (which will usually be redirected to a log file).
    """
    print(timestamp() + " XPG." + str(os.getpid()) + ":  " + msg)
    sys.stdout.flush()



def log_nothing(msg):
    """
    Default debug logger: do nothing.
    """

# FIXME: revert before ship:
log_debug = log_message
#if os.environ.get("XPG_LOG_DEBUG"):
#    log_debug = log_message
#else:
#    log_debug = log_nothing



def lock_path(path):
    """
    Given a path, return a name for an adjacent lock.
    """
    return (path + '.lock')



class InheritableFilesystemLock(DeferredFilesystemLock, object):
    """
    A filesystem lock that may be inherited by a subprocess by way of an
    environment varaible propagated to that subprocess during spawning.
    """

    environ = os.environ
    getpid = staticmethod(os.getpid)

    def __init__(self, name, scheduler):
        super(InheritableFilesystemLock, self).__init__(name, scheduler)
        self.scheduler = scheduler


    def lock(self):
        """
        Normally, acquire the filesystem lock as usual.

        However, if the lock has previously been L{bequeathed
        <InheritableFilesystemLock.bequeath>} by a superior process, inherit
        it.
        """
        try:
            linkdata = os.readlink(self.name)
        except OSError:
            linkdata = None
            pass
        got = json.loads(self.environ.get('INHERITABLE_LOCK', '{}'))
        key = os.path.abspath(self.name)
        inherited = got.pop(key, None)
        self.environ['INHERITABLE_LOCK'] = json.dumps(got)
        if inherited is not None and linkdata == inherited:
            log_debug("Accepting inherited lock: " + repr(self.name))
            tempname = self.name + ".inherit"
            # symlink() + rename() means that we atomically overwrite the
            # pid, ideally when both processes are running, so there's no
            # opportunity for anyone else to snatch the lock in the
            # meanwhile.
            os.symlink(str(self.getpid()), tempname)
            os.rename(tempname, self.name)
            self.locked = True
            self.clean = True
            log_debug("Accepted inherited lock: " + repr(self.name))
            return True

        try:
            result = super(InheritableFilesystemLock, self).lock()
            if not result:
                # Locking will fail if the lock file is stale and points to a recycled
                #     PID owned by _the same_ UID.  Clean up the stale PID file and retry if
                #     that is the case.
                locked_pid = int(os.readlink(self.name)) # convert to int
                log_message("Error locking for pid {}".format(locked_pid))
                my_uid = os.getuid()
                for process in psutil.process_iter():
                    owner = process.uids().real
                    pid = process.pid
                    if pid == locked_pid:
                        if owner is not my_uid or \
                                (process.name() != "xpostgres"):
                            log_message("Trying to remove {}".format(self.name))
                            os.remove(self.name)
                            return False
            return result
        except ValueError:
            log_message("Potentially invalid lockfile; lock not acquired.")
            return False
        except OSError:
            try:
                locked_pid = int(os.readlink(self.name)) # convert to int
                log_message("Error locking for pid {}".format(locked_pid))
                # Locking will fail if the lock file is stale and points to a recycled
                #     PID in use by _another_ UID.  Clean up the stale PID file and retry if
                #     that is the case.
                my_uid = os.getuid()
                for process in psutil.process_iter():
                    owner = process.uids().real
                    pid = process.pid
                    if pid == locked_pid and owner is not my_uid:
                        log_message("Trying to remove {}".format(self.name))
                        os.remove(self.name)
                        return False
            except:
                log_message("Could not read PID from path")
                return False
            return False
        else:
            return False


    def bequeath(self, times=30):
        """
        Allow sub-processes to obtain this lock by updating the
        C{INHERITABLE_LOCK} environment mapping.

        Note that since this is giving the lock to a subprocess, it releases
        the lock.
        """
        if self.locked:
            got = json.loads(self.environ.get('INHERITABLE_LOCK', '{}'))
            got[os.path.abspath(self.name)] = str(self.getpid())
            self.environ['INHERITABLE_LOCK'] = json.dumps(got)
            self.locked = False
            def check():
                check.times += 1
                try:
                    value = os.readlink(self.name)
                except:
                    lc.stop()
                    return
                else:
                    if value != str(os.getpid()):
                        lc.stop()
                        return
                if check.times > times:
                    raise TimeoutError()

            check.times = 0
            lc = LoopingCall(check)
            lc.clock = self.scheduler
            return lc.start(1.0).addCallback(lambda call: None)
        return fail(ValueError("Lock not held; cannot inherit."))



def simple_spawn(reactor, args, env):
    """
    Spawn a subprocess, looking up the executable via its argv[0], sharing this
    process's standard (input, output, error), and return a L{Deferred} that
    waits for its completion.

    @param reactor: the reactor to use for spawning.
    @type reactor: L{IReactorProcess}

    @param args: The argv for the subprocess.
    @type args: L{list} of L{bytes}

    @param env: The environment for the subprocess.
    @type env: L{dict}

    @return: a L{Deferred} which fires when the process exits.
    @rtype: L{Deferred}
    """
    log_message("Spawning... " + repr(args))
    w = Waiter()
    reactor.spawnProcess(w, args[0], args, env, childFDs={
        0: 0, 1: 1, 2: 2,
    })
    return w.wait()



class ControlChannelFactory(Factory):

    def __init__(self, xpg):
        self.xpg = xpg


    def buildProtocol(self, addr):
        return ControlServer(self.xpg)



class XPostgres(object):

    statvfs = staticmethod(os.statvfs)

    def __init__(self, reactor):
        self.reactor = reactor
        self.socket_directory = DEFAULT_SOCKET_DIR
        self.socket_permissions_octal = '0600'
        self.postgres_port = 5432
        self.plist_path = None
        self.data_directory = None
        self.log_directory = None
        self.archive_log_directory = None
        self.restore_before_run = False
        self.running_postgres = None
        self.refcount = 1
        self.control_socket_lock = None
        self.doing_restore = False
        self.receivexlog_process = None
        self.backup_in_progress = False
        self.reactor.addSystemEventTrigger("before", "shutdown",
                                           self._reactor_shutdown)
        self.shutdown_hooks = set()
        self.post_shutdown_hooks = set()


    def add_post_shutdown_hook(self, hook):
        self.post_shutdown_hooks.add(hook)
        return hook


    def add_shutdown_hook(self, hook):
        self.shutdown_hooks.add(hook)
        return hook


    def remove_shutdown_hook(self, hook):
        self.shutdown_hooks.remove(hook)


    def _reactor_shutdown(self):
        """
        System event trigger for the reactor; do not call directly.
        """
        phase1 = DeferredList(map(maybeDeferred, self.shutdown_hooks))
        phase1.addCallback(lambda whatever:
                           DeferredList(map(maybeDeferred,
                                            self.post_shutdown_hooks)))
        return phase1


    def incref(self):
        """
        Respond to control message: increase the reference count of users of
        this database.
        """
        self.refcount += 1
        log_message("Incremented reference count. Count is now: {0}"
                    .format(self.refcount))


    def decrefFrom(self, client):
        """
        Respond to control message: decrease the reference count of users of
        this database, stopping this process if it reaches zero.

        @param client: the client connection that the decref request came from.
        @type client: L{ControlServer}
        """
        self.refcount -= 1
        log_message("Decremented reference count. Count is now: {0}"
                    .format(self.refcount))

        d = Deferred()
        if self.refcount == 0:
            log_message("Reference count reached zero.  Shutting down."
                        .format(self.refcount))

            def fireD():
                d.callback(None)
                return succeed(None)
            # Post-shutdown hooks to send response and then wait for the client
            # to disconnect,
            self.add_post_shutdown_hook(fireD)
            self.add_post_shutdown_hook(client.waitForClose)
            # System event trigger will take care of shutting down postgres
            # cleanly / un-listening on socket for us.
            self.reactor.stop()
        else:
            d.callback(None)
        return d


    def restart(self):
        """
        Respond to control message: try to re-start postgres.

        Note that this does not yet support changing the command-line options
        to postgres.
        """
        if self.running_postgres is not None:
            self.running_postgres.transport.signalProcess("HUP")

    def _a_d_vfs_attr(self, attr):
        """
        Implementation of C{archive_disk_capacity_*}.
        """
        ssvfs = self.statvfs(self.archive_log_directory)
        block_size = ssvfs.f_frsize
        capacity_blocks = getattr(ssvfs, "f_" + attr)
        return (block_size * capacity_blocks) / GIGS


    def archive_disk_capacity_gigabytes(self):
        """
        What size is the entire disk storing the archive logs, in gigabytes?
        """
        return self._a_d_vfs_attr("blocks")


    def archive_log_bytes(self):
        """
        The size, in octets, of the contents of the archive directory.
        """
        return sum([fp.getsize() for fp in
                    FilePath(self.archive_log_directory).walk()
                    if fp.isfile()])


    def exclude_from_tm_backup(self, path):
        """
        Exclude the given path from a Time Machine backup.  Return a
        L{Deferred} that fires when said path has been excluded.
        """
        return getProcessOutputAndValue(
            TMUTIL, ["addexclusion", path], self.postgres_env,
            reactor=self.reactor
        )


    def sanitize_pid_file(self):
        # Perform any necessary actions with the postmaster PID lock file to mitigate any
        #   unwanted postgres behavior.
        pm_pid_path = os.path.join(self.data_directory, 'postmaster.pid')
        try:
            fp = FilePath(pm_pid_path)
            lockfile_lines = fp.getContent().split("\n")
        except:
            return

        matchobj = re.match(r"^\s*(\d+)\s*", lockfile_lines[0])
        if matchobj:
            old_pid = matchobj.group(1)
        else:
            log_message("Could not get PID from lock file")
            return

        process = Popen("ps -f -p " + old_pid, stdout=PIPE, shell=True)
        ps_lines = process.stdout.read().split('\n')
        postgres_found = False
        for line in ps_lines:
            matchobj = re.match(r"\s*\d+\s+%s\s+\d+\s+.*%s" % (old_pid, POSTGRES), line)
            if matchobj:
                postgres_found = True
                break
        if not postgres_found:
            log_message("Replacing PID in postmaster lock file because we know it is not a postmaster.")
            # Replacing the PID with launchd's PID of 1 will cause postgres to accept an EPERM during its
            #   "kill -0" and likely replace the postmaster lock file instead of assuming that there is a conflict
            #   with an unrelated process in case that process is owned by the same UID. (rdar://problem/21027328)
            lockfile_lines[0] = '1'
            log_message("Stripping shared memory address from postmaster lock file")
            # Snip the shared memory block out of the lockfile if no process is running
            # that matches the PID in the file.  Otherwise postgres will fail to start
            # if it wasn't shut down properly and another process is now using that memory
            # block.
            del lockfile_lines[5:]
            for i in range(0,5):
                lockfile_lines[i] += '\n'
            fp.setContent("".join(lockfile_lines))


    def sanitize_lock_file(self):
        # Perform any necessary actions with the socket lock file to mitigate any
        #   unwanted postgres behavior.
        pm_lock_path = os.path.join(self.socket_directory, ".s.PGSQL." + str(self.postgres_port) + ".lock")
        try:
            fp = FilePath(pm_lock_path)
            lockfile_lines = fp.getContent().split("\n")
        except:
            return

        matchobj = re.match(r"^\s*(\d+)\s*", lockfile_lines[0])
        if matchobj:
            old_pid = matchobj.group(1)
        else:
            log_message("Could not get PID from lock file")
            return

        process = Popen("ps -f -p " + old_pid, stdout=PIPE, shell=True)
        ps_lines = process.stdout.read().split('\n')
        postgres_found = False
        for line in ps_lines:
            matchobj = re.match(r"\s*\d+\s+%s\s+\d+\s+.*%s" % (old_pid, POSTGRES), line)
            if matchobj:
                postgres_found = True
                break
        if not postgres_found:
            log_message("Replacing PID in socket lock file because we know it is not a postmaster.")
            # Replacing the PID with launchd's PID of 1 will cause postgres to accept an EPERM during its
            #   "kill -0" and likely replace the postmaster lock file instead of assuming that there is a conflict
            #   with an unrelated process in case that process is owned by the same UID. (rdar://problem/21027328)
            lockfile_lines[0] = '1'
            fp.setContent("\n".join(lockfile_lines))


    def touch_dotfile(self):
        if os.path.exists(self._dotfile()):
            now = self.reactor.seconds()
            os.utime(self._dotfile(), (now, now))
            return succeed(None)
        else:
            open(self._dotfile(), "wb").close()
            return self.exclude_from_tm_backup(self._dotfile())


    def lock_control_socket(self):
        if not self.control_socket_lock.lock():
            log_message("Unable to lock control file.")
            return False
        return True


    def unlock_control_socket(self):
        if not self.control_socket_lock:
            log_message("control_socket cannot be unlocked.")
            return False
        if not self.control_socket_lock.locked:
            log_message("File is not locked!")
            return False
        try:
            self.control_socket_lock.unlock()
        except:
            log_message("Unable to unlock control file.")
            return False

        return True


    def parse_command_line(self, argv, env):
        postgres_argv = []
        i = iter(argv)
        i.next() # Skip executable.
        def following():
            value = i.next()
            more.append(value)
            return value
        for value in i:
            include = True
            more = [value]
            if value == "-k":
                self.socket_directory = following()
            elif value in ("-a", "--apple-configuration"):
                include = False
                self.plist_path = following()
                argv.extend(readPlist(self.plist_path)["ProgramArguments"])
            elif value == "-D":
                self.data_directory = following()
            elif value == "-p":
                self.postgres_port = int(following())
            elif value == "-c":
                override_val = following()
                key, value = override_val.split("=", 1)
                if key == "unix_socket_directories":
                    self.socket_directory = value
                if key == "unix_socket_permissions":
                    self.socket_permissions_octal = value
                elif key == "unix_socket_directory":
                    # PostgreSQL 9.3 changed the parameter name.
                    log_message("WARNING: parameter \"unix_socket_directory\" was " +
                            "specified but is no longer valid. Using the specified " +
                            "value for now, but please use \"unix_socket_directories\" " +
                            "in the future.")
                    self.socket_directory = value
                elif key == "log_directory":
                    self.log_directory = value
            if include:
                postgres_argv.extend(more)
        pg_data = env.get("PGDATA")
        if pg_data is not None:
            self.data_directory = pg_data
        pg_port = env.get("PGPORT")
        if pg_port is not None:
            self.postgres_port = int(pg_port)

        if self.data_directory is None:
            raise NoDataDirectory()

        self.archive_log_directory = os.path.join(
            os.path.dirname(self.data_directory), ARCHIVE_LOG_DIRECTORY_NAME
        )
        self.postgres_argv = postgres_argv
        self.postgres_env = env
        self.control_socket_path = os.path.join(self.socket_directory,
                                                XPG_SOCKET_NAME)
        self.control_socket_lock = InheritableFilesystemLock(
            lock_path(self.control_socket_path), self.reactor
        )


    def _dotfile(self):
        return os.path.join(self.data_directory, RESTORE_ON_ABSENCE_FILE)


    def preflight(self):
        if not os.path.isdir(self.socket_directory):
            socket_directory_perms = self.socket_permissions_octal.lstrip('0')
            if socket_directory_perms[0] is not "7":
                # at least ensure that we have owner R/W/X bits set
                log_debug("Given unix_socket_permissions are not ok for parent directory")
                socket_directory_perms = "700"
            os.mkdir(self.socket_directory, int(socket_directory_perms))
        if not os.path.exists(self.archive_log_directory):
            os.mkdir(self.archive_log_directory, 0o700)
        elif not os.path.exists(self._dotfile()):
            if self.backup_zip_file.exists():
                self.restore_before_run = True
        self.prune_useless_archive_logs()


    def prune_useless_archive_logs(self):
        """
        Some logs are not useful, and may in fact be harmful if left around:

            - C{.partial} logs which have a completed, non partial copy

            - C{.in-progress} files left over from a crash

        Delete these files if found in the backup archive log directory, before
        either starting the log receiver or attempting to do a restore.
        """
        fp = FilePath(self.archive_log_directory)
        for child in fp.children():
            name = child.basename()
            segments = name.split('.')
            if len(segments) > 1:
                extension = segments[-1]
                if extension == 'partial':
                    complete = child.sibling('.'.join(segments[:-1]))
                    if complete.exists():
                        log_message(
                            "Removing {partial} because {complete} exists."
                            .format(partial=child.path,
                                    complete=complete.path)
                        )
                        child.remove()
                elif extension == TEMP_EXT[1:]:
                    log_message(
                        "Removing incomplete log file {partial}."
                        .format(partial=child.path)
                    )
                    child.remove()


    @inlineCallbacks
    def start_postgres(self, for_restore=False):
        """
        Execute the postgres binary with the requisite configuration.

        @param for_restore: Configure the spawned postgres instance for doing a
            restore rather than normal multi-user database operation.
        @type for_restore: L{bool}

        @return: a L{Deferred} that fires as soon as the subprocess is 'ready';
            which is to say, as soon as the postgres instance's socket path has
            been created, or fires immediately as soon as we've started the
            subprocess in the case of a restore.
        @rtype: L{Deferred} L{None}
        """
        log_message("Waiting for data directory: {data_directory}"
                    .format(data_directory=self.data_directory))
        yield self.wait_for_mount(FilePath(self.data_directory))
        log_message("Data directory exists.")
        xpg = self
        class PostgresProtocol(Waiter):
            def errReceived(self, data):
                sys.stderr.write(data)
                sys.stderr.flush()
            def processExited(self, reason):
                xpg.running_postgres = None
                log_message("Postgres exited.")
                if not xpg.in_stop_trigger and not for_restore:
                    xpg.reactor.stop()
                super(PostgresProtocol, self).processExited(reason)
                # pg_receivexlog exits normally under some conditions, but
                # sometimes does not reach a "stop point" (which is fairly
                # lightly documented) and attempts to reconnect to postgres
                # forever.  Tell it to stop, once postgres has stopped, since
                # we wait for it to exit when we exit.
                if xpg.receivexlog_process is not None:
                    xpg.receivexlog_process.stopIt()

        def do_stop():
            self.in_stop_trigger = True
            return self.stop_postgres()
        self.in_stop_trigger = False
        hook = self.add_shutdown_hook(do_stop)
        self.running_postgres = PostgresProtocol()
        def remove_trigger(nothing):
            if not self.in_stop_trigger:
                self.remove_shutdown_hook(hook)
        self.wait_for_postgres_shutdown().addCallback(remove_trigger)
        if for_restore:
            # Spawn postgres with an alternate socket directory so that
            # pg_ctl/PQPing cannot access the socket
            log_message("Spawning postgres for restore only.")
            # use os.path.join rather than FilePath to avoid abspath()ing the
            # command line argument
            temp_socket_dir = os.path.join(self.socket_directory,
                                           "restore_only")
            if not os.path.isdir(temp_socket_dir):
                os.mkdir(temp_socket_dir, 0o700)
            spawn_argv = ['-D', self.data_directory, '-k', temp_socket_dir,
                          '-c', "listen_addresses= "]
            if self.log_directory is not None:
                spawn_argv.extend([
                    '-c', "log_directory=" + self.log_directory,
                    '-c', "log_filename=postgresql_recovery_"
                    + str(os.getpid()) + ".log",
                    '-c', "logging_collector=on"
                ])
        else:
            spawn_argv = self.postgres_argv

        log_message("Spawning postgres now.")
        self.reactor.spawnProcess(
            self.running_postgres,
            POSTGRES, [POSTGRES] + spawn_argv,
            self.postgres_env
        )

        if not for_restore:
            sktdir = FilePath(self.socket_directory)
            log_message(("Waiting for socket to appear in socket directory: "
                         "{socket_directory}")
                        .format(socket_directory=self.socket_directory))
            yield self.wait_for_path(sktdir.child(".s.PGSQL.5432"))
            log_message("Socket available; starting should now be complete.")

        self.touch_dotfile()


    def wait_for_postgres_shutdown(self, timeout=None):
        if self.running_postgres is None:
            return succeed(None)
        a = self.running_postgres.wait()
        if timeout is None:
            b = Deferred() # never fires.
        else:
            b = deferLater(self.reactor, timeout, lambda: None)
        return DeferredList([a, b], fireOnOneCallback=True)


    @inlineCallbacks
    def stop_postgres(self, signal='TERM'):
        if self.running_postgres is None:
            # Postgres already stopped.
            return
        sql = (
            'SELECT pid, (SELECT pg_terminate_backend(pid)) as killed from '
            "pg_stat_activity WHERE state LIKE 'idle';"
        )
        if not self.doing_restore:
            # Connection restrictions will prevent this from working; don't
            # bother.
            log_message("Killing idle connections...")
            yield self.spawn(PSQL, '-q', '-h', self.socket_directory, '-d',
                             'postgres', '-c', sql)
        self.running_postgres.transport.signalProcess(signal)
        yield self.wait_for_postgres_shutdown(50)
        if self.running_postgres is not None:
            self.running_postgres.transport.signalProcess('INT')
            yield self.wait_for_postgres_shutdown(2)


    def wait_for_mount(self, fp):
        """
        Wait for a directory to be mounted, using 'wait4path'.
        """
        return getProcessOutputAndValue(
            WAIT4PATH, [fp.path], self.postgres_env, reactor=self.reactor
        )


    def wait_for_contents(self, fp, iterations=30, ignore=()):
        counter = itertools.count()
        def check_for_contents():
            fp.changed()
            contents = [c for c in fp.children() if not c.basename() in ignore]
            print("CONTENTS:", contents)
            if fp.isdir() and len(contents):
                l.stop()
                return
            if counter.next() > iterations:
                raise NoFiles()
        l = LoopingCall(check_for_contents)
        l.clock = self.reactor
        return l.start(1)


    def wait_for_path(self, fp):
        """
        Wait for a path to exist, with a 1hz timer.
        """
        def check_for_path():
            fp.changed()
            if fp.exists():
                lc.stop()
        lc = LoopingCall(check_for_path)
        lc.clock = self.reactor
        return lc.start(1.0)


    @property
    def backup_zip_file(self):
        return (
            FilePath(self.archive_log_directory)
            .child(BACKUP_DIRECTORY_NAME)
            .child(BACKUP_ZIP_FILE_NAME)
        )


    @property
    def backup_file_parent_dir(self):
        return (
            FilePath(self.archive_log_directory)
            .child(BACKUP_DIRECTORY_NAME)
        )


    @property
    def backup_temp_file(self):
        return (
            FilePath(self.archive_log_directory)
            .child(BACKUP_DIRECTORY_NAME)
            .child(BACKUP_TEMP_FILE_NAME)
        )


    def do_not_backup_file(self):
        return (
            FilePath(self.archive_log_directory)
            .child(BACKUP_DIRECTORY_NAME)
            .child(DO_NOT_BACKUP_FILE)
        )


    def should_backup(self):
        """
        Is it time to do the backup?
        """
        if self.backup_in_progress is True:
            log_debug("Backup already in progress...")
            return False
        backup_zip_file = self.backup_zip_file
        if self.backup_zip_file.exists():
            time_since_change = (self.reactor.seconds() -
                                 backup_zip_file.getModificationTime())
            if time_since_change < MIN_BACKUP_THRESHOLD_SECS:
                # It's too soon, regardless of disk parameters.
                return False
        archive_log_gigs = self.archive_log_bytes() / GIGS
        if archive_log_gigs >= MAX_WAL_FILES_GIGS:
            # Logs are too big for this size disk.  Please backup immediately.
            log_debug("Backing up due to size of WAL archive")
            return True
        temp_do_not_backup_file = self.do_not_backup_file()
        if temp_do_not_backup_file.exists():
            # Allow for previous tests to force a backup, but otherwise do not
            # backup if the application has created this file.
            return False
        if not self.backup_zip_file.exists():
            # Likely first opportunity to backup, so do it now
            log_debug("Backing up because there is no existing backup file")
            return True
        # No reason to back up yet: no size thresholds reached.  Don't bother
        # backing up.
        return False


    def wait_for_backup_completion(self):
        while self.backup_in_progress:
            # Prevent pg_basebackup from trying to connect once postgres is shut down
            if self.running_postgres is None:
                log_debug("postgres is shutting down, don't keep trying to backup")
                return
            log_message("Waiting for for backup to finish...")
            self.reactor.iterate(1)
            log_debug("Finished self.reactor.iterate()")


    @inlineCallbacks
    def do_backup(self):
        # Make sure any active log files get backed up by Time Machine.
        for f in FilePath(self.archive_log_directory).children():
            matchobj = re.match(r"^(.*)\.partial$", f.basename())
            if matchobj:
                f.touch()

        # If a backup is needed, create it.
        if not self.should_backup():
            return
        temp_backup_file = self.backup_temp_file
        if temp_backup_file.exists():
            # Test.
            temp_backup_file.remove()

        # Make a list of WAL files to delete after backup succeeds.

        # XXX Instead of basing the list on file creation times, it would be
        # best to untar the base backup, check the backup_label file, and base
        # deletion on the START WAL LOCATION.  However, this could use a lot of
        # space.
        file_create_times = []
        for f in FilePath(self.archive_log_directory).children():
            f.restat()
            file_create_times.append((f.statinfo.st_birthtime, f))
        file_create_times.sort(key=lambda (t, f): t)

        temp_backup_dir = self.backup_file_parent_dir
        if not temp_backup_dir.exists():
            os.mkdir(self.backup_file_parent_dir.path, 0o700)

        fd = os.open(temp_backup_file.path, os.O_WRONLY | os.O_CREAT |
                     getattr(os, "O_BINARY", 0))
        try:
            log_message("Beginning base backup.")
            self.backup_in_progress = True
            while True:
                waiter = Waiter()
                self.reactor.spawnProcess(
                    waiter, PG_BASEBACKUP,
                    [PG_BASEBACKUP, '-Ft', '-z', '-h', self.socket_directory,
                     '-D', "-"],
                    env=self.postgres_env, childFDs={0: 0, 1: fd, 2: 2})
                status = yield waiter.wait()
                if status == 0:
                    break
                log_message(
                    "Base backup did not complete. Trying again in 2 seconds."
                )
                if self.refcount <= 0:
                    log_message("Aborting backup attempt, postgres should be shutting down")
                    self.backup_in_progress = False
                    os.close(fd)
                    os.remove(temp_backup_file.path)
                    return
                os.lseek(fd, 0, 0)
                os.ftruncate(fd, 0)
                yield deferLater(self.reactor, 2.0, lambda: None)
            log_message("Completing base backup...")
            import fcntl
            fcntl.fcntl(fd, fcntl.F_FULLFSYNC)
            log_message("Base backup synced.")
        finally:
            os.close(fd)
        log_message("Base backup closed.")
        os.rename(temp_backup_file.path, self.backup_zip_file.path)
        log_message("Base backup renamed.")

        # Clean up all of the log files that existed before backup, except for
        # the last few in case they are needed later. Sort matched files by
        # file creation time in ascending order
        for bt, fp in file_create_times[:-MAINTAINED_LOG_COUNT]:
            fp.remove()
        self.backup_in_progress = False


    def spawn(self, *a):
        return simple_spawn(self.reactor, a, self.postgres_env)


    @inlineCallbacks
    def do_restore(self):
        self.doing_restore = True
        self.unpartialize()
        backup_zip_file = self.backup_zip_file
        if not backup_zip_file.exists():
            raise OSError("no backup file")
        datadir = FilePath(self.data_directory)
        if datadir.exists():
            previous = backup_zip_file.sibling(backup_zip_file.basename()
                                               + '.previous')
            if previous.exists():
                previous.remove()
            datadir.moveTo(previous)
        os.mkdir(datadir.path, 0o700)
        yield self.spawn(TAR, '-xz', '-f', backup_zip_file.path, '-C',
                         datadir.path)
        recovery_done = datadir.child("recovery.done")
        if recovery_done.exists():
            recovery_done.remove()
        dotfile = FilePath(self._dotfile())
        dotfile.remove()
        with datadir.child("recovery.conf").open("wb") as f:
            f.write(
                "restore_command = '/bin/cp ../backup/%f %p'"
            )
        self.toggle_wal_archive_logging(False)

        # Scan over the archive files to see if anything looks problematic.
        #  Specifically, check for WAL archive files that don't match our expected size.
        #  WAL files should always be a fixed size, 16M by default.
        for f in FilePath(self.archive_log_directory).children():
            matchobj = re.match(r"^[0-9A-Z]+$", f.basename())
            if matchobj and not f.isdir():
                if f.getsize() != WAL_FILE_SIZE_BYTES:
                    log_message("Warning: During restore, found supposed WAL file"
                            " which does not meet the expected size.  Renaming"
                            " the file to prevent probable error in recovery.")
                    log_message("Renaming " + str(f))
                    potentially_bad_file = f.siblingExtension(
                            '.xpg_renamed_due_to_abnormal_filesize'
                    )
                    f.moveTo(potentially_bad_file)
        # If an upgrade is needed, let upgrade finish the recovery.
        did_upgrade = self.upgrade_cluster_if_needed()
        if did_upgrade:
            log_message("Recovery done; It should have been accomplished via upgrade.")
            return

        yield self.start_postgres(for_restore=True)
        log_message("Waiting for recovery done notification.")
        yield self.wait_for_path(recovery_done)
        log_message("Recovery complete.")
        self.stop_postgres(signal='INT')
        yield self.wait_for_postgres_shutdown(10000)
        self.doing_restore = False


    def unpartialize(self):
        """
        When pg_receivexlog writes a partial log file, it gives it a '.partial'
        extension.  Normally you want to leave it that way, for consistency's
        sake, so that pg_receivexlog can be sensibly restarted.  However,
        immediately before a recovery, you want to recover the data in a
        .partial file, so it has to be renamed to consider it complete.
        """
        partials = (FilePath(self.archive_log_directory)
                    .globChildren('*.partial'))

        for partial in partials:
            complete = partial.sibling(
                '.'.join(partial.basename().split('.')[:-1])
            )
            if not complete.exists():
                log_message("Moved partial {partial!r} to {complete!r}"
                            .format(partial=partial.path,
                                    complete=complete.path))
                partial.moveTo(complete)


    @inlineCallbacks
    def do_everything(self, argv, environ):
        self.parse_command_line(argv, environ)
        self.preflight()

        args = list(argv)
        args.pop(0)
        setproctitle("xpostgres {}".format(' '.join(args)))

        parent_pid = os.getppid()
        # FIXME: This should be moved to or duplicated in the xpg_ctl client case
        log_message("Process parent is PID " + str(parent_pid))

        for x in range(10):
            if self.lock_control_socket():
                break
            elif x < 9:
                log_message("Could not lock control socket, retrying")
            else:
                log_message("Could not lock control socket, aborting startup")
                return

        if os.path.exists(self.control_socket_path):
            log_message("Locked control socket, but stale socket present. "
                        "Cleaning up.")
            os.remove(self.control_socket_path)

        # Note: "mode" parameter is deprecated.  The socket should instead
        # be placed inside a secure directory.
        control_channel = UNIXServerEndpoint(
            self.reactor, self.control_socket_path, mode=0660
        )
        listener = yield control_channel.listen(ControlChannelFactory(self))

        self.add_shutdown_hook(self.wait_for_backup_completion)
        self.add_shutdown_hook(self.unlock_control_socket)
        self.add_shutdown_hook(listener.stopListening)

        if self.restore_before_run:
            log_message("Doing restore.")
            yield self.do_restore()
        else:
            DEVNULL = open(os.devnull, 'w')
            if call([PG_CONTROLDATA, self.data_directory], stdout=DEVNULL) != 0:
                log_message("Error: pg_controldata returned error while checking checking cluster at: "
                        + self.data_directory)
                returnValue(1)
            DEVNULL.close()

            data_directory_fp = FilePath(self.data_directory)
            pg_version_fp = data_directory_fp.child('PG_VERSION')
            if not os.path.exists(pg_version_fp.path):
                log_message("Error: PG_VERSION file is missing from cluster: {}".format(data_directory_fp.path))
                log_message("Move or delete this directory to provision a new cluster.")
                returnValue(1)

        self.upgrade_cluster_if_needed()

        # Make sure we successfully exclude the data directory _before_
        # starting postgres, so we don't store any data that can't be backed up
        log_message("Excluding data directory.")
        yield self.exclude_from_tm_backup(self.data_directory)
        log_message("Turning on archive logging.")
        self.toggle_wal_archive_logging(True)
        log_message("Cleaning up any existing postmaster.pid file")
        self.sanitize_pid_file()
        log_message("Cleaning up any existing socket lock file")
        self.sanitize_lock_file()
        log_message("Starting postgres.")
        yield self.start_postgres(for_restore=False)
        yield self.start_receivexlog()
        lc = LoopingCall(self.do_backup)
        lc.clock = self.reactor
        log_message("Starting backup heartbeat.")
        yield lc.start(HEARTBEAT_SECS)
        log_message("Heartbeat successfully terminated.")
        log_message("Finished.")
        returnValue(0)


    def start_receivexlog(self):
        """
        Kick off pg_receivexlog process.
        """
        started = Deferred()
        stopped = Deferred()
        class WaitForIt(ProcessProtocol):
            done = False
            def errReceived(self, data):
                log_message("log receiver: " + data.rstrip())
                if 'starting log streaming' in data:
                    started.callback(None)

            def processExited(self, reason):
                self.done = True
                stopped.callback(None)

            def stopIt(self):
                if not self.done:
                    log_message("pg_receivexlog still running; terminating.")
                    self.transport.signalProcess('INT')
                else:
                    log_message("pg_receivexlog exited normally.")

        env = self.postgres_env.copy()
        # pg_receivexlog presents no programmatic interface to discover that it
        # has started. The only indication that it has done so is that it emits
        # a localized message, and only in verbose mode.  However, it is
        # absolutely critical that we receive this notification, because a
        # basebackup created before an xlog receiver has started cannot be
        # restored.  So, force the locale to print a log message we can parse.
        env['LANG'] = 'C'
        self.add_shutdown_hook(lambda: stopped)
        self.receivexlog_process = WaitForIt()
        self.reactor.spawnProcess(
            self.receivexlog_process,
            PG_RECEIVEXLOG,
            [
                PG_RECEIVEXLOG, "-h", self.socket_directory, '--no-password',
                "--directory", self.archive_log_directory,
                '--verbose', '--status-interval=1'
            ],
            env
        )
        return started


    def enable_wal_archive_logging(self, postgres_config, hba_config):
        """
        Configure postgres for WAL archiving and allow local replication
        connections.
        """
        new_postgres_config = []
        for line in postgres_config:
            matchobj = re.match(r"^\s*#archive_mode\s*=\s*\S*(.*)", line)
            if (matchobj):
                new_postgres_config.append(
                    "archive_mode = on" + matchobj.group(1) + "\n"
                )
                continue
            matchobj = re.match(r"^\s*#archive_timeout\s*=\s*\d+(.*)", line)
            if (matchobj):
                new_postgres_config.append(
                    "archive_timeout = " + ARCHIVE_TIMEOUT + matchobj.group(1)
                    + "\n"
                )
                continue
            matchobj = re.match(r"^\s*#max_wal_senders\s*=\s*\d+(.*)", line)
            if (matchobj):
                new_postgres_config.append(
                    "max_wal_senders = " + MAX_WAL_SENDERS + matchobj.group(1)
                    + "\n"
                )
                continue
            matchobj = re.match(r"^\s*#wal_level\s*=\s*\S*(.*)", line)
            if (matchobj):
                new_postgres_config.append(
                    "wal_level = hot_standby" + matchobj.group(1) + "\n"
                )
                continue
            matchobj = re.match(
                r"\s*#*archive_command\s*=\s*['\"].*['\"](.*)", line
            )
            if (matchobj):
                command = ("'/Applications/Server.app/Contents/ServerRoot/Library/CalendarServer/bin/python" +
                            " {this} archive %p ../backup/%f'".format(this=os.path.abspath(__file__)))
                new_postgres_config.append(
                    "archive_command = " + command + matchobj.group(1) + "\n"
                )
                continue
            new_postgres_config.append(line + "\n")

        replication_enabled = False
        new_hba_config = []
        for line in hba_config:
            new_hba_config.append(line + "\n")
            matchobj = re.match(r"^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*(\S*)$",
                                line)
            if (matchobj):
                (type, database, user) = matchobj.group(1, 2, 3)
                if matchobj.group(5):
                    (address, method) = matchobj.group(4, 5)
                else:
                    address = ""
                    method = matchobj.group(4)

                if ((type == "local" and database == "replication" and
                     address == "" and user == "all" and method == "trust")):
                    replication_enabled = True
        if not replication_enabled:
            #FIXME: This has been showing up multiple redundant times in pg_hba.conf:
            new_hba_config.append(
                "local   replication     all"
                "                                      trust\n"
            )

        return new_postgres_config, new_hba_config


    def disable_wal_archive_logging(self, postgres_config):
        """
        Revert to the defaults for postgresql.conf settings that we use to
        enable archive logging.
        """
        new_postgres_config = []
        for line in postgres_config:
            matchobj = re.match(r"^\s*archive_mode\s*=\s*\S*(.*)", line)
            if (matchobj):
                new_postgres_config.append(
                    "#archive_mode = off" + matchobj.group(1) + "\n"
                )
                continue
            matchobj = re.match(r"^\s*archive_timeout\s*=\s*\d+(.*)", line)
            if (matchobj):
                new_postgres_config.append(
                    "#archive_timeout = 0" + matchobj.group(1) + "\n"
                )
                continue
            matchobj = re.match(r"^\s*max_wal_senders\s*=\s*\d+(.*)", line)
            if (matchobj):
                new_postgres_config.append("#max_wal_senders = 0" +
                                           matchobj.group(1) + "\n")
                continue
            matchobj = re.match(r"^\s*wal_level\s*=\s*\S*(.*)", line)
            if (matchobj):
                new_postgres_config.append("#wal_level = minimal" +
                                           matchobj.group(1) + "\n")
                continue
            matchobj = re.match(r"^\s*archive_command\s*=\s*['\"].*['\"](.*)",
                                line)
            if (matchobj):
                new_postgres_config.append("#archive_command = \'\'" +
                                           matchobj.group(1) + "\n")
                continue
            new_postgres_config.append(line + "\n")

        return new_postgres_config


    def wal_archiving_is_enabled(self, postgres_config):
        """
        Is WAL archiving enabled?  If any of our expected settings aren't
        configured, return False.
        """
        patterns = [
            r"^\s*#archive_mode\s*=\s*.*",
            r"^\s*#archive_command\s*=\s*['\"].*['\"].*",
            r"^\s*#max_wal_senders\s*=\s*\d+.*",
            r"^\s*#wal_level\s*=\s*\S+.*",
            r"^\s*#archive_timeout\s*=\s*\d+.*"
        ]

        for line in postgres_config:
            for p in patterns:
                pattern = re.compile(p, re.MULTILINE)
                if pattern.match(line) is not None:
                    return False
        return True


    def toggle_wal_archive_logging(self, set_enabled):
        """
        Enable or disable WAL archive logging and update related preferences.
        """
        postgres_config_path = os.path.join(self.data_directory,
                                            "postgresql.conf")
        hba_config_path = os.path.join(self.data_directory, "pg_hba.conf")
        postgres_fp = FilePath(postgres_config_path)
        postgres_config = postgres_fp.getContent().split("\n")
        hba_config_fp = FilePath(hba_config_path)
        hba_config = hba_config_fp.getContent().split("\n")

        if set_enabled:
            (postgres_config, hba_config) = self.enable_wal_archive_logging(
                postgres_config, hba_config
            )
            postgres_fp.setContent("".join(postgres_config))
            hba_config_fp.setContent("".join(hba_config))
        else:
            postgres_config = self.disable_wal_archive_logging(postgres_config)
            postgres_fp.setContent("".join(postgres_config))


    def enable_connection_restriction(self):
        """
        Disable any non-replication line in pg_hba.conf.
        """
        hba_config_path = os.path.join(self.data_directory, "pg_hba.conf")
        hba_config_fp = FilePath(hba_config_path)
        hba_config = hba_config_fp.getContent().split("\n")

        new_hba_config = []
        updated_config = False
        for line in hba_config:
            matchobj = re.match(r"^#", line)
            if (matchobj):
                new_hba_config.append(line + "\n")
                continue
            matchobj = re.match(r"^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*(\S*)$",
                                line)
            if (matchobj):
                (type, database, user) = matchobj.group(1, 2, 3)
                if matchobj.group(5):
                    (address, method) = matchobj.group(4, 5)
                # else:
                    # method = matchobj.group(4)
                if database == "replication":
                    new_hba_config.append(line + "\n")
                    continue
                new_hba_config.append("#" + line +
                                      "    # UPDATED BY xpostgres\n")
                updated_config = True
            else:
                new_hba_config.append(line + "\n")
        if updated_config:
            hba_config_fp.setContent("".join(new_hba_config))


    def disable_connection_restriction(self):
        """
        Enable any lines in pg_hba.conf that we previously disabled
        """
        hba_config_path = os.path.join(self.data_directory, "pg_hba.conf")
        hba_config_fp = FilePath(hba_config_path)
        hba_config = hba_config_fp.getContent().split("\n")

        new_hba_config = []
        updated_config = False
        for line in hba_config:
            matchobj = re.match(r"#(\s*.+)\s*# UPDATED BY xpostgres", line)
            if (matchobj):
                new_hba_config.append(matchobj.group(1) + "\n")
                updated_config = True
                continue
            else:
                new_hba_config.append(line + "\n")

        if updated_config:
            hba_config_fp.setContent("".join(new_hba_config))


    def upgrade_cluster_if_needed(self):
        """
        If the target cluster is outdated, try to upgrade it.
        Return value: True if an upgrade was completed, False if not.
        """
        if not os.path.exists(self.data_directory):
            log_debug(
                "upgrade_cluster_if_needed: data directory does not exist currently; nothing to upgrade"
            )
            return False

        data_directory_fp = FilePath(self.data_directory)
        pg_version_fp = data_directory_fp.child('PG_VERSION')
        if not os.path.exists(pg_version_fp.path):
            log_message("Error: PG_VERSION file is missing from cluster: {}".format(data_directory_fp.path))
            log_message("Move or delete this directory to provision a new cluster.")
            return False

        with pg_version_fp.open("rb") as content_file:
            content = content_file.read()

        matchobj = re.match(r"^9\.(\d+)$", content)
        if not matchobj:
            log_message("Error: Could not read version from specified data directory PG_VERSION file")
            return False
        else:
            postgres_minor_version = matchobj.group(1)

        old_bin_dir = None
        old_socket_directory_argument = "unix_socket_directories"  # postgres 9.3+
        # Warning: support for upgrade from server versions containing postgres 9.0 and 9.1
        #   has not been tested as it should be handled by other tools currently.
        if postgres_minor_version   == '0':
            old_bin_dir = POSTGRES_BINARIES_DIR_9_0
            old_socket_directory_argument = "unix_socket_directory"
        elif postgres_minor_version == '1':
            old_bin_dir = POSTGRES_BINARIES_DIR_9_1
            old_socket_directory_argument = "unix_socket_directory"
        elif postgres_minor_version == '2':
            old_bin_dir = POSTGRES_BINARIES_DIR_9_2
            old_socket_directory_argument = "unix_socket_directory"
        elif postgres_minor_version == '3':
            log_debug("Target cluster does not appear to require an upgrade.")
            return False
        else:
            log_message("Error: Could not find a supported version string")
            return False

        log_message("Preparing to upgrade outdated database cluster.")

        # Find available ports to use
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', 0))
        first_port = s.getsockname()[1]
        s.close()
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', 0))
        second_port = s.getsockname()[1]
        s.close()

        # Disable archive logging since the backups will be re-created later
        # XXX: assumes old configuration is compatible
        self.toggle_wal_archive_logging(False)

        # Move aside the original cluster and initialize a new one
        orig_data_dir = data_directory_fp.siblingExtension('.original_for_upgrade').path
        if os.path.isfile(orig_data_dir):
            log_message("Error: temporary data dir already exists at " + str(orig_data_dir) +
                        ". Consider deleting it and reattempting the upgrade.")
            return False

        my_uid = os.getuid()
        superuser_role = getpwuid(my_uid)[0]
        log_debug("my username is " + superuser_role)
        cal_pid = getpwnam('_calendar').pw_uid
        if my_uid == cal_pid:
            # special case for Calendar as its user account differs from the superuser role
            superuser_role = 'caldav'
            os.rename(self.data_directory, orig_data_dir)
            os.mkdir(self.data_directory, 0o700)
            log_message("Creating database with caldav superuser role...")
            if call([INITDB, '--encoding=UTF8', '--locale=C', '-U', superuser_role, '-D', self.data_directory]) != 0:
                log_message("Error running initdb.")
                return False
        else:
            os.rename(self.data_directory, orig_data_dir)
            os.mkdir(self.data_directory, 0o700)
            log_message("Creating database...")
            if call([INITDB, '--encoding=UTF8', '--locale=C', '-D', self.data_directory]) != 0:
                log_message("Error running initdb.")
                return False

        # So that pg_ctl won't think postgres is ready, do our upgrade in a different location,
        #   and create a placeholder lock file in the original data dir which causes pg_ctl to return
        #   a "not running" status.
        #   The xpg lock should keep other xpg instances from interfering.
        temp_target_db_dir = data_directory_fp.siblingExtension('.new_for_upgrade').path
        if os.path.isfile(temp_target_db_dir):
            log_message("Error: temporary data dir already exists at " + str(temp_target_db_dir) +
                        ". Consider deleting it and reattempting the upgrade.")
            return False

        try:
            shutil.copytree(self.data_directory, temp_target_db_dir)
        except OSError as exc:
            if exc.errno == errno.ENOTDIR:
                shutil.copy(self.data_directory, temp_target_db_dir)
            else: raise

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', 0))
        unused_port = s.getsockname()[1]
        s.close()

        data_dir = FilePath(self.data_directory)
        with data_dir.child("postmaster.pid").open("wb") as f:
            # launchd is always running but the unused port results in the desired pg_ctl status result
            f.write("1\n" +
                self.data_directory + "\n" +
                "1402713703\n" +
                str(unused_port) + "\n" +
                self.socket_directory + "\n" +
            "localhost\n")

        log_message("Upgrading cluster from 9." + postgres_minor_version + " ...")
        pg_ctl_old = os.path.join(old_bin_dir, "pg_ctl")

        log_message("Starting original postgres cluster using " + pg_ctl_old)
        # Note: Specifying "-k" or "unix_socket_directory" when using the 9.0 binaries
        # results in pg_ctl not using the correct directory.
        # PGHOST must be redefined as it will override our settings if set by the owner.
        orig_pghost = os.environ.pop("PGHOST", "")
        substituted_pghost_env_value = False
        if orig_pghost != "":
            log_debug("Temporarily overriding PGHOST in environment...")
            os.environ['PGHOST'] = DEFAULT_SOCKET_DIR
            substituted_pghost_env_value = True

        if call([pg_ctl_old, 'start',
                        '-w',
                        '-t', '86400',    # If recovery from backup occurs, this may take a while.
                                          # Recovery should complete before proceeding.
                        '-l', os.path.join(DEFAULT_SOCKET_DIR, superuser_role + "_server1-upgrade.log"),
                        '-D', orig_data_dir,
                        '-o', "-p %s -c %s='%s'" % (str(first_port),
                        old_socket_directory_argument, DEFAULT_SOCKET_DIR)]) != 0:
            log_message("pg_ctl start failed for original cluster")
            if substituted_pghost_env_value:
                s.environ['PGHOST'] = orig_pghost
            return False

        log_message("Starting new postgres cluster...")
        if call([PG_CTL, 'start',
                        '-w',
                        '-t', '60',
                        '-l', os.path.join(DEFAULT_SOCKET_DIR, superuser_role + "_server2-upgrade.log"),
                        '-D', temp_target_db_dir,
                        '-o', "-p %s -c unix_socket_directories='%s'" % (str(second_port),
                        DEFAULT_SOCKET_DIR)]) != 0:
            log_message("pg_ctl start failed for new cluster")
            if substituted_pghost_env_value:
                os.environ['PGHOST'] = orig_pghost
            return False

#        #if superuser_role == '_devicemgr':   # DEBUG
#        log_debug("Sleeping a while...")
#        import time
#        time.sleep(60)
#        log_debug("Done sleeping....")

        log_message("Doing dump and import...")
        process = Popen(PG_DUMPALL + ' -h ' + DEFAULT_SOCKET_DIR +
                    ' -p ' + str(first_port) + ' -c -U ' + superuser_role + ' -o ' +
                    ' | ' +
                    PSQL + ' postgres ' + ' -h ' + DEFAULT_SOCKET_DIR + ' -p ' +
                    str(second_port) + ' -U ' + superuser_role,
                stdout=PIPE, stderr=STDOUT, shell=True)
        output = process.communicate()[0]
        if process.wait() != 0:
            log_message("psql import failed: " + str(process.returncode))
            log_message("output :" + output)
            if substituted_pghost_env_value:
                os.environ['PGHOST'] = orig_pghost
            return False
        log_debug("output :" + output)
        log_message("Import was successful.")
        log_message("Shutting down original cluster.")
        if call([pg_ctl_old, 'stop', '-D', orig_data_dir]) != 0:
            log_message("pg_ctl stop failed for post-upgrade cluster (old)")
            if substituted_pghost_env_value:
                os.environ['PGHOST'] = orig_pghost
            return False

        log_message("Shutting down new cluster.")
        if call([PG_CTL, 'stop',
                        '-D', temp_target_db_dir]) != 0:
            log_message("pg_ctl stop failed for post-upgrade cluster (new)")
            if substituted_pghost_env_value:
                os.environ['PGHOST'] = orig_pghost
            return False

        shutil.rmtree(orig_data_dir)
        shutil.rmtree(self.data_directory)
        os.rename(temp_target_db_dir, self.data_directory)

        # Clean up old backup files and start backing up the new cluster
        backup_zip_file = self.backup_zip_file
        if self.backup_zip_file.exists():
            log_debug(
              "Removing obsolete backup archive after cluster upgrade at path: "
                    + backup_zip_file.path
            )
            backup_zip_file.remove()
        fp = FilePath(self.archive_log_directory)
        for child in fp.children():
            child.remove()
        if substituted_pghost_env_value:
            os.environ['PGHOST'] = orig_pghost
        return True


from twisted.protocols.amp import AMP, Command

class Incref(Command):
    """
    An additional client is using Postgres.  Increment the reference count.
    """



class Decref(Command):
    """
    One of the clients using Postgres has declared that it's not interested any
    more.  Decrement the reference count.
    """



class Restart(Command):
    """
    Ask PostgreSQL to re-load its configuration via SIGHUP.
    """



class ControlClient(AMP):
    def incref(self):
        return self.callRemote(Incref)


    def decref(self):
        return self.callRemote(Decref)


    def restart(self):
        return self.callRemote(Restart)



class ControlClientFactory(Factory):
    protocol = ControlClient



class ControlServer(AMP, object):
    """
    Thin wrapper providing AMP access to XPostgres's functions.
    """
    def __init__(self, xpostgres):
        super(ControlServer, self).__init__()
        self.xpostgres = xpostgres
        self.waiting = Deferred()


    @Incref.responder
    def incref(self):
        self.xpostgres.incref()
        return {}


    @Decref.responder
    def decref(self):
        return self.xpostgres.decrefFrom(self).addCallback(lambda ok: {})


    @Restart.responder
    def restart(self):
        self.xpostgres.restart()
        return {}


    def waitForClose(self):
        """
        Return a L{Deferred} that fires when this connection is closed.
        """
        return self.waiting


    def connectionLost(self, reason):
        super(ControlServer, self).connectionLost(reason)
        self.waiting.callback(None)



class XPGCtlCommand(object):
    def __init__(self, xpg_ctl):
        self.xpg_ctl = xpg_ctl


    def execute(self):
        """
        Do the work of this sub-command.  All subclasses must implement this.
        """



class CtlStart(XPGCtlCommand):
    """
    Implementation of C{xpg_ctl start}.

    If there's already an xpostgres server running, increment its reference
    count.
    """

    def actually_start_xpostgres(self):
        """
        It appears that xpostgres isn't running.  Try to start it (and, by
        extension, postgres).
        """
        # Use pg_ctl to actually start up the 'xpostgres' process so as to
        # compatibly handle e.g. logging.
        return CtlPassthrough(self.xpg_ctl, ["-p", XPOSTGRES]).execute()


    @inlineCallbacks
    def execute(self):
        # Serialize all 'start' and 'stop' invocations so that refcounting is
        # atomic.
        yield self.xpg_ctl.acquire_lock()
        xpg = self.xpg_ctl.xpostgres_object()

        for x in range(10):
            # If XPostgres is running, it should have acquired this lock.
            locked = xpg.control_socket_lock.lock()
            if locked:
                # XPostgres is not running.  Its socket is therefore not
                # listening.  Therefore, we must start it; first though,
                # bequeath this lock to the xpostgres process, so that no other
                # non-xpg_ctl xpostgres process can sneak in here.
                log_debug("Acquired socket lock; "
                          "relaying it to xpostgres process.")
                # Don't yield the bequeathal immediately; be sure to kick off
                # the xpostgres process before waiting for anything.
                send_lock = xpg.control_socket_lock.bequeath()

                # If recovery is needed, start postgres manually for that
                # purpose and later invoke pg_ctl to startup after recovery.
                if not os.path.exists(xpg._dotfile()):
                    if xpg.backup_zip_file.exists():
                        log_debug("Doing restore")
                        yield xpg.do_restore()
                else:
                    # Run some sanity checks so that we don't proceed if there is an obvious issue.
                    DEVNULL = open(os.devnull, 'w')
                    log_debug("running pg_controldata on directory: " + str(xpg.data_directory))
                    if call([PG_CONTROLDATA, xpg.data_directory], stdout=DEVNULL) != 0:
                        log_message("Error: pg_controldata returned error while checking checking cluster at: "
                                + xpg.data_directory)
                        returnValue(8)
                    DEVNULL.close()

                    data_directory_fp = FilePath(xpg.data_directory)
                    pg_version_fp = data_directory_fp.child('PG_VERSION')
                    if not os.path.exists(pg_version_fp.path):
                        log_message("Error: PG_VERSION file is missing from cluster: {}".format(data_directory_fp.path))
                        log_message("Move or delete this directory to provision a new cluster.")
                        returnValue(8)

                start_postgres = self.actually_start_xpostgres()
                yield send_lock
                yield start_postgres
                log_debug("Returned from start_postgres, exiting")
                returnValue(None)
            else:
                log_debug("Control socket _not_ locked; connecting client.")
                try:
                    client = yield self.xpg_ctl.control_client()
                except ConnectError:
                    log_debug("Control socket was not locked, "
                              "but we could not connect.  "
                              "Postgres must have exited; retrying.")
                    continue
                else:
                    try:
                        yield client.incref()
                    except:
                        # This case is unexpected, but should be reported in
                        # case something weird happens and the server crashes
                        # mid-incref.
                        f = Failure()
                        log_message("Incref command failed.")
                        f.printTraceback()
                    else:
                        returnValue(None)
                continue

        log_message("Control socket _not_ locked; this should be okay if xpostgres is already running")
        returnValue(None)


class CtlStop(XPGCtlCommand):
    """
    Implementation of C{xpg_ctl stop}.

    If there's already an xpostgres server running, decrement its reference
    count.  Otherwise do nothing; when the reference-count gets to zero it
    should exit itself.
    """

    @inlineCallbacks
    def execute(self):
        # Serialize all 'start' and 'stop' invocations so that refcounting is
        # atomic.
        yield self.xpg_ctl.acquire_lock(True)
        try:
            client = yield self.xpg_ctl.control_client(True)
        except ConnectError:
            pass
        else:
            if client is None:
                # Delegate to pg_ctl for error message.
                log_debug("Calling pg_ctl stop")
                yield CtlPassthrough(self.xpg_ctl).execute()
            else:
                log_debug("sending Decref message")
                yield client.decref()


class CtlRestart(XPGCtlCommand):
    """
    Implementation of C{xpg_ctl restart}.

    Send a command to xpostgres telling it to re-execute postgres with new
    command line options, without changing its refcount.
    """

    @inlineCallbacks
    def execute(self):
        try:
            client = yield self.xpg_ctl.control_client()
            yield client.restart()
        except ConnectError:
            pass



class CtlPassthrough(XPGCtlCommand):
    """
    Implementation of C{xpg_ctl <anything but stop, start, or restart>}.  For
    example, C{xpg_ctl status}.

    Do whatever C{pg_ctl} does in this situation.
    """

    def __init__(self, xpg_ctl, extra_args=[]):
        """
        Construct a L{CtlPassthrough} from an L{XPGCtlCommand}.
        """
        super(CtlPassthrough, self).__init__(xpg_ctl)
        self.extra_args = extra_args


    def execute(self):
        """
        Use C{exec} to invoke C{pg_ctl}.

        This method never returns.
        """
        args = [PG_CTL] + self.extra_args + self.xpg_ctl.original_args
        log_message('Executing pg_ctl ' + repr(args))
        # Note that for lock inheritance, we must use the same environ as
        # updated by InheritableFilesystemLock.
        done = simple_spawn(self.xpg_ctl.reactor, args, os.environ.copy())
        done.addCallback(lambda whatever: self.terminated(whatever))
        return done


    def terminated(self, status):
        exitCode[0] = status
        log_debug("pg_ctl terminated: " + repr(exitCode[0]))



class NoControlPath(Exception):
    """
    Exception raised when no socket path can be discovered.
    """



class XPGCtl(object):

    def __init__(self, reactor):
        self.reactor = reactor
        self.original_args = []
        self.command_object = None

        # defaults
        self.options = ''
        self._xpg = None


    def xpostgres_object(self):
        """
        Construct an L{XPostgres} object based on the information gathered thus
        far, for the purposes of examining various filesystem locations.
        """
        if self._xpg is not None:
            return self._xpg
        xpg = XPostgres(self.reactor)
        xpg.parse_command_line(
            ([XPOSTGRES] +
             shell_split(self.options, comments=True, posix=True) +
             (['-D', self.data_directory] if self.data_directory else [])),
            self.postgres_env
        )
        self._xpg = xpg
        return xpg


    @inlineCallbacks
    def acquire_lock(self, use_pidfile=False):
        """
        Acquire the C{.xpg_ctl.pid} lock in the socket directory, for a given
        L{XPostgres} instance that describes the cluster in question.
        """
        dotfile = os.path.join(
            os.path.dirname(self.control_client_path(use_pidfile)),
            ".xpg_ctl.pid"
        )
        self.dotfile_lock = InheritableFilesystemLock(dotfile, self.reactor)
        try:
            log_debug("CTL Lock: " + repr(self.dotfile_lock.name))
            result = (yield self.dotfile_lock.deferUntilLocked(30.0))
        except TimeoutError:
            log_debug("xpg_ctl timed out waiting for lock on " + repr(dotfile))
            returnValue(False)
        else:
            log_debug("CTL Lock Acquired: " + repr(result))


    def control_client_path(self, use_pidfile=False):
        if use_pidfile:
            pm_pid_path = os.path.join(self.data_directory, 'postmaster.pid')
            try:
                f = open(pm_pid_path, "rb")
            except:
                log_message("Could not open {}.".format(pm_pid_path))
                raise NoControlPath()
            with f as f:
                return os.path.join(
                    f.read().split('\n')[LOCK_FILE_LINE_SOCKET_DIR - 1],
                    XPG_SOCKET_NAME
                )
        else:
            return self.xpostgres_object().control_socket_path


    def control_client(self, use_pidfile=False):
        """
        Return a Deferred that fires with a L{ControlClient} when it can
        connect.  Fails with L{ConnectError} if it can't be done.
        """
        sockpath = self.control_client_path(use_pidfile)
        endpoint = UNIXClientEndpoint(self.reactor, sockpath)
        return endpoint.connect(ControlClientFactory())


    def parse_command_line(self, argv, env):
        self.original_args = argv[1:]
        self.postgres_env = env
        short_options = "cD:l:m:N:o:p:P:sS:t:U:wWV?"
        no_argument = False
        required_argument = True
        long_options = [
            ("help", no_argument, '?'),
            ("version", no_argument, 'V'),
            ("log", required_argument, 'l'),
            ("mode", required_argument, 'm'),
            ("pgdata", required_argument, 'D'),
            ("silent", no_argument, 's'),
            ("timeout", required_argument, 't'),
            ("core-files", no_argument, 'c'),
        ]
        option_synonyms = dict(((k, '-'+v) for (k, b, v) in long_options))
        optlist, args = getopt.gnu_getopt(
            argv, short_options,
            [option + ('=' if required else '') for
             (option, required, short) in long_options])

        option_name_mapping = {
            # -option: (attribute_name, has_value)
            '-c': ('core_files', False),
            '-D': ('data_directory', True),
            '-m': ('mode', True),
            '-l': ('logfile', True),
            '-o': ('options', True),
            '-p': ('path', True), # ignored?
            '-s': ('silent', False),
            '-t': ('timeout_seconds', True),
            '-V': ('version', False),
            '-w': ('wait', False),
            '-W': ('no_wait', False),
            '-?': ('help', False),

            # Windows options; just need to ignore these.
            '-N': ('win_servicename', False),
            '-P': ('win_password', False),
            '-S': ('win_start_type', False),
            '-U': ('win_username', False),
        }

        for (opt, val) in optlist:
            matchobj = re.match(r"^--(.*)$", opt)
            if matchobj:
                opt = matchobj.group(1)
            opt = option_synonyms.get(opt, opt)
            try:
                attr_name, has_value = option_name_mapping[opt]
            except KeyError:
                log_message("Invalid option specified for " + str(opt))
                continue
            if not has_value:
                val = True
            setattr(self, attr_name, val)
        if len(args) < 2:
            command = None
        else:
            command = args[1]
        pg_data = env.get("PGDATA")
        if pg_data is not None:
            self.data_directory = pg_data

        if hasattr(self, 'logfile'):
            if os.path.isfile(self.logfile):
                if not os.access(self.logfile, os.W_OK):
                    log_message("Specified log file cannot be written to.")
                    returnValue(1)
            elif os.path.isdir(os.path.dirname(self.logfile)):
                if not os.access(os.path.dirname(self.logfile), os.W_OK):
                    log_message("Specified log file directory cannot be written to.")
                    returnValue(1)
            else:
                log_message("Specified log file cannot be written to: no existing parent directory.")
                returnValue(1)

        command_class = {
            'start': CtlStart,
            'stop': CtlStop,
            'restart': CtlRestart,
        }.get(command, CtlPassthrough)
        self.command_object = command_class(self)


    @inlineCallbacks
    def do_everything(self, argv, environ):
        yield self.parse_command_line(argv, environ)
        try:
            result = yield self.command_object.execute()
        except NoControlPath:
            returnValue(7)
        else:
            returnValue(result)



class XPGArchive(object):
    def __init__(self, reactor):
        self.reactor = reactor


    def do_everything(self, argv, environ):
        fromPath = FilePath(argv[2])
        toPath = FilePath(argv[3])

        if toPath.exists() and toPath.getsize() == fromPath.getsize():
            # Already exists, and it's the right size.  OK.
            sys.stderr.write("{0!r} === {1!r}\n".format(fromPath.path,
                                                        toPath.path))
        else:
            sys.stderr.write("{0!r} ... {1!r}\n".format(fromPath.path,
                                                        toPath.path))
            temporary = toPath.temporarySibling(TEMP_EXT)
            fromPath.copyTo(temporary)
            temporary.moveTo(toPath)
            toPath.chmod(0o600)
            sys.stderr.write("{0!r} --> {1!r}\n".format(fromPath.path,
                                                        toPath.path))
        return succeed(None)



@inlineCallbacks
def main(reactor, argv, environ):
    if '_ctl' in argv[0] or environ.pop('BEHAVE_AS_XPG_CTL', False):
        xpg = XPGCtl(reactor)
    elif len(argv) > 1 and argv[1] == 'archive':
        # archive_command case.
        xpg = XPGArchive(reactor)
    else:
        xpg = XPostgres(reactor)
    try:
        result = yield xpg.do_everything(argv, environ)
        returnValue(result)
    finally:
        log_debug("Goodbye.")
        if reactor.running:
            reactor.stop()



if __name__ == "__main__":
    from twisted.internet import reactor
    from sys import argv
    from os import environ
    exitCode = [0]
    def start():
        ran = main(reactor, argv, environ)
        def done(result):
            if result is not None:
                if isinstance(result, Failure):
                    result.printTraceback()
                    exitCode[0] = 1
                else:
                    exitCode[0] = result
        ran.addBoth(done)

    reactor.callWhenRunning(start)
    reactor.run()
    os._exit(exitCode[0])
