# $Id: Logger.pm,v 1.1 2004/04/19 17:50:31 dasenbro Exp $ 

package Razor2::Logger;

use strict;
use Razor2::Syslog;
use Time::HiRes qw(gettimeofday);
use POSIX qw(strftime);
use IO::File;
# 2003/09/10 Anne Bennett: syslog of our choice (uses socket,
# does not assume network listener).
use Sys::Syslog;

# designed to be inherited module
# but can stand alone. 

sub new { 

    my ($class, %args) = @_;
    my %self = ( %args );
    my $self = bless \%self, $class;

    my $prefix = $args{LogPrefix} || 'razord2';
    my $facility = $args{LogFacility} || 'local3';
    my $loghost = $args{LogHost} || '127.0.0.1';

    if ($self->{LogTo} eq 'syslog') { 
        $$self{syslog} = new Razor2::Syslog (Facility=> $facility, Priority => 'debug', Name => $prefix, SyslogHost => $loghost);
        $self->{LogType} = 'syslog';
    } elsif ($self->{LogTo} =~ /^file:(.*)$/) {
        $self->{LogType} = 'file';
        my $name = $1; chomp $name;
        open (LOGF, ">>$name") or do { 
            if ($self->{DontDie}) { 
                open LOGF, ">>/dev/null" or do { 
                    print STDERR "Failed to open /dev/null, $!\n";
                };
            } else { 
                die $!;
            }
        };
        LOGF->autoflush(1);
        $self->{fd} = *LOGF{IO};
    } elsif ($self->{LogTo} eq 'sys-syslog') { 
        # 2003/09/10 Anne Bennett: syslog of our choice (uses socket,
        # does not assume network listener).
        $self->{LogType} = 'sys-syslog';
        openlog($prefix,"pid",$facility);
    } elsif ($self->{LogTo} eq 'stdout') { 
        $self->{LogType} = 'file';
        $self->{fd} = *STDOUT{IO};
    } elsif ($self->{LogTo} eq 'stderr') { 
        $self->{LogType} = 'file';
        $self->{fd} = *STDERR{IO};
    } else {
        $self->{LogType} = 'file';
        $self->{fd} = *STDERR{IO};
    }

    $self->{LogTimeFormat} ||= "%b %d %H:%M:%S";  # formatting from strftime()
    $self->{LogDebugLevel}   = exists $self->{LogDebugLevel} ? $self->{LogDebugLevel} : 5;
    $self->{Log2FileDir}   ||= "/tmp";

    # 2002/11/27 Anne Bennett: log this at level 2 so we can set level
    #   1 (to get errors only) and avoid this unneeded line.
    $self->log(2,"[bootup] Logging initiated LogDebugLevel=$self->{LogDebugLevel} to $self->{LogTo}");

    return $self;

}


sub log { 

    my ($self, $prio, $message) = @_;

    return unless $prio <= $self->{LogDebugLevel};

    my ($package, $filename, $line) = caller;
    $filename =~ s:.*/::;

    if ($self->{LogType} eq 'syslog') {
    
        my $logstr = sprintf("[%2d] %s\n", $prio, $message);
        $logstr =~ s/\n+\n$/\n/;
        $self->{syslog}->send($logstr, Priority => 'debug');

    } elsif ($self->{LogType} eq 'sys-syslog') { 
        # 2003/09/10 Anne Bennett: syslog of our choice (uses socket,
        # does not assume network listener).
        my $logstr = sprintf("[%2d] %s\n", $prio, $message);
        $logstr =~ s/\n+$//g;
        syslog("debug",$logstr);

    } elsif ($self->{LogType} eq 'file') { 

        my $now_string;
        if ($self->{LogTimestamp}) {
            my ($seconds, $microseconds) = gettimeofday;
            $now_string = strftime $self->{LogTimeFormat}, localtime($seconds);
            $now_string .= sprintf ".%06d ", $microseconds;
        }

        my $logstr = sprintf("%s[%d]: [%2d] %s\n", $self->{LogPrefix}, $$, $prio, $message);
        $logstr =~ s/\n+\n$/\n/;
        my $fd = $self->{fd};
        print $fd "$now_string$logstr";

    }
            
    return 1;
} 

sub log2file {
    my ($self, $prio, $textref, $fn_ext) = @_;

    return unless $prio <= $self->{LogDebugLevel};

    unless (ref($textref) eq 'SCALAR') {
        print "log2file: not a scalar ref ($fn_ext)\n";
        return;
    }
    my $len = length($$textref);
    my $fn = "$self->{Log2FileDir}/razor.$$.$fn_ext";

    if (open OUT, ">$fn") {
        print OUT $$textref;
        close OUT;
        $self->log($prio,"log2file: wrote message len=$len to file: $fn");
    } else {
        $self->log($prio,"log2file: could not write to $fn: $!");
    }
}

1;

