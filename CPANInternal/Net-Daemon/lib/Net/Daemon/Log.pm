# -*- perl -*-
#
#   $Id: Log.pm,v 1.3 1999/09/26 14:50:13 joe Exp $
#
#   Net::Daemon - Base class for implementing TCP/IP daemons
#
#   Copyright (C) 1998, Jochen Wiedmann
#                       Am Eisteich 9
#                       72555 Metzingen
#                       Germany
#
#                       Phone: +49 7123 14887
#                       Email: joe@ispsoft.de
#
#   All rights reserved.
#
#   You may distribute this package under the terms of either the GNU
#   General Public License or the Artistic License, as specified in the
#   Perl README file.
#
############################################################################

require 5.004;
use strict;


package Net::Daemon::Log;

$Net::Daemon::Log::VERSION = '0.01';


############################################################################
#
#   Name:    Log (Instance method)
#
#   Purpose: Does logging
#
#   Inputs:  $self - Server instance
#
#   Result:  TRUE, if the client has successfully authorized, FALSE
#            otherwise.
#
############################################################################


sub OpenLog($) {
    my $self = shift;
    return 1 unless ref($self);
    return $self->{'logfile'} if defined($self->{'logfile'});
    if ($Config::Config{'archname'} =~ /win32/i) {
	require Win32::EventLog;
	$self->{'eventLog'} = Win32::EventLog->new(ref($self), '')
	    or die "Cannot open EventLog:" . &Win32::GetLastError();
	$self->{'$eventId'} = 0;
    } else {
	eval { require Sys::Syslog };
	if ($@) {
	    die "Cannot open Syslog: $@";
	}
	if ($^O ne 'solaris'  &&  $^O ne 'freebsd'  &&
	    defined(&Sys::Syslog::setlogsock)  &&
	    eval { &Sys::Syslog::_PATH_LOG() }) {
	    &Sys::Syslog::setlogsock('unix');
	}
	&Sys::Syslog::openlog($self->{'logname'} || ref($self), 'pid',
			      $self->{'facility'} || 'daemon');
    }
    $self->{'logfile'} = 0;
}

sub Log ($$$;@) {
    my($self, $level, $format, @args) = @_;
    my $logfile = !ref($self) || $self->OpenLog();

    my $tid = '';
    if (ref($self)  &&  $self->{'mode'}) {
	if ($self->{'mode'} eq 'ithreads') {
	    if (my $sthread = threads->self()) {
		$tid = $sthread->tid() . ", ";
	    }
	} elsif ($self->{'mode'} eq 'threads') {
      if (my $sthread = Thread->self()) {
	  $tid = $sthread->tid() . ", ";
      }
    }
    }
    if ($logfile) {
	my $logtime = $self->LogTime();
	if (ref($logfile)) {
	    $logfile->print(sprintf("$logtime $level, $tid$format\n", @args));
	} else {
	    printf STDERR ("$logtime $level, $tid$format\n", @args);
	}
    } elsif (my $eventLog = $self->{'eventLog'}) {
	my($type, $category);
	if ($level eq 'debug') {
	    $type = Win32::EventLog::EVENTLOG_INFORMATION_TYPE();
	    $category = 10;
	} elsif ($level eq 'notice') {
	    $type = Win32::EventLog::EVENTLOG_INFORMATION_TYPE();
	    $category = 20;
	} else {
	    $type = Win32::EventLog::EVENTLOG_ERROR_TYPE();
	    $category = 50;
	}
	$eventLog->Report({
	    'Category' => $category,
	    'EventType' => $type,
	    'EventID' => ++$self->{'eventId'},
	    'Strings' => sprintf($format, @args),
	    'Data' => $tid
	    });
    } else {
	&Sys::Syslog::syslog($level, "$tid$format", @args);
    }
}

sub Debug ($$;@) {
    my $self = shift;
    if (!ref($self)  ||  $self->{'debug'}) {
	my $fmt = shift;
	$self->Log('debug', $fmt, @_);
    }
}

sub Error ($$;@) {
    my $self = shift; my $fmt = shift;
    $self->Log('err', $fmt, @_);
}

sub Fatal ($$;@) {
    my $self = shift; my $fmt = shift;
    my $msg = sprintf($fmt, @_);
    $self->Log('err', $msg);
    my($package, $filename, $line) = caller();
    die "$msg at $filename line $line.";
}

sub LogTime { scalar(localtime) }


1;

__END__

=head1 NAME

Net::Daemon::Log - Utility functions for logging


=head1 SYNOPSIS

  # Choose logging method: syslog or Win32::EventLog
  $self->{'facility'} = 'mail'; # Default: Daemon
  $self->{'logfile'} = undef;   # Default

  # Choose logging method: stderr
  $self->{'logfile'} = 1;

  # Choose logging method: IO handle
  my $file = IO::File->new("my.log", "a");
  $self->{'logfile'} = $file;


  # Debugging messages (equivalent):
  $self->Log('debug', "This is a debugging message");
  $self->Debug("This is a debugging message");

  # Error messages (equivalent):
  $self->Log('err', "This is an error message");
  $self->Error("This is an error message");

  # Fatal error messages (implies 'die')
  $self->Fatal("This is a fatal error message");


=head1 WARNING

THIS IS ALPHA SOFTWARE. It is *only* 'Alpha' because the interface (API)
is not finalised. The Alpha status does not reflect code quality or
stability.


=head1 DESCRIPTION

Net::Daemon::Log is a utility class for portable logging messages.
By default it uses syslog (Unix) or Win32::EventLog (Windows), but
logging messages can also be redirected to stderr or a log file.


=head2 Generic Logging

    $self->Log($level, $msg, @args);

This is the generic interface. The logging level is in syslog style,
thus one of the words 'debug', 'info', 'notice', 'err' or 'crit'.
You'll rarely need info and notice and I can hardly imagine a reason
for crit (critical). In 95% of all cases debug and err will be
sufficient.

The logging string $msg is a format string similar to printf.


=head2 Utility methods

    $self->Debug($msg, @args);
    $self->Error($msg, @args);
    $self->Fatal($msg, @args);

These are replacements for logging with levels debug and err. The difference
between the latter two is that Fatal includes throwing a Perl exception.


=head2 Chossing a logging target

By default logging will happen to syslog (Unix) or EventLog (Windows).
However you may choose logging to stderr by setting

    $self->{'logfile'} = 1;

This is required if neither of syslog and EventLog is available. An
alternative option is setting

    $self->{'logfile'} = $handle;

where $handle is any object supporting a I<print> method, for example
an IO::Handle object. Usually the logging target is choosen as soon
as you call $self->Log() the first time. However, you may force
choosing the target by doing a

    $self->OpenLog();

before calling Log the first time.


=head1 MULTITHREADING

The Multithreading capabitities of this class are depending heavily
on the underlying classes Sys::Syslog, Win32::EventLog or IO::Handle.
If they are thread safe, you can well assume that this package is
too. (The exception being that you should better call
$self->OpenLog() before threading.)


=head1 AUTHOR AND COPYRIGHT

  Net::Daemon is Copyright (C) 1998, Jochen Wiedmann
                                     Am Eisteich 9
                                     72555 Metzingen
                                     Germany

                                     Phone: +49 7123 14887
                                     Email: joe@ispsoft.de

  All rights reserved.

  You may distribute this package under the terms of either the GNU
  General Public License or the Artistic License, as specified in the
  Perl README file.


=head1 SEE ALSO

L<Net::Daemon(3)>, L<Sys::Syslog(3)>, L<Win32::EventLog(3)>,
L<IO::Handle(3)>

=cut

