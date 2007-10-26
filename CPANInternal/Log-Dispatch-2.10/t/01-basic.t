#!/usr/bin/perl -w

use strict;

use Test::More tests => 131;

use_ok('Log::Dispatch');


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
use Log::Dispatch::Screen;

use IO::File;

if ( eval { require mod_perl } )
{
    require Log::Dispatch::ApacheLog;
}

my $dispatch = Log::Dispatch->new;
ok( $dispatch, "Couldn't create Log::Dispatch object\n" );

# 3-6  Test Log::Dispatch::File
{
    $dispatch->add( Log::Dispatch::File->new( name => 'file1',
					      min_level => 'emerg',
					      filename => './emerg_test.log' ) );

    $dispatch->log( level => 'info', message => "info level 1\n" );
    $dispatch->log( level => 'emerg', message => "emerg level 1\n" );

    $dispatch->add( Log::Dispatch::File->new( name => 'file2',
					      min_level => 'debug',
					      filename => 'debug_test.log' ) );

    $dispatch->log( level => 'info', message => "info level 2\n" );
    $dispatch->log( level => 'emerg', message => "emerg level 2\n" );

    # This'll close them filehandles!
    undef $dispatch;

    open LOG1, './emerg_test.log'
	or die "Can't read ./emerg_test.log: $!";
    open LOG2, './debug_test.log'
	or die "Can't read ./debug_test.log: $!";

    my @log = <LOG1>;
    is( $log[0], "emerg level 1\n",
        "First line in log file set to level 'emerg' is 'emerg level 1'" );

    is( $log[1], "emerg level 2\n",
        "Second line in log file set to level 'emerg' is 'emerg level 2'" );

    @log = <LOG2>;
    is( $log[0], "info level 2\n",
        "First line in log file set to level 'debug' is 'info level 2'" );

    is( $log[1], "emerg level 2\n",
        "Second line in log file set to level 'debug' is 'emerg level 2'" );

    close LOG1;
    close LOG2;

    unlink './emerg_test.log'
	or diag( "Can't remove ./emerg_test.log: $!" );

    unlink './debug_test.log'
	or diag( "Can't remove ./debug_test.log: $!" );
}

# 7  max_level test
{
    my $dispatch = Log::Dispatch->new;
    $dispatch->add( Log::Dispatch::File->new( name => 'file1',
					      min_level => 'debug',
					      max_level => 'crit',
					      filename => './max_test.log' ) );

    $dispatch->log( level => 'emerg', message => "emergency\n" );
    $dispatch->log( level => 'crit',  message => "critical\n" );

    open LOG, './max_test.log'
	or die "Can't read ./max_test.log: $!";
    my @log = <LOG>;

    is( $log[0], "critical\n",
        "First line in log file with a max level of 'crit' is 'critical'" );

    close LOG;

    unlink './max_test.log'
	or diag( "Can't remove ./max_test.log: $!" );
}

# 8  Log::Dispatch::Handle test
{
    my $fh = IO::File->new('>./handle_test.log')
	or die "Can't write to ./handle_test.log: $!";

    my $dispatch = Log::Dispatch->new;
    $dispatch->add( Log::Dispatch::Handle->new( name => 'handle',
						min_level => 'debug',
						handle => $fh ) );

    $dispatch->log( level => 'notice', message =>  "handle test\n" );

    my $handle = $dispatch->remove('handle');
    undef $handle;
    undef $fh;

    open LOG, './handle_test.log'
	or die "Can't open ./handle_test.log: $!";

    my @log = <LOG>;

    is( $log[0], "handle test\n",
        "Log::Dispatch::Handle created log file should contain 'handle test\\n'" );

    unlink './handle_test.log'
	or diag( "Can't temove ./handle_test.log: $!" );
}

# 9  Log::Dispatch::Email::MailSend
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

    ok(1);
}


# 10  Log::Dispatch::Email::MailSendmail
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

    ok(1);
}

# 11  Log::Dispatch::Email::MIMELite
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

    ok(1);
}

# 12  Log::Dispatch::Screen
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

# 13-14  Log::Dispatch::Output->accepted_levels
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

# 15:  Log::Dispatch single callback
{
    my $reverse = sub { my %p = @_;  return reverse $p{message}; };
    my $dispatch = Log::Dispatch->new( callbacks => $reverse );

    $dispatch->add( Log::Dispatch::Screen->new( name => 'foo',
						min_level => 'warning',
						max_level => 'alert',
						stderr => 0 ) );

    my $text;
    tie *STDOUT, 'Test::Tie::STDOUT', \$text;
    $dispatch->log( level => 'warning', message => 'esrever' );
    untie *STDOUT;

    is( $text, 'reverse',
        "callback to reverse text" );
}

# 16:  Log::Dispatch multiple callbacks
{
    my $reverse = sub { my %p = @_;  return reverse $p{message}; };
    my $uc = sub { my %p = @_; return uc $p{message}; };

    my $dispatch = Log::Dispatch->new( callbacks => [ $reverse, $uc ] );

    $dispatch->add( Log::Dispatch::Screen->new( name => 'foo',
						min_level => 'warning',
						max_level => 'alert',
						stderr => 0 ) );

    my $text;
    tie *STDOUT, 'Test::Tie::STDOUT', \$text;
    $dispatch->log( level => 'warning', message => 'esrever' );
    untie *STDOUT;

    is( $text, 'REVERSE',
        "callback to reverse and uppercase text" );
}

# 17:  Log::Dispatch::Output single callback
{
    my $reverse = sub { my %p = @_;  return reverse $p{message}; };

    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::Screen->new( name => 'foo',
						min_level => 'warning',
						max_level => 'alert',
						stderr => 0,
						callbacks => $reverse ) );

    my $text;
    tie *STDOUT, 'Test::Tie::STDOUT', \$text;
    $dispatch->log( level => 'warning', message => 'esrever' );
    untie *STDOUT;

    is( $text, 'reverse',
        "Log::Dispatch::Output callback to reverse text" );
}

# 18:  Log::Dispatch::Output multiple callbacks
{
    my $reverse = sub { my %p = @_;  return reverse $p{message}; };
    my $uc = sub { my %p = @_; return uc $p{message}; };

    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::Screen->new( name => 'foo',
						min_level => 'warning',
						max_level => 'alert',
						stderr => 0,
						callbacks => [ $reverse, $uc ] ) );

    my $text;
    tie *STDOUT, 'Test::Tie::STDOUT', \$text;
    $dispatch->log( level => 'warning', message => 'esrever' );
    untie *STDOUT;

    is( $text, 'REVERSE',
        "Log::Dispatch::Output callbacks to reverse and uppercase text" );
}

# 19:  test level paramter to callbacks
{
    my $level = sub { my %p = @_; return uc $p{level}; };

    my $dispatch = Log::Dispatch->new( callbacks => $level );

    $dispatch->add( Log::Dispatch::Screen->new( name => 'foo',
						min_level => 'warning',
						max_level => 'alert',
						stderr => 0 ) );

    my $text;
    tie *STDOUT, 'Test::Tie::STDOUT', \$text;
    $dispatch->log( level => 'warning', message => 'esrever' );
    untie *STDOUT;

    is( $text, 'WARNING',
        "Log::Dispatch callback to uppercase the level parameter" );
}

# 20 - 107: Comprehensive test of new methods that match level names
{
    my %levels = map { $_ => $_ } ( qw( debug info notice warning error critical alert emergency ) );
    @levels{ qw( err crit emerg ) } = ( qw( error critical emergency ) );

    foreach my $allowed_level ( qw( debug info notice warning error critical alert emergency ) )
    {
	my $dispatch = Log::Dispatch->new;

	$dispatch->add( Log::Dispatch::Screen->new( name => 'foo',
						    min_level => $allowed_level,
						    max_level => $allowed_level,
						    stderr => 0 ) );

	foreach my $test_level ( qw( debug info notice warning err
                                     error crit critical alert emerg emergency ) )
	{
	    my $text;
	    tie *STDOUT, 'Test::Tie::STDOUT', \$text;
	    $dispatch->$test_level( $test_level, 'test' );
	    untie *STDOUT;

	    if ( $levels{$test_level} eq $allowed_level )
	    {
		my $expect = join $", $test_level, 'test';
		is( $text, $expect,
                    "Calling $test_level method should send message '$expect'\n" );
	    }
	    else
	    {
		ok( ! $text,
                    "Calling $test_level method should not log anything" );
	    }
	}
    }
}

# 108 - 122:  Log::Dispatch->level_is_valid method
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

# 123: make sure passing mode as write works
{
    local *F;
    open F, '>./write_mode.tst'
	or die "Cannot open ./write_mode.tst: $!";
    print F "test1\n";
    close F;

    my $f1 = Log::Dispatch::File->new( name => 'file',
				       min_level => 1,
				       filename => './write_mode.tst',
				       mode => 'write',
				      );
    $f1->log( level => 'emerg',
	      message => "test2\n" );

    undef $f1;

    open F, '<./write_mode.tst'
	or die "Cannot read ./wr_mode.tst: $!";
    my $data = join '', <F>;
    close F;

    like( $data, qr/^test2/, "test write mode" );

    unlink './write_mode.tst'
	or diag( "Can't remove ./write_mode.tst: $!" );
}

# 124  Log::Dispatch::Email::MailSender
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

    ok(1);
}

# 125 - 126 dispatcher exists
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

# 127 - 128  Test Log::Dispatch::File - close_after_write & permissions
{
    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::File->new( name => 'close',
					      min_level => 'info',
					      filename => './close_test.log',
                                              permissions => 0777,
                                              close_after_write => 1 ) );

    $dispatch->log( level => 'info', message => "info\n" );

    open LOG1, './close_test.log'
	or die "Can't read ./clse_test.log: $!";

    my @log = <LOG1>;
    is( $log[0], "info\n",
        "First line in log file should be 'info\\n'" );

    close LOG1;

    my $mode = (stat('./close_test.log'))[2]
        or die "Cannot stat ./close_test.log: $!";

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

    unlink './close_test.log'
	or diag( "Can't remove ./close_test.log: $!" );
}

# 129 - 131 - would_log
{
    my $dispatch = Log::Dispatch->new;

    $dispatch->add( Log::Dispatch::File->new( name => 'file1',
					      min_level => 'warning',
					      filename => './would_test.log' ) );

    ok( !$dispatch->would_log('foo'),
        "will not log 'foo'" );

    ok( ! $dispatch->would_log('debug'),
        "will not log 'debug'" );

    ok( $dispatch->would_log('crit'),
        "will log 'crit'" );

    unlink './would_test.log'
	or diag( "Can't remove ./would_test.log: $!" );
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
