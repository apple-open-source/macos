package Mail::Audit;

# $Id: Audit.pm,v 1.1 2004/04/09 17:04:46 dasenbro Exp $

my $logging;
my $loglevel=3;
my $logfile = "/tmp/".getpwuid($>)."-audit.log";

# ----------------------------------------------------------
# no user-modifiable parts below this line.
# ----------------------------------------------------------

use strict;
use File::Basename;
use Mail::Internet;
use Mail::Audit::MailInternet;
use Sys::Hostname; (my $HOSTNAME = hostname) =~ s/\..*//;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $ASSUME_MSGPREFIX);
# @ISA will depend on whether the message is MIME; if it is, we'll be MIME::Entity.  if not, we'll be Mail::Internet.
use Fcntl ':flock';

$ASSUME_MSGPREFIX = 0;

# stolen from linux sysexits.h, YMMV on other OSes.  sorry, but it was either this or forcing everyone to h2ph.
use constant EX_USAGE       => 64; # command line usage error
use constant EX_DATAERR     => 65; # data format error
use constant EX_NOINPUT     => 66; # cannot open input
use constant EX_NOUSER      => 67; # addressee unknown
use constant EX_NOHOST      => 68; # host name unknown
use constant EX_UNAVAILABLE => 69; # service unavailable
use constant EX_SOFTWARE    => 70; # internal software error
use constant EX_OSERR       => 71; # system error (e.g., can't fork)
use constant EX_OSFILE      => 72; # critical OS file missing
use constant EX_CANTCREAT   => 73; # can't create (user) output file
use constant EX_IOERR       => 74; # input/output error
use constant EX_TEMPFAIL    => 75; # temp failure; user is invited to retry
use constant EX_PROTOCOL    => 76; # remote error in protocol
use constant EX_NOPERM      => 77; # permission denied
use constant EX_CONFIG      => 78; # configuration error

use constant DEFERRED  => EX_TEMPFAIL;
use constant REJECTED  => 100;
use constant DELIVERED => 0;

$VERSION = '2.1';

=head1 NAME

Mail::Audit - Library for creating easy mail filters

=head1 SYNOPSIS

 use Mail::Audit; # use Mail::Audit qw(...plugins...);
 my $mail = Mail::Audit->new(emergency=>"~/emergency_mbox");
 $mail->pipe("listgate p5p")            if $mail->from =~ /perl5-porters/;
 $mail->accept("perl")                  if $mail->from =~ /perl/;
 $mail->reject("We do not accept spam") if $mail->rblcheck();
 $mail->ignore                          if $mail->subject =~ /boring/i;
 ...
 $mail->noexit(1); $mail->accept("~/Mail/Archive/%Y%m%d"); $mail->noexit(0);
 $mail->accept()

=head1 DESCRIPTION

F<procmail> is nasty. It has a tortuous and complicated recipe format,
and I don't like it. I wanted something flexible whereby I could filter
my mail using Perl tests.

C<Mail::Audit> was inspired by Tom Christiansen's F<audit_mail> and
F<deliverlib> programs. It allows a piece of email to be logged,
examined, accepted into a mailbox, filtered, resent elsewhere, rejected,
replied to, and so on. It's designed to allow you to easily create filter programs
to stick in a F<.forward> file or similar.

C<Mail::Audit> groks MIME; when appropriate, it subclasses C<MIME::Entity>.
Read the MIME::Tools man page for details.

=cut

sub import {
    my $pkg = shift;
    for (@_) {
         eval "use $pkg"."::$_"; die $@ if $@;
    }
}

sub _log {
    my ($priority, $what) = @_; 
    return if $loglevel < $priority;
    chomp $what; chomp $what;
    my ($subroutine) = (caller(1))[3]; $subroutine =~ s/(.*):://;
    my ($line)       = (caller(0))[2];
    print LOG "$line($subroutine): $what\n";
}

=head1 CONSTRUCTOR

=over 4

=item C<new(%options)>

The constructor reads a mail message from C<STDIN> (or, if the C<data>
option is set, from an array reference) and creates a C<Mail::Audit>
object from it.

Other options include the C<accept>, C<reject> or C<pipe> keys, which
specify subroutine references to override the methods with those names.

You are encouraged to specify an C<emergency> argument and
check for the appearance of messages in that mailbox on a
regular basis.  If for any reason an C<accept()> is
unsuccessful, the message will be saved to the C<emergency>
mailbox instead.  If no C<emergency> mailbox is defined,
messages will be deferred back to the MTA, where they will
show up in your mailq.

You may also specify C<< log => $logfile >> to write a debugging log; you
can set the verbosity of the log with the C<loglevel> key, on a scale of
1 to 4. If you specify a log level without a log file, logging will be
written to F</tmp/you-audit.log> where F<you> is replaced by your user
name.

Usually, the delivery methods C<accept>, C<pipe>, and
C<resend> are final; Mail::Audit will terminate when they
are done.  If you specify C<< noexit => 1 >>, C<Mail::Audit>
will not exit after completing the above actions, but
continue running your script.

The C<reject> delivery method is always final; C<noexit> has
no effect.

Percent (%) signs seen in arguments to C<accept> and C<pipe>
do not undergo C<strftime> interpolation by default.  If you
want this, use the C<interpolate_strftime> option.  You can
override the "global" interpolate_strftime option by passing
an overriding option to C<accept> and C<pipe>.

By default, MIME messages are automatically recognized and
parsed.  This is potentially expensive; if you don't want
MIME parsing, use the C<nomime> option.

=back

=cut

sub new { 
    my $class = shift;
    my %opts = @_;

    # 
    # set up logging
    # 

    open LOG, ">>/dev/null";
    if (exists $opts{loglevel}) { $logging = 1; $loglevel = $opts{loglevel}; }
    if (exists $opts{log})      { $logging = 1;                $logfile = $opts{log}; }
    if                           ($logging)     { open LOG, ">>$logfile" or open LOG, ">>/dev/null";
						  # this doesn't seem to propagate to the calling script.  hmm.
					      }

    _log(1, "------------------------------ new run at ". localtime);
    my $draft = Mail::Internet->new( exists $opts{data}? $opts{data} : \*STDIN, Modify=>0 );
    my $self = Mail::Audit::MailInternet->autotype_new( $draft );

    _log(2,"   From: " . ($self->get("from")));
    _log(2,"     To: " . ($self->get("to")));
    _log(2,"Subject: " . ($self->get("subject")));

    # do we have a MIME-Version header?
    # if so,  we subclass MIME::Entity.
    # if not, we remain   Mail::Internet, and, presumably, diminish, and go into the West.
    if ($self->get("MIME-Version")) {
	unless ($opts{'nomime'}) {
	    _log(3,"message is MIME.  MIME-Version is " . ($self->get("MIME-Version")));
	    eval { require Mail::Audit::MimeEntity; import Mail::Audit::MimeEntity; };
	    my $error;
	    ($self, $error) = Mail::Audit::MimeEntity->autotype_new( $self );
	    if ($error) { _log(0, $error) }
	}
	else { _log(3,"message is MIME, but 'nomime' option was set."); }
    }

    $self->{obj} = $self; # backwards-compatibility for everyone who desperately needed access to $self
    $self->{_audit_opts} = \%opts;
    $self->{_audit_opts}->{'noexit'} ||= 0;
    $self->{_audit_opts}->{'interpolate_strftime'} ||= 0;

    my $default_mbox = ($ENV{MAIL} || ( grep { -d $_ } qw(/var/spool/mail/ /var/mail/) )[0] . getpwuid($>));
    $self->{_default_mbox} = $default_mbox;
    $self->{_audit_opts}->{'emergency'} ||= $default_mbox;

    return $self;
}

=head1 DELIVERY METHODS

=over 4

=item C<accept($where, ...)>

You can choose to accept the mail into a mailbox by calling
the C<accept> method; with no argument, this accepts to
F</var/spool/mail/you>. The mailbox is opened append-write,
then locked F<LOCK_EX>, the mail written and then the
mailbox unlocked and closed.  If Mail::Audit sees that you
have a maildir style system, where F</var/spool/mail/you> is
a directory, it'll deliver in maildir style.  If the path
you specify does not exist, Mail::Audit will assume mbox,
unless it ends in /, which means maildir.

If multiple maildirs are given, Mail::Audit will use
hardlinks to deliver to them, so that multiple hardlinks
point to the same underlying file.  (If the maildirs turn
out to be on multiple filesystems, you get multiple files.)

If you want "%" signs to be expanded according to
strftime(3), you can pass C<accept> the option
C<interpolate_strftime>:

 accept( {interpolate_strftime=>1}, file1, file2, ... );

"interpolate_strftime" is not enabled by default for two
reasons: backward compatibility (though nobody I know has a
% in any mail folder name) and username interpolation: many
people like to save messages by their correspondent's
username, and that username may contain a % sign.  If you
are one of these people, you should

 $username =~ s/%/%%/g;

If your arguments contain "/", C<accept> will create
arbitarily deep subdirectories accordingly.  Untaint your
input by saying

 $username =~ s,/,-,g;

By default, C<accept> is final; Mail::Audit will terminate
after successfully accepting the message.  If you want to
keep going, set C<noexit>.

If for any reason C<accept> is unable to write the message
(eg. you're over quota), Mail::Audit will attempt delivery
to the C<emergency> mailbox.  If C<accept> was called with
multiple destinations, the C<emergency> action will only be
taken if the message couldn't be delivered to any of the
desired destinations.  By default the C<emergency> mailbox
is set to the system mailbox.  If we were unable to save to
the emergency mailbox, the message will be deferred back
into the MTA's queue.  This happens whether or not C<noexit>
is set, so if you observe that some of your C<accept>s
somehow aren't getting run, check your mailq.

If this isn't how you want local delivery to happen, you'll
need to override this method.

=cut

sub nifty_interpolate { # perform ~user and %Y%m%d strftime interpolation
    my $self = shift;
    my $local_opts = shift if ref($_[0]) eq 'HASH';
    my @out = @_;
    my @localtime = localtime;
    if (((exists $local_opts->{'interpolate_strftime'}
	  and    $local_opts->{'interpolate_strftime'})
	 or $self->{_audit_opts}->{'interpolate_strftime'})
	and grep { /%/ } @out) {
	require POSIX; import POSIX qw(strftime);
	@out = map { strftime($_, @localtime) } @out;
    }
    @out = map { s{^~/}     {((getpwuid($>))[7])."/"}e;
		 s{^~(\w+)/}{((getpwnam($1))[7])."/"}e;
		 $_ } @out;
    return @out;
}

# ----------------------------------------------------------
sub accept {
# ----------------------------------------------------------
    my $self = shift;
    return $self->{_audit_opts}->{accept}->(@_) if exists $self->{_audit_opts}->{accept};

    my $local_opts = {}; $local_opts = shift if ref($_[0]) eq "HASH";

    my @files = $self->nifty_interpolate($local_opts, @_);
    if (not @files) { @files = ($self->{_default_mbox}) }

    _log(2,"accepting to @files");

    # from man procmailrc:
    # 	If  it  is  a  directory,  the mail will be delivered to a
    # 	newly created, guaranteed to be unique file named $MSGPRE-
    # 	FIX* in the specified directory.  If the mailbox name ends
    # 	in "/.", then this directory  is  presumed  to  be  an  MH
    #   folder;  i.e.,  procmail will use the next number it finds
    # 	available.  If the mailbox name ends  in  "/",  then  this
    #   directory  is presumed to be a maildir folder; i.e., proc-
    # 	mail will deliver the message to a file in a  subdirectory
    # 	named  "tmp"  and  rename  it  to be inside a subdirectory
    # 	named "new".  If the mailbox is  specified  to  be  an  MH
    # 	folder  or maildir folder, procmail will create the neces-
    # 	sary directories if they don't exist,  rather  than  treat
    # 	the  mailbox as a non-existent filename.  When procmail is
    # 	delivering to directories, you can specify multiple direc-
    # 	tories  to  deliver  to  (procmail  will  do  so utilising
    # 	hardlinks).
    #
    # for now we will support maildir and mbox delivery.
    # MH delivery and MSGPREFIX delivery remain todo.

    my %accept_types = (mbox      => [],
			maildir   => [],
			mh        => [],
			msgprefix => [],
			);

    for my $file (@files) {
	my $mbox_or_maildir = $self->mbox_or_maildir($file);
	push @{$accept_types{$mbox_or_maildir}}, $file;
	_log(3, "$file is of type $mbox_or_maildir");
    }

    my $success_count = 0;
    foreach my $accept_type (sort keys %accept_types) {
	next if not @{$accept_types{$accept_type}};
	my $accept_handler = "accept_to_$accept_type";
	_log(3, "calling accept handler $accept_handler(@{$accept_types{$accept_type}})");
	$success_count += $self->$accept_handler(@{$accept_types{$accept_type}});
    }

    if ($success_count > 0) {
	_log(3, "delivered successfully to $success_count destinations at ".localtime);
	unless ((exists $local_opts->{noexit}
		 and    $local_opts->{noexit})
		or $self->{_audit_opts}->{noexit}
		) { _log(2,"Exiting with status DELIVERED = ".DELIVERED); exit DELIVERED; }
    }
    else { # nothing got delivered, take emergency action.

	# in this section you will often see
	#    $!=DEFERRED; die("unable to write to @files or to $emergency");
	# we say this instead of
	#    exit DEFERRED;
	# because we want to be able to trap the die message inside an eval {} for testing purposes.

	my $emergency = $self->{_audit_opts}->{emergency};
	if (not defined $emergency) {
	    _log(0, "unable to write to @files and no emergency mailbox defined; exiting EX_TEMPFAIL");
	    $!=DEFERRED; die("unable to write to @files");
	}
	else {
	    if (grep ($emergency eq $_, @files)) { # already tried that mailbox
		if (@files == 1) { _log(0, "unable to write to @files; exiting EX_TEMPFAIL"); }
		else             { _log(0, "unable to write to any of (@files), which includes the emergency mailbox; exiting EX_TEMPFAIL"); }
		$!=DEFERRED; die("unable to write to @files");
	    }
	    else {
		my $accept_type = $self->mbox_or_maildir($emergency);
		my $accept_handler = "accept_to_$accept_type";
		my $success = $self->$accept_handler($emergency);
		if (not $success) {
		    _log(0, "unable to write to @files or to emergency mailbox $emergency either; exiting EX_TEMPFAIL");
		    $!=DEFERRED; die("unable to write to @files");
		}
		else {
		    _log(0, "unable to write to @files; wrote to emergency mailbox $emergency.");
		}
	    }
	}
    }
}

# ----------------------------------------------------------
sub mbox_or_maildir {
# ----------------------------------------------------------
    my $self = shift;
    my $file = shift;

    if ($file =~ /\/$/)                                        { return "maildir"   }
    if ($file =~ /\/\.$/)                                      { return "mh"        }
    if (-d $file) {
	if (-d "$file/tmp" and -d "$file/new")                 { return "maildir"   }
	if (exists($self->{_audit_opts}->{ASSUME_MSGPREFIX})) {
	    if    ($self->{_audit_opts}->{ASSUME_MSGPREFIX})   { return "msgprefix" }
	    else                                               { return "maildir"   }
	                                                      }
	if ($ASSUME_MSGPREFIX)                                 { return "msgprefix" }
	else                                                   { return "maildir"   }
    }
    if ("default")                                             { return "mbox"      }
}

# ----------------------------------------------------------
sub accept_to_mbox {
# ----------------------------------------------------------
    my $self = shift; my $success_count = 0;
    foreach my $file (@_) {
	# auto-create the parent dir.
	if (my $mkdir_error = mkdir_p(dirname($file))) { _log(0, $mkdir_error); next; }
	my $error = $self->write_message($file, {need_from=>1, extra_newline=>1});
	if (not $error) { $success_count++ }
	else            { _log(1, $error); }
    }
    return $success_count;
}

# ----------------------------------------------------------
sub write_message {
# ----------------------------------------------------------
    my $self       = shift;
    my $file       = shift;
    my $write_opts = shift || {};

    $write_opts->{'need_from'} = 1 if not defined $write_opts->{'need_from'};
    $write_opts->{'need_lock'} = 1 if not defined $write_opts->{'need_lock'};
    $write_opts->{'extra_newline'} = 0 if not defined $write_opts->{'extra_newline'};

    _log(3, "writing to $file; options @{[%$write_opts]}");

    unless (open(FH, ">>$file")) { return "Couldn't open $file: $!"; }

    if ($write_opts->{'need_lock'}) { my $lock_error = audit_get_lock(\*FH, $file);
				      return $lock_error if $lock_error; }
    seek FH, 0, 2;

    if (not $write_opts->{'need_from'} and $self->head->header->[0] =~ /^From\s/) {
	_log(3,"mbox From line found, stripping because we're maildir");
	$self->delete_header("From ");
	$self->unescape_from();
    }

    if ($write_opts->{'need_from'} and $self->head->header->[0] !~ /^From\s/) {
	_log(3,"No mbox From line, making one up.");
	if (exists $ENV{UFLINE}) {
	    _log(3,"Looks qmail, but preline not run, prepending UFLINE, RPLINE, DTLINE");
	    print FH $ENV{UFLINE};
	    print FH $ENV{RPLINE};
	    print FH $ENV{DTLINE};
	} else {
	    my $from = ($self->get('Return-path') ||
			$self->get('Sender')      ||
			$self->get('Reply-To')    ||
			'root@localhost');
	    chomp $from;
	    $from = $1 if $from =~ /<(.*?)>/; # BUG: this doesn't address the other form of "(comment) email@address".
	    $from =~ s/\s+//g; # if any whitespace remains, get rid of it.

	    (my $fromtime = localtime) =~ s/(:\d\d) \S+ (\d{4})$/$1 $2/; # strip timezone.
	    print FH "From $from  $fromtime\n";
	}
    }

    _log(4, "printing self as mbox string.");
    print FH $self->as_string;
    print FH "\n" if $write_opts->{'extra_newline'}; # extra \n added because mutt seems to like a "\n\nFrom " in mbox files

    flock(FH, LOCK_UN) or return "Couldn't unlock $file";

    close FH           or return "Couldn't close $file after writing: $!";
    _log(4, "returning success.");
    return 0; # success
}

# ----------------------------------------------------------
# NOT IMPLEMENTED
# ----------------------------------------------------------

sub accept_to_mh        { my $self = shift; my $success_count = 0; return $success_count; } # not implemented
sub accept_to_msgprefix { my $self = shift; my $success_count = 0; return $success_count; } # not implemented

# variables for accept_to_maildir

my $maildir_time    = 0;
my $maildir_counter = 0;

# ----------------------------------------------------------
sub accept_to_maildir {
# ----------------------------------------------------------
    my $self = shift;
    my $success_count = 0;

    _log(3, "will write to @_");

    # since mutt won't add a lines tag to maildir messages, we'll add it here
    unless (length $self->get("Lines")) {
	my $num_lines = @{$self->body};
	$self->head->add("Lines", $num_lines);
	_log(4,"Adding Lines: $num_lines header");
    }

    if ($maildir_time != time) { $maildir_time = time; $maildir_counter = 0 } else { $maildir_counter++ }

    # write the tmp file.
    # hardlink to all the new files.
    # unlink the temp file.

    # 
    # write the tmp file in the first writable maildir directory.
    # 

    my $tmp_path;
    foreach my $file (my @maildirs = @_) {

	$file =~ s/\/$//;
	my $msg_file;
	do { $msg_file = join ".", ($maildir_time, $$ . "_$maildir_counter", $HOSTNAME); $maildir_counter++; }
	while ( -e "$file/tmp/$msg_file" );
	# todo: consider sleeping.  mengwong 20020116
	
	$tmp_path = "$file/tmp/$msg_file";
	_log(3,"writing to $tmp_path");

	# auto-create the maildir.
	if (my $mkdir_error = mkdir_p(map { "$file/$_" } qw(tmp new cur))) { _log(0, $mkdir_error); next; }

	my $error = $self->write_message($tmp_path, {need_from=>0, need_lock=>0});
	if (not $error) { last; }
	else            { _log(1, $error);
			  unlink $tmp_path;
			  $tmp_path = undef;
			  next;
		      }
    }

    if (not $tmp_path) { return 0 } # unable to write to any of the specified maildirs.

    # 
    # hardlink to all the new files
    # 

    foreach my $file (my @maildirs = @_) {
	$file =~ s/\/$//;

	my $msg_file;
	do { $msg_file = join ".", ($maildir_time=time, $$ . "_$maildir_counter", $HOSTNAME); $maildir_counter++; }
	while ( -e "$file/new/$msg_file" );

	# auto-create the maildir.
	if (my $mkdir_error = mkdir_p(map { "$file/$_" } qw(tmp new cur))) { _log(0, $mkdir_error); next; }

	my $new_path = "$file/new/$msg_file";
	_log(3,"maildir: hardlinking to $new_path");

	if    (link $tmp_path, $new_path) { $success_count++; }
	else {
	    require Errno; import Errno qw(EXDEV);
	    if ($! == &EXDEV) { # Invalid cross-device link, see /usr/**/include/*/errno.h
		_log(0,"Couldn't link $tmp_path to $new_path: $!");
		_log(0,"attempting direct maildir delivery to $new_path...");
		$success_count += $self->accept_to_maildir($file);
		next;
	    }
	    else { _log(0,"Couldn't link $tmp_path to $new_path: $!"); }
	}
    }

    # 
    # unlink the temp file.
    # 

    unlink $tmp_path or _log(1,"Couldn't unlink $tmp_path: $!");
    return $success_count;
}

=item C<reject($reason)>

This rejects the email; it will be bounced back to the sender as
undeliverable. If a reason is given, this will be included in the
bounce.

This is a final delivery method.  The C<noexit> option has no effect here.

=cut

# ----------------------------------------------------------
sub reject {
# ----------------------------------------------------------
    my $self=shift;
    return $self->{_audit_opts}->{reject}->(@_) if exists $self->{_audit_opts}->{reject};

    _log(1, "Rejecting with exitcode ". REJECTED ." and reason @_");

    $!=REJECTED; die(@_);

    # we say this instead of
    #    print STDERR @_; exit REJECTED;
    # because we want to be able to trap reject() inside an eval {} for testing purposes.
}

=item C<resend($address)>

Bounces the email in its entirety to another address.

This is a final delivery method.  Set C<noexit> if you want to keep going.

At this time this method is not overrideable by an argument to C<new>.

=cut

# ----------------------------------------------------------
sub resend         {
    my $self = shift;
    my $local_opts = {}; $local_opts = shift if ref($_[0]) eq "HASH";
    my $rcpt = shift;
    $self->smtpsend(To => $rcpt);

    unless ((exists $local_opts->{noexit}
	     and    $local_opts->{noexit})
	    or $self->{_audit_opts}->{noexit}) { _log(2,"Exiting with status DELIVERED = ".DELIVERED); exit DELIVERED; }
}
# ----------------------------------------------------------

=item C<pipe($program)>

This opens a pipe to an external program and feeds the mail to it.

This is a final delivery method.  Set C<noexit> if you want to keep going.

=cut

# ----------------------------------------------------------
sub pipe {
# ----------------------------------------------------------

    my $self = shift;
    return $self->{_audit_opts}->{pipe}->(@_) if exists $self->{_audit_opts}->{pipe};

    my $local_opts = {}; $local_opts = shift if ref($_[0]) eq "HASH";

    my ($file) = $self->nifty_interpolate($local_opts, shift);
    _log(1, "Piping to $file");
    unless (open (PIPE, "|$file")) {
	_log(0, "Couldn't open pipe $file: $!");
	$self->accept();
    }
    $self->print(\*PIPE);
    close PIPE;
    _log(3,"Pipe closed with status $?");

    unless ((exists $local_opts->{noexit}
	     and    $local_opts->{noexit})
	    or $self->{_audit_opts}->{noexit}) { _log(2,"Exiting with status DELIVERED = ".DELIVERED); exit DELIVERED; }
}

=item C<ignore>

This merely ignores the email, dropping it into the bit bucket for
eternity.

This is a final delivery method.  Set C<noexit> if you want to keep going.

=cut

# ----------------------------------------------------------
sub ignore         { my $self = shift; _log(1,"Ignoring");
		     my $local_opts = {}; $local_opts = shift if ref($_[0]) eq "HASH";
		     exit DELIVERED unless ((exists $local_opts->{noexit}
					     and    $local_opts->{noexit})
					    or $self->{_audit_opts}->{'noexit'}); }
# ----------------------------------------------------------

=item C< reply (body =E<gt> "...", %options) >

Sends an autoreply to the sender of the message.  Return
value: the recipient address of the reply.

Recognized content-related options are: from, subject, cc,
bcc, body.  The "To" field defaults to the incoming
message's "Reply-To" and "From" fields.  C<body> should be a
single multiline string.

Set the option C<EVEN_IF_FROM_DAEMON> to send a reply even if
the original message was from some sort of automated agent.
What that set, only X-Loop will stop loops.

If you use this method, use KillDups to keep track of who
you've autoreplied to, so you don't autoreply more than
once.

 use Mail::Audit qw(KillDups);
 $mail->reply(body=>"I am on vacation") if not $self->killdups($mail->from);

C<reply> is not considered a final delivery method, so
execution will continue after completion.

=cut

# ----------------------------------------------------------
sub reply {
# ----------------------------------------------------------
    my $self = shift;
    my %reply_opts = @_; foreach my $k (keys %reply_opts) { $reply_opts{lc $k} = delete $reply_opts{$k} } # lowercase option names

    if ($self->head->as_string =~ /(^(Mailing-List:|Precedence:.*(junk|bulk|list)|To: Multiple recipients of |(((Resent-)?(From|Sender)|X-Envelope-From):|>?From )([^>]*[^(.%\@a-z0-9])?(Post(ma?(st(e?r)?|n)|office)|(send)?Mail(er)?|daemon|m(mdf|ajordomo)|n?uucp|LIST(SERV|proc)|NETSERV|o(wner|ps)|r(e(quest|sponse)|oot)|b(ounce|bs\.smtp)|echo|mirror|s(erv(ices?|er)|mtp(error)?|ystem)|A(dmin(istrator)?|MMGR|utoanswer))(([^).!:a-z0-9][-_a-z0-9]*)?[%\@>\t ][^<)]*(\(.*\).*)?)?$([^>]|$)))/m) {
	unless (defined $reply_opts{even_if_from_daemon} and $reply_opts{even_if_from_daemon}) {
	    _log(2, "message is ^FROM_DAEMON, skipping reply");
	    return "(^FROM_DAEMON, no reply)";
	}
    }

    if (length $self->get("X-Loop") or
	length $self->get("X-Loop-Detect")) { return "(X-Loop header found, not replying)" }

    require Mail::Mailer;

    my $rcpt = ($reply_opts{"to"}
		|| $self->get("Resent-From")
		|| $self->get("Reply-To")
		|| $self->get("Return-Path")
		|| $self->get("From")
		|| $self->get("Sender")
		);
    my $subject = ($reply_opts{"subject"}
		   || (defined       $self->subject &&
		       length        $self->subject
		       ? (           $self->subject !~ /\bRe:/i
				     ? "Re: " . $self->subject
				     :          $self->subject)
		       : "your mail")
		   );

    chomp ($rcpt, $subject);

    my $reply = new Mail::Mailer qw(sendmail);

    my @references;
    @references = (defined $reply_opts{"references"}
		   ? (ref ($reply_opts{"references"})
		      ? map { split ' ', $_ } @{$reply_opts{"references"}}
		      : split ' ', $reply_opts{"references"})
		   : grep { length $_ } (split (' ', $self->get("References")),
					 split (' ', $self->get("Message-ID"))));
    @references = grep { /^<.*>$/ } @references;

    my %headers = (To      => $rcpt,
		   Subject => $subject,
		  );
    $headers{"CC"}         = $reply_opts{"cc"}    if defined $reply_opts{"cc"};
    $headers{"BCC"}        = $reply_opts{"bcc"}   if defined $reply_opts{"bcc"};
    $headers{"References"} = "@references"        if @references;
    $headers{"X-Loop"}     = $self->get("X-Loop") || "1";
    $headers{"X-Loop-Detect"} = $self->get("X-Loop-Detect") || "1";

    # foreach my $k (keys %headers) { $headers{$k} =~ tr/\n//d; print STDERR "$k: $headers{$k}\n"; }

    $reply->open( \%headers );

    print $reply (defined $reply_opts{'body'} ? $reply_opts{'body'} : "Your message has been received.\n");
    $reply->close;         # complete the message and send it

    _log(1,"reply sent to $rcpt");
    return $rcpt;
}

=back

=head1 HEADER MANAGEMENT METHODS

=item C<get($header)>

Retrieves the named header from the mail message.

=item C<put_header($header, $value)>

Inserts a new header into the mail message with the given value.

=item C<replace_header($header, $value)>

Removes the old header, adds a new one.

=item C<delete_header($header)>

Guess.

=head1 MISCELLANEOUS METHODS

=item C<tidy>

Tidies up the email as per L<Mail::Internet>.  If the message is a MIME message, nothing happens.

=item C<noexit( 0 or 1 )>

Toggle noexit.

=cut

# ----------------------------------------------------------
sub header         { $_[0]->head->as_string()            }
sub put_header     { $_[0]->head->add($_[1],$_[2]);      }
sub replace_header { $_[0]->head->replace ($_[1],$_[2]); }
sub delete_header  { $_[0]->head->delete ($_[1]);        }
sub get            { my $string = $_[0]->head->get($_[1]); chomp($string=(defined $string && length $string) ? $string : ""); $string; }
# ----------------------------------------------------------

# ----------------------------------------------------------
sub tidy           { $_[0]->tidy_body() } # inheriting from MIME::Entity breaks this.  mengwong 20020112
sub noexit         { $_[0]->{_audit_opts}->{'noexit'} = $_[1]; }
# ----------------------------------------------------------

=head1 ATTRIBUTE METHODS

The following attributes correspond to fields in the mail:

=over 4

=item *

from

=item *

to

=item *

subject

=item *

cc

=item *

bcc

=item C<body>

Returns a reference to an array of lines in the body of the email.

=item C<header>

Returns the header as a single string.

=item C<is_mime>

am I a MIME message?  If so, MIME::Entity methods apply.  Otherwise, Mail::Internet methods apply.

=back

=cut

# ----------------------------------------------------------
sub from           { $_[0]->get("From")            }
sub to             { $_[0]->get("To")              }
sub subject        { $_[0]->get("Subject")         }
sub bcc            { $_[0]->get("BCC")             }
sub cc             { $_[0]->get("CC")              }
sub received       { $_[0]->get("Received")        }
sub ismime         { &is_mime }
# ----------------------------------------------------------

# ----------------------------------------------------------
# utility functions
# ----------------------------------------------------------

sub audit_get_lock {
    my $FH   = shift;
    my $file = shift;
    _log(4, "  attempting to lock  file $file");
    for (1..10) {
	if (flock($FH, LOCK_EX)) { _log(4, "  successfully locked file $file"); return; }
	else                     { sleep $_ and next; }
    }
    _log(1,my $errstr="Couldn't get exclusive lock on $file");
    return $errstr;
}

sub mkdir_p { # mkdir -p
    return if not @_;
    return if not length $_[0];
    foreach (@_) {
	next if -d $_;
	while (/\/$/) { chop }
	_log(4, "$_ doesn't exist, creating.");
	if (my $error = mkdir_p(dirname($_))) { return $error }
	mkdir ($_, 0777) or return "unable to mkdir $_: $!";
    }
    return;
}

sub myALRM { die "alarm\n" }

1;
__END__

=head1 LICENSE

The usual. This program is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.

=head1 BUGS

http://rt.cpan.org/NoAuth/Bugs.html?Dist=Mail-Audit

=head1 CAVEATS

If your mailbox file in /var/spool/mail/ doesn't already
exist, you may need to use your standard system MDA to
create it.  After it's been created, Mail::Audit should be
able to append to it.  Mail::Audit may not be able to create
/var/spool/mail because programs run from .forward don't
inherit the special permissions needed to create files in
that directory.

=head1 AUTHORS

Simon Cozens <simon@cpan.org> wrote versions 1 and 2.

Meng Weng Wong <mengwong@pobox.com> turned a petite demure
v2.0 into a raging bloated v2.1, adding MIME support,
emergency recovery, filename interpolation, and autoreply
features.

=head1 SEE ALSO

L<Mail::Internet>, L<Mail::SMTP>, L<Mail::Audit::List>, L<Mail::Audit::PGP>,
L<Mail::Audit::MAPS>, L<Mail::Audit::KillDups>, L<Mail::Audit::Razor>...
