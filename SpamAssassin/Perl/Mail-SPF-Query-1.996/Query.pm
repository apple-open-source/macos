package Mail::SPF::Query;

# ----------------------------------------------------------
# 		       Mail::SPF::Query
#
# 		       Meng Weng Wong
#		  <mengwong+spf@pobox.com>
# $Id: Query.pm,v 1.1 2004/04/19 17:50:29 dasenbro Exp $
# test an IP / sender address pair for pass/fail/nodata/error
#
# http://spf.pobox.com/
#
# this version is compatible with spf-draft-20040209.txt
#
# license: Academic Free License
#          modulo issues relating to Microsoft Caller-ID For Email
#
# The result of evaluating a SPF record associated with a domain is one of:
# 
# none - the domain does not have an SPF record.
# 
# neutral - domain explicitly wishes you to pretend it had no SPF record.
# 
# unknown - a permanent error -- such as missing SPF record
#           during "include" or "redirect", parse error, unknown
#           mechanism, record loop
# 
# error - some type of temporary failure, usually DNS related
# 
# softfail - explicit softfail --- please apply strict antispam checks
# 
# fail - explicit fail --- MTA may reject, MUA may discard
# 
# pass - explicit pass --- message is not a forgery
#
# TODO:
#  - add ipv6 support
#  - support the new header syntax, which will break some preexisting code
#    (Received-SPF will now have a three part structure)
#  - rename to v2.0 when we add caller-id support and the header syntax
# 
# BUGS:
#  mengwong 20031211
#    if there are multiple unrecognized mechanisms, they all
#    need to be preserved in the 'unknown' Received-SPF header.
#    right now only the first appears.
# 
#  mengwong 20040225: override and fallback keys need to be lc'ed at start
# 
# ----------------------------------------------------------

use 5.006;
use strict;
use warnings;
no warnings 'uninitialized';
use vars qw($VERSION $CACHE_TIMEOUT);

use URI::Escape;
use Net::CIDR::Lite;
use Net::DNS qw(); # by default it exports mx, which we define.

# ----------------------------------------------------------
# 		       initialization
# ----------------------------------------------------------

my $GUESS_MECHS = "a/24 mx/24 ptr";

my $TRUSTED_FORWARDER = "include:spf.trusted-forwarder.org";

my $DEFAULT_EXPLANATION = "Please see http://spf.pobox.com/why.html?sender=%{S}&ip=%{I}&receiver=%{xR}";

my @KNOWN_MECHANISMS = qw( a mx ptr include ip4 ip6 exists all );

my $MAX_LOOKUP_COUNT    = 20;

my $Domains_Queried = {};

# if not set, then softfail is treated as neutral.
my $softfail_supported = 1;

$VERSION = "1.996";

$CACHE_TIMEOUT = 120;

# ----------------------------------------------------------
# 	 no user-serviceable parts below this line
# ----------------------------------------------------------

my $looks_like_ipv4  = qr/\d+\.\d+\.\d+\.\d+/;
my $looks_like_email = qr/\S+\@\S+/;

=head1 NAME

Mail::SPF::Query - query Sender Policy Framework for an IP,email,helo

=head1 SYNOPSIS

  my $query = new Mail::SPF::Query (ip => "127.0.0.1", sender=>'foo@example.com', helo=>"somehost.example.com", trusted=>1, guess=>1);
  my ($result,           # pass | fail | softfail | neutral | none | error | unknown [mechanism]
      $smtp_comment,     # "please see http://spf.pobox.com/why.html?..."  when rejecting, return this string to the SMTP client
      $header_comment,   # prepend_header("Received-SPF" => "$result ($header_comment)")
      $spf_record,       # "v=spf1 ..." original SPF record for the domain
     ) = $query->result();

    if    ($result eq "pass") { "domain is (probably) not forged.  apply RHSBL and content filters" }
    elsif ($result eq "fail") { "domain is forged.  reject or save to spambox" }
    else                      { "domain has no SPF, or broken SPF, may be forged.  apply content filters" }

  The default mechanism for trusted=>1 is "include:spf.trusted-forwarder.org".
  The default mechanisms for guess=>1 are "a/24 mx/24 ptr".

=head1 ABSTRACT

The SPF protocol relies on sender domains to publish a DNS
whitelist of their designated outbound mailers.  Given an
envelope sender, Mail::SPF::Query determines the legitimacy
of an SMTP client IP.

=head1 DESCRIPTION

There are two ways to use Mail::SPF::Query.  Which you
choose depends on whether the domains your server is an MX
for have secondary MXes which your server doesn't know about.

The first style, calling ->result(), is suitable when all
mail is received directly from the originator's MTA.  If the
domains you receive do not have secondary MX entries, this
is appropriate.  This style of use is outlined in the
SYNOPSIS above.  This is the common case.

The second style is more complex, but works when your server
receives mail from secondary MXes.  This performs checks as
each recipient is handled.  If the message is coming from a
valid MX secondary for a recipient, then the SPF check is
not performed, and a "pass" response is returned right away.
To do this, call C<result2()> and C<message_result2()>
instead of C<result()>.

If you do not know what a secondary MX is, you probably
don't have one.  Use the first style.

You can try out Mail::SPF::Query on the command line with
the following command:

  perl -MMail::SPF::Query -le 'print for Mail::SPF::Query->new(helo=>shift, ipv4=>shift, sender=>shift)->result' myhost.mydomain.com 1.2.3.4 myname@myhost.mydomain.com


=head1 METHODS

=over

=item C<< Mail::SPF::Query->new() >>

  my $query = eval { new Mail::SPF::Query (ip    =>"127.0.0.1",
                                           sender=>'foo@example.com',
                                           helo  =>"host.example.com") };

  optional parameters:
     debug => 1, debuglog => sub { print STDERR "@_\n" },
     local => 'extra mechanisms',
     trusted => 1,                    # do trusted forwarder processing
     guess => 1,                      # do best_guess if no SPF record
     default_explanation => 'Please see http://spf.my.isp/spferror.html for details',
     max_lookup_count => 20,          # total number of SPF include/redirect queries
     sanitize => 0,                   # do not sanitize all returned strings
     myhostname => "foo.example.com", # prepended to header_comment
     fallback => {   "foo.com" => { record => "v=spf1 a mx -all", OPTION => VALUE },
                   "*.foo.com" => { record => "v=spf1 a mx -all", OPTION => VAULE }, },
     override => {   "bar.com" => { record => "v=spf1 a mx -all", OPTION => VALUE },
                   "*.bar.com" => { record => "v=spf1 a mx -all", OPTION => VAULE }, },
     callerid => {   "hotmail.com" => { check => 1 },
                   "*.hotmail.com" => { check => 1 },
                   "*."            => { check => 0 }, },

  if ($@) { warn "bad input to Mail::SPF::Query: $@" }

Set C<trusted=E<gt>1> to turned on automatic trusted_forwarder processing.
The mechanism C<include:spf.trusted-forwarder.org> is used just before a C<-all> or C<?all>.
The precise circumstances are somewhat more complicated, but it does get the case of C<v=spf1 -all>
right -- i.e. spf.trusted-forwarder.org is not checked.

Set C<guess=E<gt>1> to turned on automatic best_guess processing.
This will use the best_guess SPF record when one cannot be found
in the DNS. Note that this can only return C<pass> or C<neutral>. 
The C<trusted> and C<local> flags also operate when the best_guess is being used.

Set C<local=E<gt>'include:local.domain'> to include some extra processing just before a C<-all> or C<?all>.
The local processing happens just before the trusted processing.

Set C<default_explanation> to a string to be used if the SPF record does not provide
a specific explanation. The default value will direct the user to a page at spf.pobox.com with 
"Please see http://spf.pobox.com/why.html?sender=%{S}&ip=%{I}&receiver=%{xR}". Note that the
string has macro substitution performed.

Set C<sanitize> to 0 to get all the returned strings unsanitized. Alternatively, pass a function reference
and this function will be used to sanitize the returned values. The function must take a single string
argument and return a single string which contains the sanitized result.

Set C<debug=E<gt>1> to watch the queries happen.

Set C<fallback> to define "pretend" SPF records for domains
that don't publish them yet.  Wildcards are supported.

Set C<override> to define SPF records for domains that do
publish but which you want to override anyway.  Wildcards
are supported.

Set C<callerid> to look for Microsoft "Caller-ID for Email"
records if an SPF record is not found.  Wildcards are
supported.  You will need Expat, XML::Parser, and
LMAP::CID2SPF installed for this to work; if you do not have
these libraries installed, the lookup will not occur.  By
default, this library will only look for those records in
hotmail.com and microsoft.com domains.

If you want to always look for Caller-ID records, set

  ->new(..., callerid => { "*." => { check => 1 } })

If you never want to do Caller-ID,

  ->new(..., callerid => { "*." => { check => 0 } })

NOTE: domain name arguments to fallback, override, and
callerid need to be in all lowercase.

=cut

# ----------------------------------------------------------
sub new {
# ----------------------------------------------------------
  my $class = shift;
  my $query = bless { depth => 0,
		      @_,
		    }, $class;

  $query->{ipv4} = delete $query->{ip}   if $query->{ip}   and $query->{ip} =~ $looks_like_ipv4;
  $query->{helo} = delete $query->{ehlo} if $query->{ehlo};

  $query->{local} .= ' ' . $TRUSTED_FORWARDER if ($query->{trusted});

  $query->{trusted} = undef;

  $query->{spf_error_explanation} ||= "SPF record error";

  $query->{default_explanation} ||= $DEFAULT_EXPLANATION;

  $query->{default_record} = $GUESS_MECHS if ($query->{guess});

  if (($query->{sanitize} && !ref($query->{sanitize})) || !defined($query->{sanitize})) {
      # Apply default sanitizer
      $query->{sanitize} = \&strict_sanitize;
  }

  $query->{sender} =~ s/<(.*)>/$1/g;

  if (not ($query->{ipv4}   and length $query->{ipv4}))   { die "no IP address given to spfquery"   }

  for ($query->{sender}) { s/^\s+//; s/\s+$//; }

  $query->{spf_source} = "domain of $query->{sender}";

  ($query->{domain}) = $query->{sender} =~ /([^@]+)$/; # given foo@bar@baz.com, the domain is baz.com, not bar@baz.com.

  if (not $query->{helo}) { require Carp; import Carp qw(cluck); cluck ("Mail::SPF::Query: ->new() requires a \"helo\" argument.\n");
			    $query->{helo} = $query->{domain};
			  }

  $query->debuglog("new: ipv4=$query->{ipv4}, sender=$query->{sender}, helo=$query->{helo}");

  ($query->{helo}) =~ s/.*\@//; # strip localpart from helo

  if (not $query->{domain}) {
    $query->debuglog("spfquery: sender $query->{sender} has no domain, using HELO domain $query->{helo} instead.");
    $query->{domain} = $query->{helo};
    $query->{sender} = $query->{helo};
  }

  if (not length $query->{domain}) { die "unable to identify domain of sender $query->{sender}" }

  $query->{orig_domain} = $query->{domain};

  $query->{loop_report} = [$query->{domain}];

  ($query->{localpart}) = $query->{sender} =~ /(.+)\@/;
  $query->{localpart} = "postmaster" if not length $query->{localpart};

  $query->debuglog("localpart is $query->{localpart}");

  $query->{Reversed_IP} = ($query->{ipv4} ? reverse_in_addr($query->{ipv4}) :
			   $query->{ipv6} ? die "IPv6 not supported" : "");

  if (not $query->{myhostname}) {
    use Sys::Hostname; 
    eval { require Sys::Hostname::Long };
    $query->{myhostname} = $@ ? hostname() : Sys::Hostname::Long::hostname_long();
  }

  $query->{myhostname} ||= "localhost";

  $query->{callerid} ||= 
    {     "hotmail.com" => { check => 1 }, # by default, check microsoft and hotmail domains
	"*.hotmail.com" => { check => 1 }, # for caller-id records when no SPF record is found.
	"microsoft.com" => { check => 1 },
      "*.microsoft.com" => { check => 1 },
	           "*." => { check => 0 }, # by default, do not check any other domains
    };

  $query->post_new(@_) if $class->can("post_new");

  return $query;
}

=head2 $query->result()

  my ($result, $smtp_comment, $header_comment, $spf_record) = $query->result();

C<$result> will be one of C<pass>, C<fail>, C<softfail>, C<neutral>, C<none>, C<error> or C<unknown [...]>.

C<pass> means the client IP is a designated mailer for the
sender.  The mail should be accepted subject to local policy
regarding the sender.

C<fail> means the client IP is not a designated mailer, and
the sender wants you to reject the transaction for fear of
forgery.

C<softfail> means the client IP is not a designated mailer,
but the sender prefers that you accept the transaction
because it isn't absolutely sure all its users are mailing
through approved servers.  The C<softfail> status is often
used during initial deployment of SPF records by a domain.

C<neutral> means that the sender makes no assertion about the
status of the client IP.

C<none> means that there is no SPF record for this domain.

C<unknown [...]> means the domain has a configuration error in
the published data or defines a mechanism which this library
does not understand.  If the data contained an unrecognized
mechanism, it will be presented following "unknown".  You
should test for unknown using a regexp C</^unknown/> rather
than C<eq "unknown">.

C<error> means the DNS lookup encountered a temporary error
during processing.

Results are cached internally for a default of 120 seconds.
You can call C<-E<gt>result()> repeatedly; subsequent
lookups won't hit your DNS.

The smtp_comment should be displayed to the SMTP client.

The header_comment goes into a Received-SPF header, like so: C<Received-SPF: $result ($header_comment)>

The spf_record shows the original SPF record fetched for the
query.  If there is no SPF record, it is blank.  Otherwise,
it will start with "v=spf1" and contain the SPF mechanisms
and such that describe the domain.

Note that the strings returned by this method (and most of the other methods)
are (at least partially) under the control of the sender's 
domain. This means that, if the sender is an attacker,
the contents can be assumed to be hostile. 
The various methods that return these strings make sure
that (by default) the strings returned contain only
characters in the range 32 - 126. This behavior can
be changed by setting C<sanitize> to 0 to turn off sanitization
entirely. You can also set C<sanitize> to a function reference to
perform custom sanitization.
In particular, assume that the C<smtp_comment> might
contain a newline character. 

=cut


# ----------------------------------------------------------
#			    result
# ----------------------------------------------------------

sub result {
  my $query = shift;
  my %result_set;

  my ($result, $smtp_explanation, $smtp_why, $orig_txt) = $query->spfquery( ($query->{best_guess} ? $query->{guess_mechs} : () ) );

  # print STDERR "*** result = $result, exp = $smtp_explanation, why = $smtp_why\n";

  # before this, we weould see a "default" on the end: Please see http://spf.pobox.com/why.html?sender=mengwong%40vw.mailzone.com&ip=208.210.125.1&receiver=poopy.com: default
  $smtp_why = "" if $smtp_why eq "default";

  my $smtp_comment = ($smtp_explanation && $smtp_why) ? "$smtp_explanation: $smtp_why" : ($smtp_explanation || $smtp_why);

  $query->{smtp_comment} = $smtp_comment;

  my $header_comment = "$query->{myhostname}: ". $query->header_comment($result);

  # $result =~ s/\s.*$//; # this regex truncates "unknown some:mechanism" to just "unknown"

  return ($query->sanitize(lc $result),
	  $query->sanitize($smtp_comment),
	  $query->sanitize($header_comment),
	  $query->sanitize($orig_txt),
	 ) if wantarray;

  return  $query->sanitize(lc $result);
}

sub header_comment {
  my $query = shift;
  my $result = shift;
  my $ip = $query->ip;
  if ($result eq "pass" and $query->{smtp_comment} eq "localhost is always allowed.") { return $query->{smtp_comment} }

  return
    (  $result eq "pass"      ? "$query->{spf_source} designates $ip as permitted sender"
     : $result eq "fail"      ? "$query->{spf_source} does not designate $ip as permitted sender"
     : $result eq "softfail"  ? "transitioning $query->{spf_source} does not designate $ip as permitted sender"
     : $result =~ /^unknown / ? "encountered unrecognized mechanism during SPF processing of $query->{spf_source}"
     : $result eq "unknown"   ? "error in processing during lookup of $query->{sender}"
     : $result eq "neutral"   ? "$ip is neither permitted nor denied by domain of $query->{sender}"
     : $result eq "error"     ? "encountered temporary error during SPF processing of $query->{spf_source}"
     : $result eq "none"      ? "$query->{spf_source} does not designate permitted sender hosts" 
     :                          "could not perform SPF query for $query->{spf_source}" );

}

=item C<< $query->result2() >>

  my ($result, $smtp_comment, $header_comment, $spf_record) = $query->result2('recipient@domain', 'recipient2@domain');

C<result2> does everything that C<result> does, but it first
checks to see if the sending system is a recognized MX
secondary for the recipient(s). If so, then it returns C<pass>
and does not perform the SPF query. Note that the sending
system may be a MX secondary for some (but not all) of the
recipients for a multi-recipient message, which is why
result2 takes an argument list.  See also C<message_result2()>.

C<$result> will be one of C<pass>, C<fail>, C<neutral [...]>, or C<unknown>.
See C<result()> above for meanings.

If you have MX secondaries and if you are unable to
explicitly whitelist those secondaries before SPF tests
occur, you can use this method in place of C<result()>, calling
it as many times as there are recipients, or just providing
all the recipients at one time.

The smtp_comment can be displayed to the SMTP client.

For example:

  my $query = new Mail::SPF::Query (ip => "127.0.0.1",
                                    sender=>'foo@example.com',
                                    helo=>"somehost.example.com");

  ...

  my ($result, $smtp_comment, $header_comment);

  ($result, $smtp_comment, $header_comment) = $query->result2('recip1@mydomain.com');
  # return suitable error code based on $result eq 'fail' or not

  ($result, $smtp_comment, $header_comment) = $query->result2('recip2@mydom.org');
  # return suitable error code based on $result eq 'fail' or not

  ($result, $smtp_comment, $header_comment) = $query->message_result2();
  # return suitable error if $result eq 'fail'
  # prefix message with "Received-SPF: $result ($header_comment)"

This feature is relatively new to the module.  You can get
support on the mailing list spf-devel@listbox.com.

The methods C<result2()> and C<message_result2()> use "2" because they
work for secondary MXes. C<result2()> takes care to minimize the number of DNS operations
so that there is little performance penalty from using it in place of C<result()>.
In particular, if no arguments are supplied, then it just calls C<result()> and
returns the method response.

=cut

# ----------------------------------------------------------
#			    result2
# ----------------------------------------------------------

sub result2 {
  my $query = shift;
  my @recipients = @_;

  if (!$query->{result2}) {
      my $all_mx_secondary = 'neutral';

      foreach my $recip (@recipients) {
          my ($rhost) = $recip =~ /([^@]+)$/;

          $query->debuglog("result2: Checking status of recipient $recip (at host $rhost)");

          my $cache_result = $query->{mx_cache}->{$rhost};
          if (not defined($cache_result)) {
              $cache_result = $query->{mx_cache}->{$rhost} = is_secondary_for($rhost, $query->{ipv4}) ? 'yes' : 'no';
              $query->debuglog("result2: $query->{ipv4} is a MX for $rhost: $cache_result");
          }

          if ($cache_result eq 'yes') {
              $query->{is_mx_good} = [$query->sanitize('pass'),
                                      $query->sanitize('message from secondary MX'),
                                      $query->sanitize("$query->{myhostname}: message received from $query->{ipv4} which is an MX secondary for $recip"),
                                      undef];
              $all_mx_secondary = 'yes';
          } else {
              $all_mx_secondary = 'no';
              last;
          }
      }

      if ($all_mx_secondary eq 'yes') {
          return @{$query->{is_mx_good}} if wantarray;
          return $query->{is_mx_good}->[0];
      }

      my @result = $query->result();

      $query->{result2} = \@result;
  }

  return @{$query->{result2}} if wantarray;
  return $query->{result2}->[0];
}

sub is_secondary_for {
    my ($host, $addr) = @_;

    my $resolver = Net::DNS::Resolver->new;
    if ($resolver) {
        my $mx = $resolver->send($host, 'MX');
        if ($mx) {
            my @mxlist = sort { $a->preference <=> $b->preference } (grep { $_->type eq 'MX' } $mx->answer);
            # discard the first entry (top priority) - we shouldn't get mail from them
            shift @mxlist;
            foreach my $rr (@mxlist) {
                my $a = $resolver->send($rr->exchange, 'A');
                if ($a) {
                    foreach my $rra ($a->answer) {
                        if ($rra->type eq 'A') {
                            if ($rra->address eq $addr) {
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }

    return undef;
}

=item C<< $query->message_result2() >>

  my ($result, $smtp_comment, $header_comment, $spf_record) = $query->message_result2();

C<message_result2()> returns an overall status for the message
after zero or more calls to C<result2()>. It will always be the last 
status returned by C<result2()>, or the status returned by C<result()> if
C<result2()> was never called.

C<$result> will be one of C<pass>, C<fail>, C<neutral [...]>, or C<error>.
See C<result()> above for meanings.

=cut

# ----------------------------------------------------------
#			    message_result2
# ----------------------------------------------------------

sub message_result2 {
  my $query = shift;

  if (!$query->{result2}) {
      if ($query->{is_mx_good}) {
          return @{$query->{is_mx_good}} if wantarray;
          return $query->{is_mx_good}->[0];
      }

      # we are very unlikely to get here -- unless result2 was not called.

      my @result = $query->result();

      $query->{result2} = \@result;
  }

  return @{$query->{result2}} if wantarray;
  return $query->{result2}->[0];
}

=item C<< $query->best_guess() >>

      my ($result, $smtp_comment, $header_comment) = $query->best_guess();

When a domain does not publish SPF records, this library can
produce an educated guess anyway.

It pretends the domain defined A, MX, and PTR mechanisms,
plus a few others.  The default set of directives is

  "a/24 mx/24 ptr"

That default set will return either "pass" or "neutral".

If you want to experiment with a different default, you can
pass it as an argument: C<< $query->best_guess("a mx ptr") >>

Note that this method is deprecated. You should set C<guess=E<gt>1>
on the C<new> method instead.

=item C<< $query->trusted_forwarder() >>

      my ($result, $smtp_comment, $header_comment) = $query->best_guess();

It is possible that the message is coming through a
known-good relay like acm.org or pobox.com.  During the
transitional period, many legitimate services may appear to
forge a sender address: for example, a news website may have
a "send me this article in email" link.

The trusted-forwarder.org domain is a whitelist of
known-good hosts that either forward mail or perform
legitimate envelope sender forgery.

  "include:spf.trusted-forwarder.org"

This will return either "pass" or "neutral".

Note that this method is deprecated. You should set C<trusted=E<gt>1>
on the C<new> method instead.


=cut

sub clone {
  my $query = shift;
  my $class = ref $query;

  my %guts = (%$query, @_, parent=>$query);

  my $clone = bless \%guts, $class;

  push @{$clone->{loop_report}}, delete $clone->{reason};

  $query->debuglog("  clone: new object:");
  for ($clone->show) { $clone->debuglog( "clone: $_" ) }

  return $clone;
}

sub top {
  my $query = shift;
  if ($query->{parent}) { return $query->{parent}->top }
  return $query;
}

sub set_temperror {
  my $query = shift;
  $query->{error} = shift;
}

sub show {
  my $query = shift;

  return map { sprintf ("%20s = %s", $_, $query->{$_}) } keys %$query;
}

sub best_guess {
  my $query = shift;
  my $guess_mechs = shift || $GUESS_MECHS;

  # clone the query object with best_guess mode turned on.
  my $guess_query = $query->clone( best_guess => 1,
				   guess_mechs => $guess_mechs,
				   reason => "has no data.  best guess",
				 );

  $guess_query->{depth} = 0;
  $guess_query->top->{lookup_count} = 0;

  # if result is not defined, the domain has no SPF.
  #    perform fallback lookups.
  #    perform trusted-forwarder lookups.
  #    perform guess lookups.
  #
  # if result is defined, return it.

  my ($result, $smtp_comment, $header_comment) = $guess_query->result();
  if (defined $result and $result eq "pass") {
    my $ip = $query->ip;
    $header_comment = $query->sanitize("seems reasonable for $query->{sender} to mail through $ip");
    return ($result, $smtp_comment, $header_comment) if wantarray;
    return $result;
  }

  return $query->sanitize("neutral");
}

sub trusted_forwarder {
  my $query = shift;
  my $guess_mechs = shift || $TRUSTED_FORWARDER;
  return $query->best_guess($guess_mechs);
}

# ----------------------------------------------------------

=item C<< $query->sanitize('string') >>

This applies the sanitization rules for the particular query
object. These rules are controlled by the C<sanitize> parameter
to the Mail::SPF::Query new method.

=cut

sub sanitize {
  my $query = shift;
  my $txt = shift;

  if (ref($query->{sanitize})) {
      $txt = $query->{sanitize}->($txt);
  }

  return $txt;
}

# ----------------------------------------------------------

=item C<< strict_sanitize('string') >>

This ensures that all the characters in the returned string are printable.
All whitespace is converted into spaces, and all other non-printable
characters are converted into question marks. This is probably
over aggressive for many applications.

This function is used by default when the C<sanitize> option is passed to
the new method of Mail::SPF::Query.

Note that this function is not a class method.

=cut

sub strict_sanitize {
  my $txt = shift;

  $txt =~ s/\s/ /g;
  $txt =~ s/[^[:print:]]/?/g;

  return $txt;
}

# ----------------------------------------------------------

=item C<< $query->debuglog() >>

Subclasses may override this with their own debug logger.
I recommend Log::Dispatch.

Alternatively, pass the C<new()> constructor a
C<debuglog => sub { ... }> callback, and we'll pass
debugging lines to that.

=cut

sub debuglog {
  my $self = shift;
  return if ref $self and not $self->{debug};
  
  my $toprint = join (" ", @_);
  chomp $toprint;
  $toprint = sprintf ("%-8s %s %s %s",
		      ("|" x ($self->{depth}+1)),
		      $self->{localpart},
		      $self->{domain},
		      $toprint);

  if (exists $self->{debuglog} and ref $self->{debuglog} eq "CODE") { eval { $self->{debuglog}->($toprint) } ; }
  else { printf STDERR "%s", "$toprint\n"; }
}

# ----------------------------------------------------------
#			    spfquery
# ----------------------------------------------------------

sub spfquery {
  #
  # usage: my ($result, $explanation, $text, $time) = $query->spfquery( [ GUESS_MECHS ] )
  #
  #  performs a full SPF resolution using the data in $query.  to use different data, clone the object.
  #
  #  if GUESS_MECHS is present, we are operating in "guess" mode so we will not actually query the domain for TXT; we will use the guess_mechs instead.
  #
  my $query = shift;
  my $guess_mechs = shift;

  if ($query->{ipv4} and
      $query->{ipv4}=~ /^127\./) { return "pass", "localhost is always allowed." }

  $query->top->{lookup_count}++;

  if ($query->is_looping)            { return "unknown", $query->{spf_error_explanation}, $query->is_looping }
  if ($query->can_use_cached_result) { return $query->cached_result; }
  else                               { $query->tell_cache_that_lookup_is_underway; }

  my $directive_set = DirectiveSet->new($query->{domain}, $query, $guess_mechs, $query->{local}, $query->{default_record});

  if (not defined $directive_set) {
    $query->debuglog("no SPF record found for $query->{domain}");
    $query->delete_cache_point;
    if ($query->{domain} ne $query->{orig_domain}) {
        if ($query->{error}) {
            return "error", $query->{spf_error_explanation}, $query->{error};
        }
        return "unknown", $query->{spf_error_explanation}, "Missing SPF record at $query->{domain}";
    }
    if ($query->{last_dns_error} eq 'NXDOMAIN') {
        my $explanation = $query->macro_substitute($query->{default_explanation});
        return "fail", $explanation, "domain of sender $query->{sender} does not exist";
    }
    return "none", "SPF", "domain of sender $query->{sender} does not designate mailers";
  }

  if ($directive_set->{hard_syntax_error}) {
    $query->debuglog("  syntax error while parsing $directive_set->{txt}");
    $query->delete_cache_point;
    return "unknown", $query->{spf_error_explanation}, $directive_set->{hard_syntax_error};
  }

  $query->{directive_set} = $directive_set;

  foreach my $mechanism ($directive_set->mechanisms) {
    my ($result, $comment) = $query->evaluate_mechanism($mechanism);

    if ($query->{error}) {
      $query->debuglog("  returning temporary error: $query->{error}");
      $query->delete_cache_point;
      return "error", $query->{spf_error_explanation}, $query->{error};
    }

    if (defined $result) {
      $query->debuglog("  saving result $result to cache point and returning.");
      my $explanation = $query->interpolate_explanation(
            ($result =~ /^unknown/)
            ? $query->{spf_error_explanation} : $query->{default_explanation});
      $query->save_result_to_cache($result,
				   $explanation,
				   $comment,
				   $query->{directive_set}->{orig_txt});
      return $result, $explanation, $comment, $query->{directive_set}->{orig_txt};
    }
  }

  # run the redirect modifier
  if ($query->{directive_set}->redirect) {
    my $new_domain = $query->macro_substitute($query->{directive_set}->redirect);

    $query->debuglog("  executing redirect=$new_domain");

    my $inner_query = $query->clone(domain => $new_domain,
				    depth  => $query->{depth} + 1,
				    reason => "redirects to $new_domain",
				   );

    my @inner_result = $inner_query->spfquery();

    $query->delete_cache_point;

    $query->debuglog("  executed redirect=$new_domain, got result @inner_result");

    $query->{spf_source} = $inner_query->{spf_source};

    return @inner_result;
  }

  $query->debuglog("  no mechanisms matched; deleting cache point and using neutral");
  $query->delete_cache_point;
  return "neutral", $query->interpolate_explanation($query->{default_explanation}), $directive_set->{soft_syntax_error};
}

# ----------------------------------------------------------
# 	      we cache into $Domains_Queried.
# ----------------------------------------------------------

sub cache_point {
  my $query = shift;
  return my $cache_point = join "/", ($query->{best_guess}  || 0,
				      $query->{guess_mechs} || "",
				      $query->{ipv4},
				      $query->{localpart},
				      $query->{domain},
				      $query->{default_record},
				      $query->{local});
}

sub is_looping {
  my $query = shift;
  my $cache_point = $query->cache_point;
  return (join " ", "loop encountered:", @{$query->{loop_report}})
    if (exists $Domains_Queried->{$cache_point}
	and
	not defined $Domains_Queried->{$cache_point}->[0]);

  return (join " ", "exceeded maximum recursion depth:", @{$query->{loop_report}})
    if ($query->{depth} >= $query->max_lookup_count);

  return ("query caused more than " . $query->max_lookup_count . " lookups") if ($query->max_lookup_count 
										 and
										 $query->top->{lookup_count} > $query->max_lookup_count);

  return 0;
}

sub max_lookup_count {
  my $query = shift;
  return $query->{max_lookup_count} || $MAX_LOOKUP_COUNT;
}

sub can_use_cached_result {
  my $query = shift;
  my $cache_point = $query->cache_point;

  if ($Domains_Queried->{$cache_point}) {
    $query->debuglog("  lookup: we have already processed $query->{domain} before with $query->{ipv4}.");
    my @cached = @{ $Domains_Queried->{$cache_point} };
    if (not defined $CACHE_TIMEOUT
	or time - $cached[-1] > $CACHE_TIMEOUT) {
      $query->debuglog("  lookup: but its cache entry is stale; deleting it.");
      delete $Domains_Queried->{$cache_point};
      return 0;
    }

    $query->debuglog("  lookup: the cache entry is fresh; returning it.");
    return 1;
  }
  return 0;
}

sub tell_cache_that_lookup_is_underway {
  my $query = shift;

  # define an entry here so we don't loop endlessly in an Include loop.
  $Domains_Queried->{$query->cache_point} = [undef, undef, undef, undef, time];
}

sub save_result_to_cache {
  my $query = shift;
  my ($result, $explanation, $comment, $orig_txt) = (shift, shift, shift, shift);

  # define an entry here so we don't loop endlessly in an Include loop.
  $Domains_Queried->{$query->cache_point} = [$result, $explanation, $comment, $orig_txt, time];
}

sub cached_result {
  my $query = shift;
  my $cache_point = $query->cache_point;

  if ($Domains_Queried->{$cache_point}) {
    return @{ $Domains_Queried->{$cache_point} };
  }
  return;
}

sub delete_cache_point {
  my $query = shift;
  delete $Domains_Queried->{$query->cache_point};
}

sub clear_cache {
  $Domains_Queried = {};
}

sub get_ptr_domain {
    my ($query) = shift;

    return $query->{ptr_domain} if ($query->{ptr_domain});
    
    foreach my $ptrdname ($query->myquery(reverse_in_addr($query->{ipv4}) . ".in-addr.arpa", "PTR", "ptrdname")) {
        $query->debuglog("  get_ptr_domain: $query->{ipv4} is $ptrdname");
    
        $query->debuglog("  get_ptr_domain: checking hostname $ptrdname for legitimacy.");
    
        # check for legitimacy --- PTR -> hostname A -> PTR
        foreach my $ptr_to_a ($query->myquery($ptrdname, "A", "address")) {
          
            $query->debuglog("  get_ptr_domain: hostname $ptrdname -> $ptr_to_a");
      
            if ($ptr_to_a eq $query->{ipv4}) {
                return $query->{ptr_domain} = $ptrdname;
            }
        }
    }

    return undef;
}

sub macro_substitute_item {
    my $query = shift;
    my $arg = shift;

    if ($arg eq "%") { return "%" }
    if ($arg eq "_") { return " " }
    if ($arg eq "-") { return "%20" }

    $arg =~ s/^{(.*)}$/$1/;

    my ($field, $num, $reverse, $delim) = $arg =~ /^(x?\w)(\d*)(r?)(.*)$/;

    $delim = '.' if not length $delim;

    my $newval = $arg;
    my $timestamp = time;

    $newval = $query->{localpart}       if (lc $field eq 'u');
    $newval = $query->{localpart}       if (lc $field eq 'l');
    $newval = $query->{domain}          if (lc $field eq 'd');
    $newval = $query->{sender}          if (lc $field eq 's');
    $newval = $query->{orig_domain}     if (lc $field eq 'o');
    $newval = $query->ip                if (lc $field eq 'i');
    $newval = $timestamp                if (lc $field eq 't');
    $newval = $query->{helo}            if (lc $field eq 'h');
    $newval = $query->get_ptr_domain    if (lc $field eq 'p');
    $newval = $query->{myhostname}      if (lc $field eq 'xr');  # only used in explanation
    $newval = $query->{ipv4} ? 'in-addr' : 'ip6'
                                        if (lc $field eq 'v');

    # We need to escape a bunch of characters inside a character class
    $delim =~ s/([\^\-\]\:\\])/\\$1/g;

    if ($reverse || $num) {
        my @parts = split /[$delim]/, $newval;

        @parts = reverse @parts if ($reverse);

        if ($num) {
            while (@parts > $num) { shift @parts }
        }

        $newval = join ".", @parts;
    }

    $newval = uri_escape($newval)       if ($field ne lc $field);

    $query->debuglog("  macro_substitute_item: $arg: field=$field, num=$num, reverse=$reverse, delim=$delim, newval=$newval");

    return $newval;
}

sub macro_substitute {
    my $query = shift;
    my $arg = shift;
    my $maxlen = shift;

    my $original = $arg;

#      macro-char   = ( '%{' alpha *digit [ 'r' ] *delim '}' )
#                     / '%%'
#                     / '%_'
#                     / '%-'

    $arg =~ s/%([%_-]|{(\w[^}]*)})/$query->macro_substitute_item($1)/ge;

    if ($maxlen && length $arg > $maxlen) {
      $arg = substr($arg, -$maxlen);  # super.long.string -> er.long.string
      $arg =~ s/[^.]*\.//;            #    er.long.string ->    long.string
    }
    $query->debuglog("  macro_substitute: $original -> $arg") if ($original ne $arg);
    return $arg;
}

# ----------------------------------------------------------
#		     evaluate_mechanism
# ----------------------------------------------------------

sub evaluate_mechanism {
  my $query = shift;
  my ($modifier, $mechanism, $argument, $source) = @{shift()};

  $modifier = "+" if not length $modifier;

  $query->debuglog("  evaluate_mechanism: $modifier$mechanism($argument) for domain=$query->{domain}");

  if ({ map { $_=>1 } @KNOWN_MECHANISMS }->{$mechanism}) {
    my $mech_sub = "mech_$mechanism";
    my ($hit, $text) = $query->$mech_sub($query->macro_substitute($argument, 255));
    no warnings 'uninitialized';
    $query->debuglog("  evaluate_mechanism: $modifier$mechanism($argument) returned $hit $text");

    return if not $hit;

    return ($hit, $text) if ($hit ne "hit");

    $query->{spf_source} = $source if ($source);

    return $query->shorthand2value($modifier), $text;
  }
  else {
    my $unrecognized_mechanism = join ("",
				       ($modifier eq "+" ? "" : $modifier),
				       $mechanism,
				       ($argument ? ":" : ""),
				       $argument);
    my $error_string = "unknown $unrecognized_mechanism";
    $query->debuglog("  evaluate_mechanism: unrecognized mechanism $unrecognized_mechanism, returning $error_string");
    return $error_string => "unrecognized mechanism $unrecognized_mechanism";
  }

  return ("neutral", "evaluate-mechanism: neutral");
}

# ----------------------------------------------------------
# 	     myquery wraps DNS resolver queries
#
# ----------------------------------------------------------

sub myquery {
  my $query = shift;
  my $label = shift;
  my $qtype = shift;
  my $method = shift;
  my $sortby = shift;

  $query->debuglog("  myquery: doing $qtype query on $label");

  for ($label) {
    if (/\.\./ or /^\./) {
      # convert .foo..com to foo.com, etc.
      $query->debuglog("  myquery: fixing up invalid syntax in $label");
      s/\.\.+/\./g;
      s/^\.//;
      $query->debuglog("  myquery: corrected label is $label");
    }
  }
  my $resquery = $query->resolver->query($label, $qtype);

  my $errorstring = $query->resolver->errorstring;
  if (not $resquery and $errorstring eq "NOERROR") {
    return;
  }

  $query->{last_dns_error} = $errorstring;

  if (not $resquery) {
    if ($errorstring eq "NXDOMAIN") {
      $query->debuglog("  myquery: $label $qtype failed: NXDOMAIN.");
      return;
    }

    $query->debuglog("  myquery: $label $qtype lookup error: $errorstring");
    $query->debuglog("  myquery: will set error condition.");
    $query->set_temperror("DNS error while looking up $label $qtype: $errorstring");
    return;
  }

  my @answers = grep { lc $_->type eq lc $qtype } $resquery->answer;

  # $query->debuglog("  myquery: found $qtype response: @answers");

  my @toreturn;
  if ($sortby) { @toreturn = map { rr_method($_,$method) } sort { $a->$sortby() <=> $b->$sortby() } @answers; }
  else         { @toreturn = map { rr_method($_,$method) }                                          @answers; }

  if (not @toreturn) {
    $query->debuglog("  myquery: result had no data.");
    return;
  }

  return @toreturn;
}

sub rr_method {
  my ($answer, $method) = @_;
  if ($method ne "char_str_list") { return $answer->$method() }

  # long TXT records can't be had with txtdata; they need to be pulled out with char_str_list which returns a list of strings
  # that need to be joined.

  my @char_str_list = $answer->$method();
  # print "rr_method returning join of @char_str_list\n";

  return join "", @char_str_list;
}

#
# Mechanisms return one of the following:
#
# hit
#       mechanism matched
# undef
#       mechanism did not match
#
# unknown
#       some error happened during processing
# error
#       some temporary error
#
# ----------------------------------------------------------
# 			    all
# ----------------------------------------------------------

sub mech_all {
  my $query = shift;
  return "hit" => "default";
}

# ----------------------------------------------------------
#			  include
# ----------------------------------------------------------

sub mech_include {
  my $query = shift;
  my $argument = shift;

  if (not $argument) {
    $query->debuglog("  mechanism include: no argument given.");
    return "unknown", "include mechanism not given an argument";
  }

  $query->debuglog("  mechanism include: recursing into $argument");

  my $inner_query = $query->clone(domain => $argument,
				  depth  => $query->{depth} + 1,
				  reason => "includes $argument",
                                  local => undef,
                                  trusted => undef,
                                  guess => undef,
                                  default_record => undef,
				 );

  my ($result, $explanation, $text, $orig_txt, $time) = $inner_query->spfquery();

  $query->debuglog("  mechanism include: got back result $result / $text / $time");

  if ($result eq "pass")            { return hit     => $text, $time; }
  if ($result eq "error")           { return $result => $text, $time; }
  if ($result eq "unknown")         { return $result => $text, $time; }
  if ($result eq "none")            { return unknown => $text, $time; } # fail-safe mode.  convert an included NONE into an UNKNOWN error.
  if ($result eq "fail" ||
      $result eq "neutral" ||
      $result eq "softfail")        { return undef,     $text, $time; }
  
  $query->debuglog("  mechanism include: reducing result $result to unknown");
  return "unknown", $text, $time;
}

# ----------------------------------------------------------
# 			     a
# ----------------------------------------------------------

sub mech_a {
  my $query = shift;
  my $argument = shift;
  
  my $ip4_cidr_length = ($argument =~ s/  \/(\d+)//x) ? $1 : 32;
  my $ip6_cidr_length = ($argument =~ s/\/\/(\d+)//x) ? $1 : 128;

  my $domain_to_use = $argument || $query->{domain};

  # see code below in ip4
  foreach my $a ($query->myquery($domain_to_use, "A", "address")) {
    $query->debuglog("  mechanism a: $a");
    if ($a eq $query->{ipv4}) {
      $query->debuglog("  mechanism a: match found: $domain_to_use A $a == $query->{ipv4}");
      return "hit", "$domain_to_use A $query->{ipv4}";
    }
    elsif ($ip4_cidr_length < 32) {
      my $cidr = Net::CIDR::Lite->new("$a/$ip4_cidr_length");

      $query->debuglog("  mechanism a: looking for $query->{ipv4} in $a/$ip4_cidr_length");
      
      return (hit => "$domain_to_use A $a /$ip4_cidr_length contains $query->{ipv4}")
	if $cidr->find($query->{ipv4});
    }
  }
  return;
}

# ----------------------------------------------------------
# 			     mx
# ----------------------------------------------------------

sub mech_mx {
  my $query = shift;
  my $argument = shift;

  my $ip4_cidr_length = ($argument =~ s/  \/(\d+)//x) ? $1 : 32;
  my $ip6_cidr_length = ($argument =~ s/\/\/(\d+)//x) ? $1 : 128;

  my $domain_to_use = $argument || $query->{domain};

  my @mxes = $query->myquery($domain_to_use, "MX", "exchange", "preference");

  # if a domain has no MX record, we MUST NOT use its IP address instead.
  # if (! @mxes) {
  #   $query->debuglog("  mechanism mx: no MX found for $domain_to_use.  Will pretend it is its own MX, and test its IP address.");
  #   @mxes = ($domain_to_use);
  # }

  foreach my $mx (@mxes) {
    # $query->debuglog("  mechanism mx: $mx");

    foreach my $a ($query->myquery($mx, "A", "address")) {
      if ($a eq $query->{ipv4}) {
	$query->debuglog("  mechanism mx: we have a match; $domain_to_use MX $mx A $a == $query->{ipv4}");
	return "hit", "$domain_to_use MX $mx A $a";
      }
      elsif ($ip4_cidr_length < 32) {
	my $cidr = Net::CIDR::Lite->new("$a/$ip4_cidr_length");

	$query->debuglog("  mechanism mx: looking for $query->{ipv4} in $a/$ip4_cidr_length");

	return (hit => "$domain_to_use MX $mx A $a /$ip4_cidr_length contains $query->{ipv4}")
	  if $cidr->find($query->{ipv4});

      }
    }
  }
  return;
}

# ----------------------------------------------------------
# 			    ptr
# ----------------------------------------------------------

sub mech_ptr {
  my $query = shift;
  my $argument = shift;

  if ($query->{ipv6}) { return "neutral", "ipv6 not yet supported"; }

  my $domain_to_use = $argument || $query->{domain};

  foreach my $ptrdname ($query->myquery(reverse_in_addr($query->{ipv4}) . ".in-addr.arpa", "PTR", "ptrdname")) {
    $query->debuglog("  mechanism ptr: $query->{ipv4} is $ptrdname");
    
    $query->debuglog("  mechanism ptr: checking hostname $ptrdname for legitimacy.");
    
    # check for legitimacy --- PTR -> hostname A -> PTR
    foreach my $ptr_to_a ($query->myquery($ptrdname, "A", "address")) {
      
      $query->debuglog("  mechanism ptr: hostname $ptrdname -> $ptr_to_a");
      
      if ($ptr_to_a eq $query->{ipv4}) {
	$query->debuglog("  mechanism ptr: we have a valid PTR: $query->{ipv4} PTR $ptrdname A $ptr_to_a");
	$query->debuglog("  mechanism ptr: now we see if $ptrdname ends in $domain_to_use.");
	
	if ($ptrdname =~ /(^|\.)\Q$domain_to_use\E$/i) {
	  $query->debuglog("  mechanism ptr: $query->{ipv4} PTR $ptrdname does end in $domain_to_use.");
	  return hit => "$query->{ipv4} PTR $ptrdname matches $domain_to_use";
	}
	else {
	  $query->debuglog("  mechanism ptr: $ptrdname does not end in $domain_to_use.  no match.");
	}
      }
    }
  }
  return;
}

# ----------------------------------------------------------
# 			     exists
# ----------------------------------------------------------

sub mech_exists {
  my $query = shift;
  my $argument = shift;

  return if (!$argument);

  my $domain_to_use = $argument;

  $query->debuglog("  mechanism exists: looking up $domain_to_use");
  
  foreach ($query->myquery($domain_to_use, "A", "address")) {
    $query->debuglog("  mechanism exists: $_");
    $query->debuglog("  mechanism exists: we have a match.");
    my @txt = map { s/^"//; s/"$//; $_ } $query->myquery($domain_to_use, "TXT", "char_str_list");
    if (@txt) {
        return hit => join(" ", @txt);
    }
    return hit => "$domain_to_use found";
  }
  return;
}

# ----------------------------------------------------------
# 			    ip4
# ----------------------------------------------------------

sub mech_ip4 {
  my $query = shift;
  my $cidr_spec = shift;

  return if not length $cidr_spec;

  my ($network, $cidr_length) = split (/\//, $cidr_spec, 2);

  my $dot_count = $network =~ tr/././;
  
  # turn "1.2.3/24" into "1.2.3.0/24"
  for (1 .. (3 - $dot_count)) { $network .= ".0"; }

  # TODO: add library compatibility test for ill-formed ip4 syntax
  if ($network !~ /^\d+\.\d+\.\d+\.\d+$/) { return ("unknown" => "bad argument to ip4: $cidr_spec"); }
  
  $cidr_length = "32" if not defined $cidr_length;

  my $cidr = eval { Net::CIDR::Lite->new("$network/$cidr_length") }; # TODO: make this work for ipv6 as well
  if ($@) { return ("unknown" => "unable to parse ip4:$cidr_spec"); }

  $query->debuglog("  mechanism ip4: looking for $query->{ipv4} in $cidr_spec");

  return (hit => "$cidr_spec contains $query->{ipv4}") if $cidr->find($query->{ipv4});

  return;
}

# ----------------------------------------------------------
# 			    ip6
# ----------------------------------------------------------

sub mech_ip6 {
  my $query = shift;

  return;
}

# ----------------------------------------------------------
# 			 functions
# ----------------------------------------------------------

sub ip { # accessor
  my $query = shift;
  return $query->{ipv4} || $query->{ipv6};
}

sub reverse_in_addr {
  return join (".", (reverse split /\./, shift));
}

sub resolver {
  my $query = shift;
  return $query->{res} ||= Net::DNS::Resolver->new;
}

sub fallbacks {
  my $query = shift;
  return @{$query->{fallbacks}};
}

sub shorthand2value {
  my $query = shift;
  my $shorthand = shift;
  return { "-" => "fail",
	   "+" => "pass",
	   "~" => "softfail",
	   "?" => "neutral" } -> {$shorthand} || $shorthand;
}

sub value2shorthand {
  my $query = shift;
  my $value = lc shift;
  return { "fail"     => "-",
	   "pass"     => "+",
	   "softfail" => "~",
	   "deny"     => "-",
	   "allow"    => "+",
	   "softdeny" => "~",
	   "unknown"  => "?",
	   "neutral"  => "?" } -> {$value} || $value;
}

sub interpolate_explanation {
  my $query = shift;
  my $txt = shift;

  if ($query->{directive_set}->explanation) {
    my @txt = map { s/^"//; s/"$//; $_ } $query->myquery($query->macro_substitute($query->{directive_set}->explanation), "TXT", "char_str_list");
    $txt = join " ", @txt;
  }

  return $query->macro_substitute($txt);
}

sub find_ancestor {
  my $query = shift;
  my $which_hash = shift;
  my $current_domain = shift;

  return if not exists $query->{$which_hash};

  $current_domain =~ s/\.$//g;
  my @current_domain = split /\./, $current_domain;

  foreach my $ancestor_level (0 .. @current_domain) {
    my @ancestor = @current_domain;
    for (1 .. $ancestor_level) { shift @ancestor }
    my $ancestor = join ".", @ancestor;

    for my $match ($ancestor_level > 0 ? "*.$ancestor" : $ancestor) {
      $query->debuglog("  DirectiveSet $which_hash: is $match in the $which_hash hash?");
      if (my $found = $query->{$which_hash}->{lc $match}) {
	$query->debuglog("  DirectiveSet $which_hash: yes, it is.");
	return wantarray ? ($which_hash, $match, $found) : $found;
      }
    }
  }
  return;
}

sub found_record_for {
  my $query = shift;
  my ($which_hash, $matched_domain_glob, $found) = $query->find_ancestor(@_);
  return if not $found;
  my $txt = $found->{record};
  $query->{spf_source} = "explicit $which_hash found: $matched_domain_glob defines $txt";
  $txt = "v=spf1 $txt" if $txt !~ /^v=spf1\b/i;
  return $txt;
}

sub try_override {
  my $query = shift;
  return $query->found_record_for("override", @_);
}

sub try_fallback {
  my $query = shift;
  return $query->found_record_for("fallback", @_);
}

sub callerid_wanted_for {
  my $query = shift;
  my ($which_hash, $matched_domain_glob, $found) = $query->find_ancestor("callerid" => @_);

  if (not $found) {
    $query->debuglog("  callerid_wanted_for(@_) did not return a match in the callerid config hash.");
    return;
  }
  my $check = $found->{check};
  $query->debuglog("  callerid_wanted_for: callerid config defines check=$check for $matched_domain_glob");
  return $check;
}

# ----------------------------------------------------------
# 		      algo
# ----------------------------------------------------------

{
  package DirectiveSet;

  sub new {
    my $class = shift;
    my $current_domain = shift;
    my $query = shift;
    my $override_text = shift;
    my $localpolicy = shift;
    my $default_record = shift;

    my $txt;

    # overrides can come from two places:
    #  1 - when operating in best_guess mode, spfquery may be called with a ($guess_mechs) argument, which comes in as $override_text.
    #  2 - when operating with ->new(..., override => { ... }) we need to load the override dynamically.

    if (not $override_text
	and
	exists $query->{override}
       ) {
      $txt = $query->try_override($current_domain);
    }

    if ($override_text) {
      $txt = "v=spf1 $override_text ?all";
      $query->{spf_source} = "local policy";
    }
    else {
      my @txt;

      if ($current_domain !~ /^_ep\./) {
	$query->debuglog("  DirectiveSet->new(): doing TXT query on $current_domain");
	@txt = $query->myquery($current_domain, "TXT", "char_str_list");
	$query->debuglog("  DirectiveSet->new(): TXT query on $current_domain returned error=$query->{error}, last_dns_error=$query->{last_dns_error}");

	if ($query->{error} || $query->{last_dns_error} eq 'NXDOMAIN' || ! @txt) {
	  # try the fallbacks.
	  $query->debuglog("  DirectiveSet->new(): will try fallbacks.");
	  if (exists $query->{fallback}
	      and
	      my $found_txt = $query->try_fallback($current_domain, "fallback")) {
	    @txt = $found_txt;
	  }
	  else {
	    $query->debuglog("  DirectiveSet->new(): fallback search failed.");
	  }
	}
      }

      if (not @txt
	  and
	  $query->callerid_wanted_for($current_domain)
	 ) {

	eval { require LMAP::CID2SPF; };
	if ($@) { 
	  $query->debuglog("  DirectiveSet->new(): LMAP::CID2SPF not available, will not do Caller-ID lookup.");
	}
	else {
	  my @errors_before_ep = ($query->{error}, $query->{last_dns_error});
	  my $ep_version = "_ep.$current_domain"; $ep_version =~ s/^_ep\._ep/_ep/i;
	  $query->debuglog("  DirectiveSet->new(): doing TXT query on $ep_version");
	  my @eptxt = $query->myquery($ep_version, "TXT", "char_str_list");
	  $query->debuglog("  DirectiveSet->new(): TXT query on $current_domain returned error=$query->{error}, last_dns_error=$query->{last_dns_error}");

	  if (@eptxt) {
	    my $xml = join "", @eptxt;
	    
	    # "<ep xmlns='http://ms.net/1'>...</ep>"
	    if ($xml =~ m(^<ep xmlns='http://ms.net/1')) {
	      my $c2s = LMAP::CID2SPF->new();
	      $c2s->cid($xml);
	      my $spf = $c2s->convert();
	      $query->debuglog("  CID2SPF:  in: $xml");
	      $query->debuglog("  CID2SPF: out: $spf");
	      $query->{spf_source} = "Microsoft Caller-ID for Email record at $ep_version";
	      @txt = $spf;
	    }
	  }
	  else {
	    $query->debuglog("  restoring error from @errors_before_ep; had become previously $query->{error}");
	    ($query->{error}, $query->{last_dns_error}) = @errors_before_ep;
	  }
	}
      }

      # squish multiline responses into one first.
      foreach (@txt) {
	s/^"(.*)"$/$1/;
	s/^\s+//;
	s/\s+$//;
	
	if (/^v=spf1(\s.*|)$/i) {
	  $txt .= $1;
	}
      }

      if (!defined $txt && $default_record) {
          $txt = "v=spf1 $default_record ?all";
          $query->{spf_source} = "local policy";
      }
    }

    $query->debuglog("  DirectiveSet->new(): SPF policy: $txt");

    return if not defined $txt;

    # TODO: the prepending of the v=spf1 is a massive hack; get it right by saving the actual raw orig_txt.
    my $directive_set = bless { orig_txt => ($txt =~ /^v=spf1/ ? $txt : "v=spf1$txt"), txt => $txt } , $class;

    TXT_RESPONSE:
    for ($txt) {
      $query->debuglog("  lookup:   TXT $_");

      # parse the policy record
      
      while (/\S/) {
	s/^\s*(\S+)\s*//;
	my $word = $1;
	# $query->debuglog("  lookup:  word parsing word $word");
	if ($word =~ /^v=(\S+)/i) {
	  my $version = $1;
	  $query->debuglog("  lookup:   TXT version=$version");
	  $directive_set->{version} = $version;
	  next TXT_RESPONSE if ($version ne "spf1");
	  next;
	}

	# modifiers always have an = sign.
	if (my ($lhs, $rhs) = $word =~ /^([^:\/]+)=(\S*)$/) {
	  # $query->debuglog("  lookup:   TXT modifier found: $lhs = $rhs");

	  # if we ever come to support multiple of the same modifier, we need to make this a list.
	  $directive_set->{modifiers}->{lc $lhs} = $rhs;
	  next;
	}

	# RHS optional, defaults to domain.
	# [:/] matches a:foo and a/24
	if (my ($prefix, $lhs, $rhs) = $word =~ /^([-~+?]?)([\w_-]+)([\/:]\S*)?$/i) {
	  $rhs =~ s/^://;
	  $prefix ||= "+";
	  $query->debuglog("  lookup:   TXT prefix=$prefix, lhs=$lhs, rhs=$rhs");
	  push @{$directive_set->{mechanisms}}, [$prefix => lc $lhs => $rhs];
	  next;
	}

      }
    }

    if (my $rhs = delete $directive_set->{modifiers}->{default}) {
      push @{$directive_set->{mechanisms}}, [ $query->value2shorthand($rhs), all => undef ];
    }

    $directive_set->{mechanisms} = []           if not $directive_set->{mechanisms};
    if ($localpolicy) {
        my $mechanisms = $directive_set->{mechanisms};
        my $lastmech = $mechanisms->[$#$mechanisms];
        if (($lastmech->[0] eq '-' || $lastmech->[0] eq '?') &&
             $lastmech->[1] eq 'all') {
            my $index;

            for ($index = $#$mechanisms - 1; $index >= 0; $index--) {
                last if ($lastmech->[0] ne $mechanisms->[$index]->[0]);
            }
            if ($index >= 0) {
                # We want to insert the localpolicy just *after* $index
                $query->debuglog("  inserting local policy mechanisms into @{[$directive_set->show_mechanisms]} after position $index");
                my $localset = DirectiveSet->new($current_domain, $query->clone, $localpolicy);

                if ($localset) {
                    my @locallist = $localset->mechanisms;
                    # Get rid of the ?all at the end of the list
                    pop @locallist;
                    map { $_->[3] = $_->[1] eq 'include' ? "SPF record at " . $query->macro_substitute($_->[2]) : "local policy" } @locallist;
                    splice(@$mechanisms, $index + 1, 0, @locallist);
                }
            }
        }
    }
    $query->debuglog("  lookup:  mec mechanisms=@{[$directive_set->show_mechanisms]}");
    return $directive_set;
  }

  sub version      {   shift->{version}      }
  sub mechanisms   { @{shift->{mechanisms}}  }
  sub explanation  {   shift->{modifiers}->{exp}      }
  sub redirect     {   shift->{modifiers}->{redirect} }
  sub get_modifier {   shift->{modifiers}->{shift()}  }
  sub syntax_error {   shift->{syntax_error} }

  sub show_mechanisms   {
    my $directive_set = shift;
    return map { $_->[0] . $_->[1] . "(" . ($_->[2]||"") . ")" } $directive_set->mechanisms;
  }
}

1;

=item EXPORT

None by default.

=back

=head1 WARNINGS

Mail::Query::SPF should only be used at the point where messages are received from the Internet.
The underlying assumption is that the sender of the email is sending the message directly to you
or one of your secondaries. If your MTA does not have an exhaustive list of secondaries, then
the C<result2()> and C<message_result2()> methods should be used. These methods take care to
permit mail from secondaries.

=head1 AUTHORS

Meng Weng Wong, <mengwong+spf@pobox.com>

Philip Gladstone

=head1 SEE ALSO

http://spf.pobox.com/

=cut

