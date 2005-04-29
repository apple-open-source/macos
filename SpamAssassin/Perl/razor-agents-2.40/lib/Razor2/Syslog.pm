package Razor2::Syslog;

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
use IO::Socket;
use IO::File;
use Data::Dumper;

require Exporter;

@ISA = qw(Exporter AutoLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	
);
$VERSION = '0.03';


# Preloaded methods go here.

my %syslog_priorities=(
	emerg   => 0,
	alert   => 1,
	crit    => 2,
	err     => 3,
	warning => 4,
    notice  => 5,
    info    => 6,
	debug   => 7
);

my %syslog_facilities=(
	kern	=> 0,
	user	=> 1,
	mail	=> 2,
	daemon	=> 3,	
	auth	=> 4,
	syslog	=> 5,
	lpr	=> 6,
	news 	=> 7,
	uucp 	=> 8,
	cron	=> 9,
	authpriv=> 10,
	ftp	=> 11,
	local0	=> 16,
	local1	=> 17,
	local2	=> 18,
	local3	=> 19,
	local4	=> 20,
	local5	=> 21,
	local6	=> 22,
);


sub new {

  my $class = shift;
  my $name = $0;
  if($name =~ /.+\/(.+)/){
     $name = $1;
  }
  my $self = { Name     => $name,
               Facility => 'local5',
               Priority => 'err',
               SyslogPort    => 514,
               SyslogHost    => '127.0.0.1'};
  bless $self,$class;
  my %par = @_;

  foreach (keys %par){
    $self->{$_}=$par{$_};
  }

  my $sock=new IO::Socket::INET(PeerAddr => $$self{SyslogHost},
                        PeerPort => $$self{SyslogPort},
                        Proto    => 'udp');
  die "Socket could not be created : $!\n" unless $sock;

  $self->{sock} = $sock;

  return $self;

}


sub send {

  my $self = shift;
  my $msg=shift;
  my %par = @_;
  my %local=%$self;

  foreach (keys %par){
    $local{$_}=$par{$_};
  }

  my $pid=$$;
  my $facility_i=$syslog_facilities{$local{Facility}};
  my $priority_i=$syslog_priorities{$local{Priority}};  

  if(!defined $facility_i){
    $facility_i=21;
  }

  if(!defined $priority_i){
    $priority_i=4;
  }

  my $d=(($facility_i<<3)|($priority_i));
  my $message = "<$d>$local{Name}\[$pid\]: $msg";

  my $sock = $self->{sock};

  print $sock $message;

}


# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

Razor2::Syslog -- Syslog support for Razor2

=head1 SYNOPSIS

  use Razor2::Syslog;
  my $s=new Razor2::Syslog(Facility=>'local4',Priority=>'debug');
  $s->send('see this in syslog',Priority=>'info');

=head1 DESCRIPTION

This module has been derived from Net::Syslog. Some optimizations were
made to Net::Syslog, in particular support for keeping a socket open. What
follows is the documentation for Net::Syslog, which completely applies to
this module.

Net::Syslog implements the intra-host syslog forwarding protocol. It is
not intended to replace the Sys::Syslog or Unix::Syslog modules, but
instead to provide a method of using syslog when a local syslogd is
unavailable or when you don't want to write syslog messages to the
local syslog.

The new call sets up default values, any of which can be overridden in the
send call.  Keys (listed with default values) are:

	Name		<calling script name>
	Facility 	local5
	Priority 	err
	SyslogPort    	514
	SyslogHost    	127.0.0.1

Valid Facilities are:
	kern, user, mail, daemon, auth, syslog, lpr, news, uucp, cron, 
	authpriv, ftp, local0, local1, local2, local3, local4, local5, local6

Valid Priorities are:
	emerg, alert, crit, err, warning, notice, info, debug



=head1 AUTHOR

 Les Howard, les@lesandchris.com
 Vipul Ved Prakash, mail@vipul.net

=head1 SEE ALSO

syslog(3), Sys::Syslog(3), syslogd(8), Unix::Syslog(3), IO::Socket, perl(1)

=cut
