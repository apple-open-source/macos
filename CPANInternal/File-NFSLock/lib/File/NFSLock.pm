# -*- perl -*-
#
#  File::NFSLock - bdpO - NFS compatible (safe) locking utility
#
#  $Id: NFSLock.pm,v 1.34 2003/05/13 18:06:41 hookbot Exp $
#
#  Copyright (C) 2002, Paul T Seamons
#                      paul@seamons.com
#                      http://seamons.com/
#
#                      Rob B Brown
#                      bbb@cpan.org
#
#  This package may be distributed under the terms of either the
#  GNU General Public License
#    or the
#  Perl Artistic License
#
#  All rights reserved.
#
#  Please read the perldoc File::NFSLock
#
################################################################

package File::NFSLock;

use strict;
use Exporter ();
use vars qw(@ISA @EXPORT_OK $VERSION $TYPES
            $LOCK_EXTENSION $SHARE_BIT $HOSTNAME $errstr
            $graceful_sig @CATCH_SIGS);
use Carp qw(croak confess);

@ISA = qw(Exporter);
@EXPORT_OK = qw(uncache);

$VERSION = '1.20';

#Get constants, but without the bloat of
#use Fcntl qw(LOCK_SH LOCK_EX LOCK_NB);
sub LOCK_SH {1}
sub LOCK_EX {2}
sub LOCK_NB {4}

### Convert lock_type to a number
$TYPES = {
  BLOCKING    => LOCK_EX,
  BL          => LOCK_EX,
  EXCLUSIVE   => LOCK_EX,
  EX          => LOCK_EX,
  NONBLOCKING => LOCK_EX | LOCK_NB,
  NB          => LOCK_EX | LOCK_NB,
  SHARED      => LOCK_SH,
  SH          => LOCK_SH,
};
$LOCK_EXTENSION = '.NFSLock'; # customizable extension
$HOSTNAME = undef;
$SHARE_BIT = 1;

###----------------------------------------------------------------###

my $graceful_sig = sub {
  print STDERR "Received SIG$_[0]\n" if @_;
  # Perl's exit should safely DESTROY any objects
  # still "alive" before calling the real _exit().
  exit;
};

@CATCH_SIGS = qw(TERM INT);

sub new {
  $errstr = undef;

  my $type  = shift;
  my $class = ref($type) || $type || __PACKAGE__;
  my $self  = {};

  ### allow for arguments by hash ref or serially
  if( @_ && ref $_[0] ){
    $self = shift;
  }else{
    $self->{file}      = shift;
    $self->{lock_type} = shift;
    $self->{blocking_timeout}   = shift;
    $self->{stale_lock_timeout} = shift;
  }
  $self->{file}       ||= "";
  $self->{lock_type}  ||= 0;
  $self->{blocking_timeout}   ||= 0;
  $self->{stale_lock_timeout} ||= 0;
  $self->{lock_pid} = $$;
  $self->{unlocked} = 1;
  foreach my $signal (@CATCH_SIGS) {
    if (!$SIG{$signal} ||
        $SIG{$signal} eq "DEFAULT") {
      $SIG{$signal} = $graceful_sig;
    }
  }

  ### force lock_type to be numerical
  if( $self->{lock_type} &&
      $self->{lock_type} !~ /^\d+/ &&
      exists $TYPES->{$self->{lock_type}} ){
    $self->{lock_type} = $TYPES->{$self->{lock_type}};
  }

  ### need the hostname
  if( !$HOSTNAME ){
    require Sys::Hostname;
    $HOSTNAME = &Sys::Hostname::hostname();
  }

  ### quick usage check
  croak ($errstr = "Usage: my \$f = $class->new('/pathtofile/file',\n"
         ."'BLOCKING|EXCLUSIVE|NONBLOCKING|SHARED', [blocking_timeout, stale_lock_timeout]);\n"
         ."(You passed \"$self->{file}\" and \"$self->{lock_type}\")")
    unless length($self->{file});

  croak ($errstr = "Unrecognized lock_type operation setting [$self->{lock_type}]")
    unless $self->{lock_type} && $self->{lock_type} =~ /^\d+$/;

  ### Input syntax checking passed, ready to bless
  bless $self, $class;

  ### choose a random filename
  $self->{rand_file} = rand_file( $self->{file} );

  ### choose the lock filename
  $self->{lock_file} = $self->{file} . $LOCK_EXTENSION;

  my $quit_time = $self->{blocking_timeout} &&
    !($self->{lock_type} & LOCK_NB) ?
      time() + $self->{blocking_timeout} : 0;

  ### remove an old lockfile if it is older than the stale_timeout
  if( -e $self->{lock_file} &&
      $self->{stale_lock_timeout} > 0 &&
      time() - (stat _)[9] > $self->{stale_lock_timeout} ){
    unlink $self->{lock_file};
  }

  while (1) {
    ### open the temporary file
    $self->create_magic
      or return undef;

    if ( $self->{lock_type} & LOCK_EX ) {
      last if $self->do_lock;
    } elsif ( $self->{lock_type} & LOCK_SH ) {
      last if $self->do_lock_shared;
    } else {
      $errstr = "Unknown lock_type [$self->{lock_type}]";
      return undef;
    }

    ### Lock failed!

    ### I know this may be a race condition, but it's okay.  It is just a
    ### stab in the dark to possibly find long dead processes.

    ### If lock exists and is readable, see who is mooching on the lock

    if ( -e $self->{lock_file} &&
         open (_FH,"+<$self->{lock_file}") ){

      my @mine = ();
      my @them = ();
      my @dead = ();

      my $has_lock_exclusive = !((stat _)[2] & $SHARE_BIT);
      my $try_lock_exclusive = !($self->{lock_type} & LOCK_SH);

      while(defined(my $line=<_FH>)){
        if ($line =~ /^$HOSTNAME (\d+) /) {
          my $pid = $1;
          if ($pid == $$) {       # This is me.
            push @mine, $line;
          }elsif(kill 0, $pid) {  # Still running on this host.
            push @them, $line;
          }else{                  # Finished running on this host.
            push @dead, $line;
          }
        } else {                  # Running on another host, so
          push @them, $line;      #  assume it is still running.
        }
      }

      ### If there was at least one stale lock discovered...
      if (@dead) {
        # Lock lock_file to avoid a race condition.
        local $LOCK_EXTENSION = ".shared";
        my $lock = new File::NFSLock {
          file => $self->{lock_file},
          lock_type => LOCK_EX,
          blocking_timeout => 62,
          stale_lock_timeout => 60,
        };

        ### Rescan in case lock contents were modified between time stale lock
        ###  was discovered and lockfile lock was acquired.
        seek (_FH, 0, 0);
        my $content = '';
        while(defined(my $line=<_FH>)){
          if ($line =~ /^$HOSTNAME (\d+) /) {
            my $pid = $1;
            next if (!kill 0, $pid);  # Skip dead locks from this host
          }
          $content .= $line;          # Save valid locks
        }

        ### Save any valid locks or wipe file.
        if( length($content) ){
          seek     _FH, 0, 0;
          print    _FH $content;
          truncate _FH, length($content);
          close    _FH;
        }else{
          close _FH;
          unlink $self->{lock_file};
        }

      ### No "dead" or stale locks found.
      } else {
        close _FH;
      }

      ### If attempting to acquire the same type of lock
      ###  that it is already locked with, and I've already
      ###  locked it myself, then it is safe to lock again.
      ### Just kick out successfully without really locking.
      ### Assumes locks will be released in the reverse
      ###  order from how they were established.
      if ($try_lock_exclusive eq $has_lock_exclusive && @mine){
        return $self;
      }
    }

    ### If non-blocking, then kick out now.
    ### ($errstr might already be set to the reason.)
    if ($self->{lock_type} & LOCK_NB) {
      $errstr ||= "NONBLOCKING lock failed!";
      return undef;
    }

    ### wait a moment
    sleep(1);

    ### but don't wait past the time out
    if( $quit_time && (time > $quit_time) ){
      $errstr = "Timed out waiting for blocking lock";
      return undef;
    }

    # BLOCKING Lock, So Keep Trying
  }

  ### clear up the NFS cache
  $self->uncache;

  ### Yes, the lock has been aquired.
  delete $self->{unlocked};

  return $self;
}

sub DESTROY {
  shift()->unlock();
}

sub unlock ($) {
  my $self = shift;
  if (!$self->{unlocked}) {
    unlink( $self->{rand_file} ) if -e $self->{rand_file};
    if( $self->{lock_type} & LOCK_SH ){
      return $self->do_unlock_shared;
    }else{
      return $self->do_unlock;
    }
    $self->{unlocked} = 1;
    foreach my $signal (@CATCH_SIGS) {
      if ($SIG{$signal} &&
          ($SIG{$signal} eq $graceful_sig)) {
        # Revert handler back to how it used to be.
        # Unfortunately, this will restore the
        # handler back even if there are other
        # locks still in tact, but for most cases,
        # it will still be an improvement.
        delete $SIG{$signal};
      }
    }
  }
  return 1;
}

###----------------------------------------------------------------###

# concepts for these routines were taken from Mail::Box which
# took the concepts from Mail::Folder


sub rand_file ($) {
  my $file = shift;
  "$file.tmp.". time()%10000 .'.'. $$ .'.'. int(rand()*10000);
}

sub create_magic ($;$) {
  $errstr = undef;
  my $self = shift;
  my $append_file = shift || $self->{rand_file};
  $self->{lock_line} ||= "$HOSTNAME $self->{lock_pid} ".time()." ".int(rand()*10000)."\n";
  local *_FH;
  open (_FH,">>$append_file") or do { $errstr = "Couldn't open \"$append_file\" [$!]"; return undef; };
  print _FH $self->{lock_line};
  close _FH;
  return 1;
}

sub do_lock {
  $errstr = undef;
  my $self = shift;
  my $lock_file = $self->{lock_file};
  my $rand_file = $self->{rand_file};
  my $chmod = 0600;
  chmod( $chmod, $rand_file)
    || die "I need ability to chmod files to adequatetly perform locking";

  ### try a hard link, if it worked
  ### two files are pointing to $rand_file
  my $success = link( $rand_file, $lock_file )
    && -e $rand_file && (stat _)[3] == 2;
  unlink $rand_file;

  return $success;
}

sub do_lock_shared {
  $errstr = undef;
  my $self = shift;
  my $lock_file  = $self->{lock_file};
  my $rand_file  = $self->{rand_file};

  ### chmod local file to make sure we know before
  my $chmod = 0600;
  $chmod |= $SHARE_BIT;
  chmod( $chmod, $rand_file)
    || die "I need ability to chmod files to adequatetly perform locking";

  ### lock the locking process
  local $LOCK_EXTENSION = ".shared";
  my $lock = new File::NFSLock {
    file => $lock_file,
    lock_type => LOCK_EX,
    blocking_timeout => 62,
    stale_lock_timeout => 60,
  };
  # The ".shared" lock will be released as this status
  # is returned, whether or not the status is successful.

  ### If I didn't have exclusive and the shared bit is not
  ### set, I have failed

  ### Try to create $lock_file from the special
  ### file with the magic $SHARE_BIT set.
  my $success = link( $rand_file, $lock_file);
  unlink $rand_file;
  if ( !$success &&
       -e $lock_file &&
       ((stat _)[2] & $SHARE_BIT) != $SHARE_BIT ){

    $errstr = 'Exclusive lock exists.';
    return undef;

  } elsif ( !$success ) {
    ### Shared lock exists, append my lock
    $self->create_magic ($self->{lock_file});
  }

  # Success
  return 1;
}

sub do_unlock ($) {
  return unlink shift->{lock_file};
}

sub do_unlock_shared ($) {
  $errstr = undef;
  my $self = shift;
  my $lock_file = $self->{lock_file};
  my $lock_line = $self->{lock_line};

  ### lock the locking process
  local $LOCK_EXTENSION = '.shared';
  my $lock = new File::NFSLock ($lock_file,LOCK_EX,62,60);

  ### get the handle on the lock file
  local *_FH;
  if( ! open (_FH,"+<$lock_file") ){
    if( ! -e $lock_file ){
      return 1;
    }else{
      die "Could not open for writing shared lock file $lock_file ($!)";
    }
  }

  ### read existing file
  my $content = '';
  while(defined(my $line=<_FH>)){
    next if $line eq $lock_line;
    $content .= $line;
  }

  ### other shared locks exist
  if( length($content) ){
    seek     _FH, 0, 0;
    print    _FH $content;
    truncate _FH, length($content);
    close    _FH;

  ### only I exist
  }else{
    close _FH;
    unlink $lock_file;
  }

}

sub uncache ($;$) {
  # allow as method call
  my $file = pop;
  ref $file && ($file = $file->{file});
  my $rand_file = rand_file( $file );

  ### hard link to the actual file which will bring it up to date
  return ( link( $file, $rand_file) && unlink($rand_file) );
}

sub newpid {
  my $self = shift;
  # Detect if this is the parent or the child
  if ($self->{lock_pid} == $$) {
    # This is the parent

    # Must wait for child to call newpid before processing.
    # A little patience for the child to call newpid
    my $patience = time + 10;
    while (time < $patience) {
      if (rename("$self->{lock_file}.fork",$self->{rand_file})) {
        # Child finished its newpid call.
        # Wipe the signal file.
        unlink $self->{rand_file};
        last;
      }
      # Brief pause before checking again
      # to avoid intensive IO across NFS.
      select(undef,undef,undef,0.1);
    }

    # Fake the parent into thinking it is already
    # unlocked because the child will take care of it.
    $self->{unlocked} = 1;
  } else {
    # This is the new child

    # The lock_line found in the lock_file contents
    # must be modified to reflect the new pid.

    # Fix lock_pid to the new pid.
    $self->{lock_pid} = $$;
    # Backup the old lock_line.
    my $old_line = $self->{lock_line};
    # Clear lock_line to create a fresh one.
    delete $self->{lock_line};
    # Append a new lock_line to the lock_file.
    $self->create_magic($self->{lock_file});
    # Remove the old lock_line from lock_file.
    local $self->{lock_line} = $old_line;
    $self->do_unlock_shared;
    # Create signal file to notify parent that
    # the lock_line entry has been delegated.
    open (_FH, ">$self->{lock_file}.fork");
    close(_FH);
  }
}

1;


=head1 NAME

File::NFSLock - perl module to do NFS (or not) locking

=head1 SYNOPSIS

  use File::NFSLock qw(uncache);
  use Fcntl qw(LOCK_EX LOCK_NB);

  my $file = "somefile";

  ### set up a lock - lasts until object looses scope
  if (my $lock = new File::NFSLock {
    file      => $file,
    lock_type => LOCK_EX|LOCK_NB,
    blocking_timeout   => 10,      # 10 sec
    stale_lock_timeout => 30 * 60, # 30 min
  }) {

    ### OR
    ### my $lock = File::NFSLock->new($file,LOCK_EX|LOCK_NB,10,30*60);

    ### do write protected stuff on $file
    ### at this point $file is uncached from NFS (most recent)
    open(FILE, "+<$file") || die $!;

    ### or open it any way you like
    ### my $fh = IO::File->open( $file, 'w' ) || die $!

    ### update (uncache across NFS) other files
    uncache("someotherfile1");
    uncache("someotherfile2");
    # open(FILE2,"someotherfile1");

    ### unlock it
    $lock->unlock();
    ### OR
    ### undef $lock;
    ### OR let $lock go out of scope
  }else{
    die "I couldn't lock the file [$File::NFSLock::errstr]";
  }


=head1 DESCRIPTION

Program based of concept of hard linking of files being atomic across
NFS.  This concept was mentioned in Mail::Box::Locker (which was
originally presented in Mail::Folder::Maildir).  Some routine flow is
taken from there -- particularly the idea of creating a random local
file, hard linking a common file to the local file, and then checking
the nlink status.  Some ideologies were not complete (uncache
mechanism, shared locking) and some coding was even incorrect (wrong
stat index).  File::NFSLock was written to be light, generic,
and fast.


=head1 USAGE

Locking occurs by creating a File::NFSLock object.  If the object
is created successfully, a lock is currently in place and remains in
place until the lock object goes out of scope (or calls the unlock
method).

A lock object is created by calling the new method and passing two
to four parameters in the following manner:

  my $lock = File::NFSLock->new($file,
                                $lock_type,
                                $blocking_timeout,
                                $stale_lock_timeout,
                                );

Additionally, parameters may be passed as a hashref:

  my $lock = File::NFSLock->new({
    file               => $file,
    lock_type          => $lock_type,
    blocking_timeout   => $blocking_timeout,
    stale_lock_timeout => $stale_lock_timeout,
  });

=head1 PARAMETERS

=over 4

=item Parameter 1: file

Filename of the file upon which it is anticipated that a write will
happen to.  Locking will provide the most recent version (uncached)
of this file upon a successful file lock.  It is not necessary
for this file to exist.

=item Parameter 2: lock_type

Lock type must be one of the following:

  BLOCKING
  BL
  EXCLUSIVE (BLOCKING)
  EX
  NONBLOCKING
  NB
  SHARED
  SH

Or else one or more of the following joined with '|':

  Fcntl::LOCK_EX() (BLOCKING)
  Fcntl::LOCK_NB() (NONBLOCKING)
  Fcntl::LOCK_SH() (SHARED)

Lock type determines whether the lock will be blocking, non blocking,
or shared.  Blocking locks will wait until other locks are removed
before the process continues.  Non blocking locks will return undef if
another process currently has the lock.  Shared will allow other
process to do a shared lock at the same time as long as there is not
already an exclusive lock obtained.

=item Parameter 3: blocking_timeout (optional)

Timeout is used in conjunction with a blocking timeout.  If specified,
File::NFSLock will block up to the number of seconds specified in
timeout before returning undef (could not get a lock).


=item Parameter 4: stale_lock_timeout (optional)

Timeout is used to see if an existing lock file is older than the stale
lock timeout.  If do_lock fails to get a lock, the modified time is checked
and do_lock is attempted again.  If the stale_lock_timeout is set to low, a
recursion load could exist so do_lock will only recurse 10 times (this is only
a problem if the stale_lock_timeout is set too low -- on the order of one or two
seconds).

=head1 METHODS

After the $lock object is instantiated with new,
as outlined above, some methods may be used for
additional functionality.

=head2 unlock

  $lock->unlock;

This method may be used to explicitly release a lock
that is aquired.  In most cases, it is not necessary
to call unlock directly since it will implicitly be
called when the object leaves whatever scope it is in.

=head2 uncache

  $lock->uncache;
  $lock->uncache("otherfile1");
  uncache("otherfile2");

This method is used to freshen up the contents of a
file across NFS, ignoring what is contained in the
NFS client cache.  It is always called from within
the new constructor on the file that the lock is
being attempted.  uncache may be used as either an
object method or as a stand alone subroutine.

=head2 newpid

  my $pid = fork;
  if (defined $pid) {
    # Fork Failed
  } elsif ($pid) {
    $lock->newpid; # Parent
  } else {
    $lock->newpid; # Child
  }

If fork() is called after a lock has been aquired,
then when the lock object leaves scope in either
the parent or child, it will be released.  This
behavior may be inappropriate for your application.
To delegate ownership of the lock from the parent
to the child, both the parent and child process
must call the newpid() method after a successful
fork() call.  This will prevent the parent from
releasing the lock when unlock is called or when
the lock object leaves scope.  This is also
useful to allow the parent to fail on subsequent
lock attempts if the child lock is still aquired.

=head1 FAILURE

On failure, a global variable, $File::NFSLock::errstr, should be set and should
contain the cause for the failure to get a lock.  Useful primarily for debugging.

=head1 LOCK_EXTENSION

By default File::NFSLock will use a lock file extenstion of ".NFSLock".  This is
in a global variable $File::NFSLock::LOCK_EXTENSION that may be changed to
suit other purposes (such as compatibility in mail systems).

=head1 BUGS

Notify paul@seamons.com or bbb@cpan.org if you spot anything.

=head2 FIFO

Locks are not necessarily obtained on a first come first serve basis.
Not only does this not seem fair to new processes trying to obtain a lock,
but it may cause a process starvation condition on heavily locked files.


=head2 DIRECTORIES

Locks cannot be obtained on directory nodes, nor can a directory node be
uncached with the uncache routine because hard links do not work with
directory nodes.  Some other algorithm might be used to uncache a
directory, but I am unaware of the best way to do it.  The biggest use I
can see would be to avoid NFS cache of directory modified and last accessed
timestamps.

=head1 INSTALL

Download and extract tarball before running
these commands in its base directory:

  perl Makefile.PL
  make
  make test
  make install

For RPM installation, download tarball before
running these commands in your _topdir:

  rpm -ta SOURCES/File-NFSLock-*.tar.gz
  rpm -ih RPMS/noarch/perl-File-NFSLock-*.rpm

=head1 AUTHORS

Paul T Seamons (paul@seamons.com) - Performed majority of the
programming with copious amounts of input from Rob Brown.

Rob B Brown (bbb@cpan.org) - In addition to helping in the
programming, Rob Brown provided most of the core testing to make sure
implementation worked properly.  He is now the current maintainer.

Also Mark Overmeer (mark@overmeer.net) - Author of Mail::Box::Locker,
from which some key concepts for File::NFSLock were taken.

Also Kevin Johnson (kjj@pobox.com) - Author of Mail::Folder::Maildir,
from which Mark Overmeer based Mail::Box::Locker.

=head1 COPYRIGHT

  Copyright (C) 2001
  Paul T Seamons
  paul@seamons.com
  http://seamons.com/

  Copyright (C) 2002-2003,
  Rob B Brown
  bbb@cpan.org

  This package may be distributed under the terms of either the
  GNU General Public License
    or the
  Perl Artistic License

  All rights reserved.

=cut
