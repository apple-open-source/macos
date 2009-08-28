use strict;
use warnings;

use Test::More tests => 144;

use File::Spec;
use File::Temp qw( tempdir );
use Log::Dispatch;


my %tests;
BEGIN
{
    foreach ( qw( MailSend MIMELite MailSendmail MailSender ) )
    {
        eval "use Log::Dispatch::Email::$_";
        $tests{$_} = ! $@;
    }

    eval "use Log::Dispatch::Syslog";
    $tests{Syslog} = ! $@;
}

my %TestConfig;
if ( -d '.svn' )
{
    %TestConfig = ( email_address => 'autarch@urth.org',
                    syslog_file   => '/var/log/messages',
                  );
}

use Log::Dispatch::File;
use Log::Dispatch::Handle;
use Log::Dispatch::Null;
use Log::Dispatch::Screen;

use IO::File;

my $tempdir = tempdir( CLEANUP => 1 );

my $dispatch = Log::Dispatch->new;
ok( $dispatch, "created Log::Dispatch object" );

# Test Log::Dispatch::File
{
    my $emerg_log = File::Spec->catdir( $tempdir, 'emerg.log' );

    $dispatch->add( Log::Dispatch::File->new( name => 'file1',
                                              min_level => 'emerg',
                                              filename => $emerg_log ) );

    $dispatch->log( level => 'info', message => "info level 1\n" );
    $dispatch->log( level => 'emerg', message => "emerg level 1\n" );

    my $debug_log = File::Spec->catdir( $tempdir, 'debug.log' );

    $dispatch->add( Log::Dispatch::File->new( name => 'file2',
                                              min_level => 'debug',
                                              filename => $debug_log ) );

    $dispatch->log( level => 'info', message => "info level 2\n" );
    $dispatch->log( level => 'emerg', message => "emerg level 2\n" );

    # This'll close them filehandles!
    undef $dispatch;

    open my $emerg_fh, '<', $emerg_log
        or die "Can't read $emerg_log: $!";
    open my $debug_fh, '<', $debug_log
        or die "Can't read $debug_log: $!";

    my @log = <$emerg_fh>;
    is( $log[0], "emerg level 1\n",
        "First line in log file set to level 'emerg' is 'emerg level 1'" );

    is( $log[1], "emerg level 2\n",
        "Second line in log file set to level 'emerg' is 'emerg level 2'" );

    @log = <$debug_fh>;
    is( $log[0], "info level 2\n",
        "First line in log file set to level 'debug' is 'info level 2'" );

    is( $log[1], "emerg level 2\n",
        "Second line in log file set to level 'debug' is 'emerg level 2'" );
}

# max_level test
{
    my $max_log = File::Spec->catfile( $tempdir, 'max.log' );

    my $dispatch = Log::Dispatch->new;
    $dispatch->add( Log::Dispatch::File->new( name => 'file1',
                                              min_level => 'debug',
                                              max_level => 'crit',
                                              filename => $max_log ) );

    $dispatch->log( level => 'emerg', message => "emergency\n" );
    $dispatch->log( level => 'crit',  message => "critical\n" );

    undef $dispatch; # close file handles

    open my $fh, '<', $max_log
        or die "Can't read $max_log: $!";
    my @log = <$fh>;

    is( $log[0], "critical\n",
        "First line in log file with a max level of 'crit' is 'critical'" );
}

# Log::Dispatch::Handle test
{
    my $handle_log = File::Spec->catfile( $tempdir, 'handle.log' );

    my $fh = IO::File->new( $handle_log, 'w' )
        or die "Can't write to $handle_log: $!";

    my $dispatch = Log::Dispatch->new;
    $dispatch->add( Log::Dispatch::Handle->new( name => 'handle',
                                                min_level => 'debug',
                                                handle => $fh ) );

    $dispatch->log( level => 'notice', message =>  "handle test\n" );

    # close file handles
    undef $dispatch;
    undef $fh;

    open $fh, '<', $handle_log
        or die "Can't open $handle_log: $!";

    my @log = <$fh>;

    close $fh;

    is( $log[0], "handle test\n",
        "Log::Dispatch::Handle created log file should contain 'handle test\\n'" );
}

# Log::Dispatch::Email::MailSend
SKIP:
{
    skip "Cannot do MailSend tests", 1
        unless $tests{MailSend} && $TestConfig{email_address};

    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::Email::MailSend->new( name => 'Mail::Send',
                                                         min_level => 'debug',
                                                         to => $TestConfig{email_address},
                                                         subject => 'Log::Dispatch test suite' ) );

    $dispatch->log( level => 'emerg', message => "Mail::Send test - If you can read this then the test succeeded (PID $$)" );

    diag( "Sending email with Mail::Send to $TestConfig{email_address}.\nIf you get it then the test succeeded (PID $$)\n" );
    undef $dispatch;

    ok( 1, 'sent email via MailSend' );
}


# Log::Dispatch::Email::MailSendmail
SKIP:
{
    skip "Cannot do MailSendmail tests", 1
        unless $tests{MailSendmail} && $TestConfig{email_address};

    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::Email::MailSendmail->new( name => 'Mail::Sendmail',
                                                             min_level => 'debug',
                                                             to => $TestConfig{email_address},
                                                             subject => 'Log::Dispatch test suite' ) );

    $dispatch->log( level => 'emerg', message => "Mail::Sendmail test - If you can read this then the test succeeded (PID $$)" );

    diag( "Sending email with Mail::Sendmail to $TestConfig{email_address}.\nIf you get it then the test succeeded (PID $$)\n" );
    undef $dispatch;

    ok( 1, 'sent email via MailSendmail' );
}

# Log::Dispatch::Email::MIMELite
SKIP:
{

    skip "Cannot do MIMELite tests", 1
        unless $tests{MIMELite} && $TestConfig{email_address};

    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::Email::MIMELite->new( name => 'Mime::Lite',
                                                         min_level => 'debug',
                                                         to => $TestConfig{email_address},
                                                         subject => 'Log::Dispatch test suite' ) );

    $dispatch->log( level => 'emerg', message => "MIME::Lite - If you can read this then the test succeeded (PID $$)" );

    diag( "Sending email with MIME::Lite to $TestConfig{email_address}.\nIf you get it then the test succeeded (PID $$)\n" );
    undef $dispatch;

    ok( 1, 'sent mail via MIMELite' );
}

# Log::Dispatch::Screen
{
    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::Screen->new( name => 'screen',
                                                min_level => 'debug',
                                                stderr => 0 ) );

    my $text;
    tie *STDOUT, 'Test::Tie::STDOUT', \$text;
    $dispatch->log( level => 'crit', message => 'testing screen' );
    untie *STDOUT;

    is( $text, 'testing screen',
        "Log::Dispatch::Screen outputs to STDOUT" );
}

# Log::Dispatch::Output->accepted_levels
{
    my $l = Log::Dispatch::Screen->new( name => 'foo',
                                        min_level => 'warning',
                                        max_level => 'alert',
                                        stderr => 0 );

    my @expected = qw(warning error critical alert);
    my @levels = $l->accepted_levels;

    my $pass = 1;
    for (my $x = 0; $x < scalar @expected; $x++)
    {
        $pass = 0 unless $expected[$x] eq $levels[$x];
    }

    is( scalar @expected, scalar @levels,
        "number of levels matched" );

    ok( $pass, "levels matched" );
}

# Log::Dispatch single callback
{
    my $reverse = sub { my %p = @_;  return reverse $p{message}; };
    my $dispatch = Log::Dispatch->new( callbacks => $reverse );

    my $string;
    $dispatch->add( Log::Dispatch::String->new( name => 'foo',
                                                string => \$string,
                                                min_level => 'warning',
                                                max_level => 'alert',
                                              ) );

    $dispatch->log( level => 'warning', message => 'esrever' );

    is( $string, 'reverse',
        "callback to reverse text" );
}

# Log::Dispatch multiple callbacks
{
    my $reverse = sub { my %p = @_;  return reverse $p{message}; };
    my $uc = sub { my %p = @_; return uc $p{message}; };

    my $dispatch = Log::Dispatch->new( callbacks => [ $reverse, $uc ] );

    my $string;
    $dispatch->add( Log::Dispatch::String->new( name => 'foo',
                                                string => \$string,
                                                min_level => 'warning',
                                                max_level => 'alert',
                                              ) );

    $dispatch->log( level => 'warning', message => 'esrever' );

    is( $string, 'REVERSE',
        "callback to reverse and uppercase text" );
}

# Log::Dispatch::Output single callback
{
    my $reverse = sub { my %p = @_;  return reverse $p{message}; };

    my $dispatch = Log::Dispatch->new;

    my $string;
    $dispatch->add( Log::Dispatch::String->new( name => 'foo',
                                                string => \$string,
                                                min_level => 'warning',
                                                max_level => 'alert',
                                                callbacks => $reverse ) );

    $dispatch->log( level => 'warning', message => 'esrever' );

    is( $string, 'reverse',
        "Log::Dispatch::Output callback to reverse text" );
}

# Log::Dispatch::Output multiple callbacks
{
    my $reverse = sub { my %p = @_;  return reverse $p{message}; };
    my $uc = sub { my %p = @_; return uc $p{message}; };

    my $dispatch = Log::Dispatch->new;

    my $string;
    $dispatch->add( Log::Dispatch::String->new( name => 'foo',
                                                string => \$string,
                                                min_level => 'warning',
                                                max_level => 'alert',
                                                callbacks => [ $reverse, $uc ] ) );

    $dispatch->log( level => 'warning', message => 'esrever' );

    is( $string, 'REVERSE',
        "Log::Dispatch::Output callbacks to reverse and uppercase text" );
}

# test level paramter to callbacks
{
    my $level = sub { my %p = @_; return uc $p{level}; };

    my $dispatch = Log::Dispatch->new( callbacks => $level );

    my $string;
    $dispatch->add( Log::Dispatch::String->new( name => 'foo',
                                                string => \$string,
                                                min_level => 'warning',
                                                max_level => 'alert',
                                                stderr => 0 ) );

    $dispatch->log( level => 'warning', message => 'esrever' );

    is( $string, 'WARNING',
        "Log::Dispatch callback to uppercase the level parameter" );
}

# Comprehensive test of new methods that match level names
{
    my %levels = map { $_ => $_ } ( qw( debug info notice warning error critical alert emergency ) );
    @levels{ qw( err crit emerg ) } = ( qw( error critical emergency ) );

    foreach my $allowed_level ( qw( debug info notice warning error critical alert emergency ) )
    {
        my $dispatch = Log::Dispatch->new;

        my $string;
        $dispatch->add( Log::Dispatch::String->new( name => 'foo',
                                                    string => \$string,
                                                    min_level => $allowed_level,
                                                    max_level => $allowed_level,
                                                  ) );

        foreach my $test_level ( qw( debug info notice warning err
                                     error crit critical alert emerg emergency ) )
        {
            $string = '';
            $dispatch->$test_level( $test_level, 'test' );

            if ( $levels{$test_level} eq $allowed_level )
            {
                my $expect = join $", $test_level, 'test';
                is( $string, $expect,
                    "Calling $test_level method should send message '$expect'" );
            }
            else
            {
                ok( ! length $string,
                    "Calling $test_level method should not log anything" );
            }
        }
    }
}

# Log::Dispatch->level_is_valid method
{
    foreach my $l ( qw( debug info notice warning err error
                        crit critical alert emerg emergency ) )
    {
        ok( Log::Dispatch->level_is_valid($l), "$l is valid level" );
    }

    foreach my $l ( qw( debu inf foo bar ) )
    {
        ok( ! Log::Dispatch->level_is_valid($l), "$l is not valid level" );
    }
}

# make sure passing mode as write works
{
    my $mode_log = File::Spec->catfile( $tempdir, 'mode.log' );

    my $f1 = Log::Dispatch::File->new( name => 'file',
                                       min_level => 1,
                                       filename => $mode_log,
                                       mode => 'write',
                                      );
    $f1->log( level => 'emerg',
              message => "test2\n" );

    undef $f1;

    open my $fh, '<', $mode_log
        or die "Cannot read $mode_log: $!";
    my $data = join '', <$fh>;
    close $fh;

    like( $data, qr/^test2/, "test write mode" );
}

# Log::Dispatch::Email::MailSender
SKIP:
{
    skip "Cannot do MailSender tests", 1
        unless $tests{MailSender} && $TestConfig{email_address};

    my $dispatch = Log::Dispatch->new;

    $dispatch->add
        ( Log::Dispatch::Email::MailSender->new
              ( name => 'Mail::Sender',
                min_level => 'debug',
                smtp => 'localhost',
                to => $TestConfig{email_address},
                subject => 'Log::Dispatch test suite' ) );

    $dispatch->log( level => 'emerg', message => "Mail::Sender - If you can read this then the test succeeded (PID $$)" );

    diag( "Sending email with Mail::Sender to $TestConfig{email_address}.\nIf you get it then the test succeeded (PID $$)\n" );
    undef $dispatch;

    ok( 1, 'sent email via MailSender' );
}

# dispatcher exists
{
    my $dispatch = Log::Dispatch->new;

    $dispatch->add
        ( Log::Dispatch::Screen->new( name => 'yomama',
                                      min_level => 'alert' ) );

    ok( $dispatch->output('yomama'),
        "yomama output should exist" );

    ok( ! $dispatch->output('nomama'),
        "nomama output should not exist" );
}

# Test Log::Dispatch::File - close_after_write & permissions
{
    my $dispatch = Log::Dispatch->new;

    my $close_log = File::Spec->catfile( $tempdir, 'close.log' );

    $dispatch->add( Log::Dispatch::File->new( name => 'close',
                                              min_level => 'info',
                                              filename => $close_log,
                                              permissions => 0777,
                                              close_after_write => 1 ) );

    $dispatch->log( level => 'info', message => "info\n" );

    open my $fh, '<', $close_log
        or die "Can't read $close_log: $!";

    my @log = <$fh>;
    close $fh;

    is( $log[0], "info\n",
        "First line in log file should be 'info\\n'" );

    my $mode = ( stat $close_log )[2]
        or die "Cannot stat $close_log: $!";

    my $mode_string = sprintf( '%04o', $mode & 07777 );

    if( $^O =~ /win32/i )
    {
        ok( $mode_string == '0777' || $mode_string == '0666',
            "Mode should be 0777 or 0666");
    }
    else
    {
        is( $mode_string, '0777',
            "Mode should be 0777" );
    }
}

{
    my $dispatch = Log::Dispatch->new;

    my $chmod_log = File::Spec->catfile( $tempdir, 'chmod.log' );

    open my $fh, '>', $chmod_log
        or die "Cannot write to $chmod_log: $!";
    close $fh;

    chmod 0777, $chmod_log
        or die "Cannot chmod 0777 $chmod_log: $!";

    my @chmod;
    no warnings 'once';
    local *CORE::chmod = sub { @chmod = @_; warn @chmod };

    $dispatch->add( Log::Dispatch::File->new( name => 'chmod',
                                              min_level => 'info',
                                              filename => $chmod_log,
                                              permissions => 0777,
                                            ) );

    $dispatch->warning('test');

    ok( ! scalar @chmod,
        'chmod() was not called when permissions already matched what was specified' );
}


SKIP:
{
    skip "Cannot test utf8 files with this version of Perl ($])", 1
        unless $] >= 5.008;

    my $dispatch = Log::Dispatch->new;

    my $utf8_log = File::Spec->catfile( $tempdir, 'utf8.log' );

    $dispatch->add( Log::Dispatch::File->new( name => 'utf8',
                                              min_level => 'info',
                                              filename => $utf8_log,
                                              binmode => ':utf8',
                                            ) );

    my @warnings;

    {
        local $SIG{__WARN__} = sub { push @warnings, @_ };
        $dispatch->warning("\x{999A}");
    }

    ok( ! scalar @warnings,
        'utf8 binmode was applied to file and no warnings were issued' );
}

# would_log
{
    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::Null->new( name => 'null',
                                              min_level => 'warning',
                                            ) );

    ok( ! $dispatch->would_log('foo'),
        "will not log 'foo'" );

    ok( ! $dispatch->would_log('debug'),
        "will not log 'debug'" );

    ok( $dispatch->would_log('crit'),
        "will log 'crit'" );
}

{
    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::Null->new( name => 'null',
                                              min_level => 'info',
                                              max_level => 'critical',
                                            ) );

    my $called = 0;
    my $message = sub { $called = 1 };

    $dispatch->log( level => 'debug', message => $message );
    ok( ! $called, 'subref is not called if the message would not be logged' );

    $called = 0;
    $dispatch->log( level => 'warning', message => $message );
    ok( $called, 'subref is called when message is logged' );

    $called = 0;
    $dispatch->log( level => 'emergency', message => $message );
    ok( ! $called, 'subref is not called when message would not be logged' );
}

{
    my $string;

    my $dispatch = Log::Dispatch->new;
    $dispatch->add( Log::Dispatch::String->new( name => 'handle',
                                                string => \$string,
                                                min_level => 'debug',
                                              ) );

    $dispatch->log( level => 'debug',
                    message => sub { 'this is my message' },
                  );

    is( $string, 'this is my message', 'message returned by subref is logged' );
}

{
    my $string;

    my $dispatch = Log::Dispatch->new;
    $dispatch->add( Log::Dispatch::String->new( name => 'handle',
                                                string => \$string,
                                                min_level => 'debug',
                                              ) );

    eval
    {
        $dispatch->log_and_die( level => 'error',
                                message => 'this is my message',
                              );
    };

    my $e = $@;

    ok( $e, 'died when calling log_and_die()' );
    like( $e, qr{this is my message}, 'error contains expected message' );
    like( $e, qr{01-basic\.t line 614}, 'error croaked' );

    is( $string, 'this is my message', 'message is logged' );

    undef $string;

    eval
    {
        Croaker::croak($dispatch);
    };

    $e = $@;

    ok( $e, 'died when calling log_and_croak()' );
    like( $e, qr{croak}, 'error contains expected message' );
    like( $e, qr{01-basic\.t line 680}, 'error croaked from perspective of caller' );

    is( $string, 'croak', 'message is logged' );
}

package Log::Dispatch::String;

use strict;

use Log::Dispatch::Output;

use base qw( Log::Dispatch::Output );


sub new
{
    my $proto = shift;
    my $class = ref $proto || $proto;
    my %p = @_;

    my $self = bless { string => $p{string} }, $class;

    $self->_basic_init(%p);

    return $self;
}

sub log_message
{
    my $self = shift;
    my %p = @_;

    ${ $self->{string} } .= $p{message};
}


package Croaker;

sub croak
{
    my $log = shift;

    $log->log_and_croak( level => 'error', message => 'croak' );
}

# Used for testing Log::Dispatch::Screen
package Test::Tie::STDOUT;

sub TIEHANDLE
{
    my $class = shift;
    my $self = {};
    $self->{string} = shift;
    ${ $self->{string} } ||= '';

    return bless $self, $class;
}

sub PRINT
{
    my $self = shift;
    ${ $self->{string} } .= join '', @_;
}

sub PRINTF
{
    my $self = shift;
    my $format = shift;
    ${ $self->{string} } .= sprintf($format, @_);
}
