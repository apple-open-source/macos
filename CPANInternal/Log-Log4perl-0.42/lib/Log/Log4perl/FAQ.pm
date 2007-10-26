1;

__END__

=head1 NAME

Log::Log4perl::FAQ - Frequently Asked Questions on Log::Log4perl

=head1 DESCRIPTION

This FAQ shows a wide variety of 
commonly encountered logging tasks and how to solve them 
in the most elegant way with Log::Log4perl. Most of the time, this will
be just a matter of smartly configuring your Log::Log4perl configuration files.

This document is supposed to grow week by week as the latest
"Log::Log4perl recipe of the week" hits the Log::Log4perl mailing list
at C<log4perl-devel@lists.sourceforge.net>.

=head2 How can I simply log all my ERROR messages to a file?

After pulling in the C<Log::Log4perl> module, just initialize its
behaviour by passing in a configuration to its C<init> method as a string
reference. Then, obtain a logger instance and write out a message
with its C<error()> method:

    use Log::Log4perl qw(get_logger);

        # Define configuration
    my $conf = q(
        log4perl.logger                    = ERROR, FileApp
        log4perl.appender.FileApp          = Log::Log4perl::Appender::File
        log4perl.appender.FileApp.filename = test.log
        log4perl.appender.FileApp.layout   = PatternLayout
        log4perl.appender.FileApp.layout.ConversionPattern = %d> %m%n
    );

        # Initialize logging behaviour
    Log::Log4perl->init( \$conf );

        # Obtain a logger instance
    my $logger = get_logger("Bar::Twix");
    $logger->error("Oh my, a dreadful error!");
    $logger->warn("Oh my, a dreadful warning!");

This will append something like

    2002/10/29 20:11:55> Oh my, a dreadful error!

to the log file C<test.log>. How does this all work? 

While the Log::Log4perl C<init()> method typically 
takes the name of a configuration file as its input parameter like
in

    Log::Log4perl->init( "/path/mylog.conf" );

the example above shows how to pass in a configuration as text in a 
scalar reference.

The configuration as shown
defines a logger of the root category, which has an appender of type 
C<Log::Log4perl::Appender::File> attached. The line

    log4perl.logger = ERROR, FileApp

doesn't list a category, defining a root logger. Compare that with

    log4perl.logger.Bar.Twix = ERROR, FileApp

which would define a logger for the category C<Bar::Twix>,
showing probably different behaviour. C<FileApp> on
the right side of the assignment is
an arbitrarily defined variable name, which is only used to somehow 
reference an appender defined later on.

Appender settings in the configuration are defined as follows:

    log4perl.appender.FileApp          = Log::Log4perl::Appender::File
    log4perl.appender.FileApp.filename = test.log

It selects the file appender of the C<Log::Log4perl::Appender>
hierarchy, which will append to the file C<test.log> if it already
exists. If we wanted to overwrite a potentially existing file, we would
have to explicitly set the appropriate C<Log::Log4perl::Appender::File>
parameter C<mode>:

    log4perl.appender.FileApp          = Log::Log4perl::Appender::File
    log4perl.appender.FileApp.filename = test.log
    log4perl.appender.FileApp.mode     = write

Also, the configuration defines a PatternLayout format, adding
the nicely formatted current date and time, an arrow (E<gt>) and
a space before the messages, which is then followed by a newline:

    log4perl.appender.FileApp.layout   = PatternLayout
    log4perl.appender.FileApp.layout.ConversionPattern = %d> %m%n

Obtaining a logger instance and actually logging something is typically
done in a different system part as the Log::Log4perl initialisation section,
but in this example, it's just done right after init for the 
sake of compactness:

        # Obtain a logger instance
    my $logger = get_logger("Bar::Twix");
    $logger->error("Oh my, a dreadful error!");

This retrieves an instance of the logger of the category C<Bar::Twix>, 
which, as all other categories, inherits behaviour from the root logger if no
other loggers are defined in the initialization section. 

The C<error()>
method fires up a message, which the root logger catches. Its
priority is equal to
or higher than the root logger's priority (ERROR), which causes the root logger
to forward it to its attached appender. By contrast, the following

    $logger->warn("Oh my, a dreadful warning!");

doesn't make it through, because the root logger sports a higher setting
(ERROR and up) than the WARN priority of the message.

=head2 How can I install Log::Log4perl on Microsoft Windows?

Log::Log4perl is fully supported on the Win32 platform. It has been tested 
with Activestate perl 5.6.1 under Windows 98 and rumor has it that it
also runs smoothly on all other major flavors (Windows NT, 2000, XP, etc.).

It also runs nicely with ActiveState 5.8.0, and, believe me, 
we had to jump through some major hoops for that.

Typically, Win32 systems don't have the C<make> utility installed,
so the standard C<perl Makefile.PL; make install> on the downloadable
distribution won't work. But don't despair, there's a very easy solution!

The C<Log::Log4perl> homepage provides a so-called PPD file for ActiveState's
C<ppm> installer, which comes with ActiveState perl by default.

=over 4

=item Install on ActiveState 5.6.*

The DOS command line

    ppm install "http://log4perl.sourceforge.net/ppm/Log-Log4perl.ppd"

will contact the Log4perl homepage, download the latest
C<Log::Log4perl>
distribution and install it. If your ActiveState installation
lacks any of the modules C<Log::Log4perl> depends upon, C<ppm> will 
automatically contact ActivateState and download them from their CPAN-like
repository.

=item Install on ActiveState 5.8.*

ActiveState's "Programmer's Package Manager" can be called from
Window's Start Menu:
Start-E<gt>Programs->E<gt>ActiveState ActivePerl 5.8E<gt>Perl Package Manager
will invoke ppm. Since Log::Log4perl hasn't made it yet into the standard
ActiveState repository (and you probably don't want their outdated packages
anyway), just tell ppm the first time you call it to add the Log4perl 
repository

    ppm> repository add http://log4perl.sourceforge.net/ppm

Then, just tell it to install Log::Log4perl and it will resolve all
dependencies automatically and fetch them from log4perl.sourceforge.net
if it can't find them in the main archives:

    ppm> install Log::Log4perl

=back

That's it! Afterwards, just create a Perl script like

    use Log::Log4perl qw(:easy);
    Log::Log4perl->easy_init($DEBUG);

    my $logger = get_logger("Twix::Bar");
    $logger->debug("Watch me!");

and run it. It should print something like 

    2002/11/06 01:22:05 Watch me!

If you find that something doesn't work, please let us know at
log4perl-devel@lists.sourceforge.net -- we'll apprechiate it. Have fun!

=head2 What's the easiest way to use Log4perl?

If you just want to get all the comfort of logging, without much
overhead, use I<Stealth Loggers>. If you use Log::Log4perl in 
C<:easy> mode like

    use Log::Log4perl qw(:easy);

you'll have the following functions available in the current package:

    DEBUG("message");
    INFO("message");
    WARN("message");
    ERROR("message");
    FATAL("message");

Just make sure that every package of your code where you're using them in
pulls in C<use Log::Log4perl qw(:easy)> first, then you're set.
Every stealth logger's category will be equivalent to the name of the
package it's located in.

These stealth loggers
will be absolutely silent until you initialize Log::Log4perl in 
your main program with either 

        # Define any Log4perl behaviour
    Log::Log4perl->init("foo.conf");

(using a full-blown Log4perl config file) or the super-easy method

        # Just log to STDERR
    Log::Log4perl->easy_init($DEBUG);

or the parameter-style method with a complexity somewhat in between:

        # Append to a log file
    Log::Log4perl->easy_init( { level   => $DEBUG,
                                file    => ">>test.log" } );

For more info, please check out L<Log::Log4perl/"Stealth Loggers">.

=head2 How can I include global (thread-specific) data in my log messages?

Say, you're writing a web application and want all your
log messages to include the current client's IP address. Most certainly,
you don't want to include it in each and every log message like in

    $logger->debug( $r->connection->remote_ip,
                    " Retrieving user data from DB" );

do you? Instead, you want to set it in a global data structure and
have Log::Log4perl include it automatically via a PatternLayout setting
in the configuration file:

    log4perl.appender.FileApp.layout.ConversionPattern = %X{ip} %m%n

The conversion specifier C<%X{ip}> references an entry under the key
C<ip> in the global C<MDC> (mapped diagnostic context) table, which 
you've set once via

    Log::Log4perl::MDC->put("ip", $r->connection->remote_ip);

at the start of the request handler. Note that this is a
I<static> (class) method, there's no logger object involved.
You can use this method with as many key/value pairs as you like as long
as you reference them under different names.

The mappings are stored in a global hash table within Log::Log4perl.
Luckily, because the thread
model in 5.8.0 doesn't share global variables between threads unless
they're explicitly marked as such, there's no problem with multi-threaded
environments.

For more details on the MDC, please refer to 
L<Log::Log4perl/"Mapped Diagnostic Context (MDC)"> and
L<Log::Log4perl::MDC>.

=head2 My application is already logging to a file. How can I duplicate all messages to also go to the screen?

Assuming that you already have a Log4perl configuration file like

    log4perl.logger                    = DEBUG, FileApp

    log4perl.appender.FileApp          = Log::Log4perl::Appender::File
    log4perl.appender.FileApp.filename = test.log
    log4perl.appender.FileApp.layout   = PatternLayout
    log4perl.appender.FileApp.layout.ConversionPattern = %d> %m%n

and log statements all over your code,
it's very easy with Log4perl to have the same messages both printed to
the logfile and the screen. No reason to change your code, of course, 
just add another appender to the configuration file and you're done:

    log4perl.logger                    = DEBUG, FileApp, ScreenApp

    log4perl.appender.FileApp          = Log::Log4perl::Appender::File
    log4perl.appender.FileApp.filename = test.log
    log4perl.appender.FileApp.layout   = PatternLayout
    log4perl.appender.FileApp.layout.ConversionPattern = %d> %m%n

    log4perl.appender.ScreenApp          = Log::Log4perl::Appender::Screen
    log4perl.appender.ScreenApp.stderr   = 0
    log4perl.appender.ScreenApp.layout   = PatternLayout
    log4perl.appender.ScreenApp.layout.ConversionPattern = %d> %m%n

The configuration file above is assuming that both appenders are
active in the same logger hierarchy, in this case the C<root> category.
But even if you've got file loggers defined in several parts of your system,
belonging to different logger categories,
each logging to different files, you can gobble up all logged messages
by defining a root logger with a screen appender, which would duplicate 
messages from all your file loggers to the screen due to Log4perl's 
appender inheritance. Check 

    http://www.perl.com/pub/a/2002/09/11/log4perl.html

for details. Have fun!

=head2 How can I make sure my application logs a message when it dies unexpectedly?

Whenever you encounter a fatal error in your application, instead of saying
something like

    open FILE, "<blah" or die "Can't open blah -- bailing out!";

just use Log::Log4perl's fatal functions instead:

    my $log = get_logger("Some::Package");
    open FILE, "<blah" or $log->logdie("Can't open blah -- bailing out!");

This will both log the message with priority FATAL according to your current
Log::Log4perl configuration and then call Perl's C<die()> 
afterwards to terminate the program. It works the same with 
stealth loggers (see L<Log::Log4perl/"Stealth Loggers">), 
all you need to do is call

    use Log::Log4perl qw(:easy);
    open FILE, "<blah" or LOGDIE "Can't open blah -- bailing out!";

What can you do if you're using some library which doesn't use Log::Log4perl
and calls C<die()> internally if something goes wrong? Use a
C<$SIG{__DIE__}> pseudo signal handler

    use Log::Log4perl qw(get_logger);

    $SIG{__DIE__} = sub {
        $Log::Log4perl::caller_depth++;
        my $logger = get_logger("");
        $logger->fatal(@_);
        die @_; # Now terminate really
    };

This will catch every C<die()>-Exception of your
application or the modules it uses. It
will fetch a root logger and pass on the C<die()>-Message to it.
If you make sure you've configured with a root logger like this:

    Log::Log4perl->init(\q{
        log4perl.category         = FATAL, Logfile
        log4perl.appender.Logfile = Log::Log4perl::Appender::File
        log4perl.appender.Logfile.filename = fatal_errors.log
        log4perl.appender.Logfile.layout = \
                   Log::Log4perl::Layout::PatternLayout
        log4perl.appender.Logfile.layout.ConversionPattern = %F{1}-%L (%M)> %m%n
    });

then all C<die()> messages will be routed to a file properly. The line

     $Log::Log4perl::caller_depth++;

in the pseudo signal handler above merits a more detailed explanation. With
the setup above, if a module calls C<die()> in one of its functions, 
the fatal message will be logged in the signal handler and not in the
original function -- which will cause the %F, %L and %M placeholders
in the pattern layout to be replaced by the filename, the line number
and the function/method name of the signal handler, not the error-throwing
module. To adjust this, Log::Log4perl has the C<$caller_depth> variable, 
which defaults to 0, but can be set to positive integer values
to offset the caller level. Increasing
it by one will cause it to log the calling function's parameters, not
the ones of the signal handler. 
See L<Log::Log4perl/"Using Log::Log4perl from wrapper classes"> for more
details.

=head2 How can I hook up the LWP library with Log::Log4perl?

Or, to put it more generally: How can you utilize a third-party
library's embedded logging and debug statements in Log::Log4perl? 
How can you make them print
to configurable appenders, turn them on and off, just as if they 
were regular Log::Log4perl logging statements?

The easiest solution is to map the third-party library logging statements
to Log::Log4perl's stealth loggers via a typeglob assignment.

As an example, let's take LWP, one of the most popular Perl modules, 
which makes handling WWW requests and responses a breeze.
Internally, LWP uses its own logging and debugging system, 
utilizing the following calls 
inside the LWP code (from the LWP::Debug man page):

        # Function tracing
    LWP::Debug::trace('send()');

        # High-granular state in functions
    LWP::Debug::debug('url ok');

        # Data going over the wire
    LWP::Debug::conns("read $n bytes: $data");

First, let's assign Log::Log4perl priorities 
to these functions: I'd suggest that
C<debug()> messages have priority C<INFO>, 
C<trace()> uses C<DEBUG> and C<conns()> also logs with C<DEBUG> -- 
although your mileage may certainly vary.

Now, in order to transpartently hook up LWP::Debug with Log::Log4perl,
all we have to do is say

    package LWP::Debug;
    use Log::Log4perl qw(:easy);

    *trace = *INFO;
    *conns = *DEBUG;
    *debug = *DEBUG;

    package main;
    # ... go on with your regular program ...

at the beginning of our program. In this way, every time the, say, 
C<LWP::UserAgent> module calls C<LWP::Debug::trace()>, it will implicitely 
call INFO(), which is the C<info()> method of a stealth logger defined for
the Log::Log4perl category C<LWP::Debug>. Is this cool or what?

Here's a complete program:

    use LWP::UserAgent;
    use HTTP::Request::Common;
    use Log::Log4perl qw(:easy);

    Log::Log4perl->easy_init(
        { category => "LWP::Debug",
          level    => $DEBUG,
          layout   => "%r %p %M-%L %m%n",
        });

    package LWP::Debug;
    use Log::Log4perl qw(:easy);
    *trace = *INFO;
    *conns = *DEBUG;
    *debug = *DEBUG;

    package main;
    my $ua = LWP::UserAgent->new();
    my $resp = $ua->request(GET "http://amazon.com");

    if($resp->is_success()) {
        print "Success: Received ", 
              length($resp->content()), "\n";
    } else {
        print "Error: ", $resp->code(), "\n";
    }

This will generate the following output on STDERR:

    174 INFO LWP::UserAgent::new-164 ()
    208 INFO LWP::UserAgent::request-436 ()
    211 INFO LWP::UserAgent::send_request-294 GET http://amazon.com
    212 DEBUG LWP::UserAgent::_need_proxy-1123 Not proxied
    405 INFO LWP::Protocol::http::request-122 ()
    859 DEBUG LWP::Protocol::collect-206 read 233 bytes
    863 DEBUG LWP::UserAgent::request-443 Simple response: Found
    869 INFO LWP::UserAgent::request-436 ()
    871 INFO LWP::UserAgent::send_request-294 
     GET http://www.amazon.com:80/exec/obidos/gateway_redirect
    872 DEBUG LWP::UserAgent::_need_proxy-1123 Not proxied
    873 INFO LWP::Protocol::http::request-122 ()
    1016 DEBUG LWP::UserAgent::request-443 Simple response: Found
    1020 INFO LWP::UserAgent::request-436 ()
    1022 INFO LWP::UserAgent::send_request-294 
     GET http://www.amazon.com/exec/obidos/subst/home/home.html/
    1023 DEBUG LWP::UserAgent::_need_proxy-1123 Not proxied
    1024 INFO LWP::Protocol::http::request-122 ()
    1382 DEBUG LWP::Protocol::collect-206 read 632 bytes
    ...
    2605 DEBUG LWP::Protocol::collect-206 read 77 bytes
    2607 DEBUG LWP::UserAgent::request-443 Simple response: OK
    Success: Received 42584

Of course, in this way, the embedded logging and debug statements within
LWP can be utilized in any Log::Log4perl way you can think of. You can
have them sent to different appenders, block them based on the
category and everything else Log::Log4perl has to offer.

Only drawback of this method: Steering logging behaviour via category 
is always based on the C<LWP::Debug> package. Although the logging
statements reflect the package name of the issuing module properly, 
the stealth loggers in C<LWP::Debug> are all of the category C<LWP::Debug>.
This implies that you can't control the logging behaviour based on the
package that's I<initiating> a log request (e.g. LWP::UserAgent) but only
based on the package that's actually I<executing> the logging statement, 
C<LWP::Debug> in this case.

To work around this conundrum, we need to write a wrapper function and
plant it into the C<LWP::Debug> package. It will determine the caller and
create a logger bound to a category with the same name as the caller's
package:

    package LWP::Debug;

    use Log::Log4perl qw(:levels get_logger);

    sub l4p_wrapper {
        my($prio, @message) = @_;
        $Log::Log4perl::caller_depth += 2;
        get_logger(caller(1))->log($prio, @message);
        $Log::Log4perl::caller_depth -= 2;
    }

    no warnings 'redefine';
    *trace = sub { l4p_wrapper($INFO, @_); };
    *debug = *conns = sub { l4p_wrapper($DEBUG, @_); };

    package main;
    # ... go on with your main program ...

This is less performant than the previous approach, because every
log request will request a reference to a logger first, then call
the wrapper, which will in turn call the appropriate log function.

This hierarchy shift has to be compensated for by increasing
C<$Log::Log4perl::caller_depth> by 2 before calling the log function
and decreasing it by 2 right afterwards. Also, the C<l4p_wrapper>
function shown above calls C<caller(1)> which determines the name
of the package I<two> levels down the calling hierarchy (and 
therefore compensates for both the wrapper function and the
anonymous subroutine calling it).

C<no warnings 'redefine'> suppresses a warning Perl would generate
otherwise
upon redefining C<LWP::Debug>'s C<trace()>, C<debug()> and C<conns()>
functions. In case you use a perl prior to 5.6.x, you need
to manipulate C<$^W> instead.

=head2 What if I need dynamic values in a static Log4perl configuration file?

Say, your application uses Log::Log4perl for logging and 
therefore comes with a Log4perl configuration file, specifying the logging
behaviour.
But, you also want it to take command line parameters to set values
like the name of the log file.
How can you have
both a static Log4perl configuration file and a dynamic command line
interface?

As of Log::Log4perl 0.28, every value in the configuration file
can be specified as a I<Perl hook>. So, instead of saying

    log4perl.appender.Logfile.filename = test.log

you could just as well have a Perl subroutine deliver the value
dynamically:

    log4perl.appender.Logfile.filename = sub { logfile(); };

given that C<logfile()> is a valid function in your C<main> package
returning a string containing the path to the log file.

Or, think about using the value of an environment variable:

    log4perl.appender.DBI.user = sub { $ENV{USERNAME} };

When C<Log::Log4perl-E<gt>init()> parses the configuration
file, it will notice the assignment above because of its
C<sub {...}> pattern and treat it in a special way:
It will evaluate the subroutine (which can contain
arbitrary Perl code) and take its return value as the right side
of the assignment.

A typical application would be called like this on the command line:

    app                # log file is "test.log"
    app -l mylog.txt   # log file is "mylog.txt"

Here's some sample code implementing the command line interface above:

    use Log::Log4perl qw(get_logger);
    use Getopt::Std;

    getopt('l:', \our %OPTS);

    my $conf = q(
    log4perl.category.Bar.Twix         = WARN, Logfile
    log4perl.appender.Logfile          = Log::Log4perl::Appender::File
    log4perl.appender.Logfile.filename = sub { logfile(); };
    log4perl.appender.Logfile.layout   = SimpleLayout
    );

    Log::Log4perl::init(\$conf);

    my $logger = get_logger("Bar::Twix");
    $logger->error("Blah");

    ###########################################
    sub logfile {
    ###########################################
        if(exists $OPTS{l}) {
            return $OPTS{l};
        } else {
            return "test.log";
        }
    }

Every Perl hook may contain arbitrary perl code,
just make sure to fully qualify eventual variable names
(e.g. C<%main::OPTS> instead of C<%OPTS>).

B<SECURITY NOTE>: this feature means arbitrary perl code
can be embedded in the config file.  In the rare case
where the people who have access to your config file
are different from the people who write your code and
shouldn't have execute rights, you might want to call

    $Log::Log4perl::Config->allow_code(0);

before you call init(). This will prevent Log::Log4perl from
executing I<any> Perl code in the config file (including
code for custom conversion specifiers 
(see L<Log::Log4perl::Layout::PatternLayout/"Custom cspecs">).

=head2 How can I roll over my logfiles automatically at midnight?

Long-running applications tend to produce ever-increasing logfiles.
For backup and cleanup purposes, however, it is often desirable to move
the current logfile to a different location from time to time and
start writing a new one.

This is a non-trivial task, because it has to happen in sync with 
the logging system in order not to lose any messages in the process.

Luckily, I<Mark Pfeiffer>'s C<Log::Dispatch::FileRotate> appender
works well with Log::Log4perl to rotate your logfiles in a variety of ways.
All you have to do is specify it in your Log::Log4perl configuration file
and your logfiles will be rotated automatically.

You can choose between rolling based on a maximum size ("roll if greater
than 10 MB") or based on a date pattern ("roll everyday at midnight").
In both cases, C<Log::Dispatch::FileRotate> allows you to define a 
number C<max> of saved files to keep around until it starts overwriting
the oldest ones. If you set the C<max> parameter to 2 and the name of
your logfile is C<test.log>, C<Log::Dispatch::FileRotate> will
move C<test.log> to C<test.log.1> on the first rollover. On the second
rollover, it will move C<test.log.1> to C<test.log.2> and then C<test.log>
to C<test.log.1>. On the third rollover, it will move C<test.log.1> to 
C<test.log.2> (therefore discarding the old C<test.log.2>) and 
C<test.log> to C<test.log.1>. And so forth. This way, there's always 
going to be a maximum of 2 saved log files around.

Here's an example of a Log::Log4perl configuration file, defining a
daily rollover at midnight (date pattern C<yyyy-MM-dd>), keeping
a maximum of 5 saved logfiles around:

    log4perl.category         = WARN, Logfile
    log4perl.appender.Logfile = Log::Dispatch::FileRotate
    log4perl.appender.Logfile.filename    = test.log
    log4perl.appender.Logfile.max         = 5
    log4perl.appender.Logfile.DatePattern = yyyy-MM-dd
    log4perl.appender.Logfile.TZ          = PST
    log4perl.appender.Logfile.layout = \
        Log::Log4perl::Layout::PatternLayout 
    log4perl.appender.Logfile.layout.ConversionPattern = %d %m %n 

Please see the C<Log::Dispatch::FileRotate> documentation for details.
C<Log::Dispatch::FileRotate> is available on CPAN.

=head2 What's the easiest way to turn off all logging, even with a lengthy Log4perl configuration file?

In addition to category-based levels and appender thresholds,
Log::Log4perl supports system-wide logging thresholds. This is the 
minimum level the system will require of any logging events in order for them 
to make it through to any configured appenders.

For example, putting the line

    log4perl.threshold = ERROR

anywhere in your configuration file will limit any output to any appender
to events with priority of ERROR or higher (ERROR or FATAL that is). 

However, in order to suppress all logging entirely, you need to use a
priority that's higher than FATAL: It is simply called C<OFF>, and it is never
used by any logger. By definition, it is higher than the highest 
defined logger level.

Therefore, if you keep the line

    log4perl.threshold = OFF

somewhere in your Log::Log4perl configuration, the system will be quiet
as a graveyard. If you deactivate the line (e.g. by commenting it out), 
the system will, upon config reload, snap back to normal operation, providing 
logging messages according to the rest of the configuration file again.

=head2 I keep getting duplicate log messages! What's wrong?

Having several settings for related categories in the Log4perl 
configuration file sometimes leads to a phenomenon called 
"message duplication". It can be very confusing at first,
but if thought through properly, it turns out that Log4perl behaves
as advertised. But, don't despair, of course there's a number of 
ways to avoid message duplication in your logs.

Here's a sample Log4perl configuration file that produces the
phenomenon:

    log4perl.logger.Cat        = ERROR, Screen
    log4perl.logger.Cat.Subcat = WARN, Screen

    log4perl.appender.Screen   = Log::Log4perl::Appender::Screen
    log4perl.appender.Screen.layout = SimpleLayout

It defines two loggers, one for category C<Cat> and one for
C<Cat::Subcat>, which is obviously a subcategory of C<Cat>.
The parent logger has a priority setting of ERROR, the child
is set to the lower C<WARN> level.

Now imagine the following code in your program:

    my $logger = get_logger("Cat.Subcat");
    $logger->warn("Warning!");

What do you think will happen? An unexperienced Log4perl user
might think: "Well, the message is being sent with level WARN, so the 
C<Cat::Subcat> logger will accept it and forward it to the 
attached C<Screen> appender. Then, the message will percolate up 
the logger hierarchy, find
the C<Cat> logger, which will suppress the message because of its
ERROR setting."
But, perhaps surprisingly, what you'll get with the
code snippet above is not one but two log messages written 
to the screen:

    WARN - Warning!
    WARN - Warning!

What happened? The culprit is that once the logger C<Cat::Subcat> 
decides to fire, it will forward the message I<unconditionally> 
to all directly or indirectly attached appenders. The C<Cat> logger 
will never be asked if it wants the message or not -- the message 
will just be pushed through to the appender attached to C<Cat>.

One way to prevent the message from bubbling up the logger
hierarchy is to set the C<additivity> flag of the subordinate logger to
C<0>:

    log4perl.logger.Cat            = ERROR, Screen
    log4perl.logger.Cat.Subcat     = WARN, Screen
    log4perl.additivity.Cat.Subcat = 0

    log4perl.appender.Screen   = Log::Log4perl::Appender::Screen
    log4perl.appender.Screen.layout = SimpleLayout

The message will now be accepted by the C<Cat::Subcat> logger,
forwarded to its appender, but then C<Cat::Subcat> will suppress
any further action. While this setting avoids duplicate messages
as seen before, it is often not the desired behaviour. Messages
percolating up the hierarchy are a useful Log4perl feature.

If you're defining I<different> appenders for the two loggers,
one other option is to define an appender threshold for the
higher-level appender. Typically it is set to be 
equal to the logger's level setting:

    log4perl.logger.Cat           = ERROR, Screen1
    log4perl.logger.Cat.Subcat    = WARN, Screen2

    log4perl.appender.Screen1   = Log::Log4perl::Appender::Screen
    log4perl.appender.Screen1.layout = SimpleLayout
    log4perl.appender.Screen1.Threshold = ERROR

    log4perl.appender.Screen2   = Log::Log4perl::Appender::Screen
    log4perl.appender.Screen2.layout = SimpleLayout

Since the C<Screen1> appender now blocks every message with
a priority less than ERROR, even if the logger in charge
lets it through, the message percolating up the hierarchy is
being blocked at the last minute and I<not> appended to C<Screen1>.

So far, we've been operating well within the boundaries of the 
Log4j standard, which Log4perl adheres to. However, if 
you would really, really like to use a single appender 
and keep the message percolation intact without having to deal
with message duplication, there's a non-standard solution for you:

    log4perl.logger.Cat        = ERROR, Screen
    log4perl.logger.Cat.Subcat = WARN, Screen

    log4perl.appender.Screen   = Log::Log4perl::Appender::Screen
    log4perl.appender.Screen.layout = SimpleLayout

    log4perl.oneMessagePerAppender = 1

The C<oneMessagePerAppender> flag will suppress duplicate messages
to the same appender. Again, that's non-standard. But way cool :).

=head2 How can I configure Log::Log4perl to send me email if something happens?

Some incidents require immediate action. You can't wait until someone
checks the log files, you need to get notified on your pager right away.

The easiest way to do that is by using the C<Log::Dispatch::Email::MailSend>
module as an appender. It comes with the C<Log::Dispatch> bundle and
allows you to specify recipient and subject of outgoing emails in the Log4perl
configuration file:

    log4perl.category = FATAL, Mailer
    log4perl.appender.Mailer         = Log::Dispatch::Email::MailSend
    log4perl.appender.Mailer.to      = drone@pageme.net
    log4perl.appender.Mailer.subject = Something's broken!
    log4perl.appender.Mailer.layout  = SimpleLayout

The message of every log incident this appender gets
will then be forwarded to the given
email address. Check the C<Log::Dispatch::Email::MailSend> documentation
for details. And please make sure there's not a flood of email messages 
sent out by your application, filling up the receipient's inbox.

There's one caveat you need to know about: The C<Log::Dispatch::Email>
hierarchy of appenders turns on I<buffering> by default. This means that
the appender will not send out messages right away but wait until a 
certain threshold has been reached. If you'd rather have your alerts
sent out immeditately, use

    log4perl.appender.Mailer.buffered = 0

to turn buffering off.

=head2 How can I write my own appender?

First off, Log::Log4perl comes with a set of standard appenders. Then,
there's a lot of Log4perl-compatible appenders already
available on CPAN: Just run a search for C<Log::Dispatch> on 
http://search.cpan.org and chances are that what you're looking for 
has already been developed, debugged and been used successfully 
in production -- no need for you to reinvent the wheel.

Also, Log::Log4perl ships with a nifty database appender named
Log::Log4perl::Appender::DBI -- check it out if talking to databases is your
desire.

But if you're up for a truly exotic task, you might have to write
an appender yourself. That's very easy -- it takes no longer
than a couple of minutes.

Say, we wanted to create an appender of the class
C<ColorScreenAppender>, which logs messages
to the screen in a configurable color. Just create a new class 
in C<ColorScreenAppender.pm>:

    package ColorScreen;

Now let's assume that your Log::Log4perl
configuration file C<test.conf> looks like this:

    log4perl.logger = INFO, ColorApp

    log4perl.appender.ColorApp=ColorScreenAppender
    log4perl.appender.ColorApp.color=blue

    log4perl.appender.ColorApp.layout = PatternLayout
    log4perl.appender.ColorApp.layout.ConversionPattern=%d %m %n

This will cause Log::Log4perl on C<init()> to look for a class
ColorScreenAppender and call its constructor new(). Let's add
new() to ColorScreenAppender.pm:

    sub new {
        my($class, %options) = @_;

        my $self = { %options };
        bless $self, $class;

        return $self;
    }

To initialize this appender, Log::Log4perl will call 
and pass all attributes of the appender as defined in the configuration
file to the constructor as name/value pairs (in this case just one):

    ColorScreenAppender->new(color => "blue");

The new() method listed above stores the contents of the
%options hash in the object's
instance data hash (referred to by $self).
That's all for initializing a new appender with Log::Log4perl.

Second, ColorScreenAppender needs to expose a 
C<log()> method, which will be called by Log::Log4perl 
every time it thinks the appender should fire. Along with the
object reference (as usual in Perl's object world), log()
will receive a list of name/value pairs, of which only the one
under the key C<message> shall be of interest for now since it is the
message string to be logged. At this point, Log::Log4perl has already taken
care of joining the message to be a single string.

For our special appender ColorScreenAppender, we're using the
Term::ANSIColor module to colorize the output:

    use Term::ANSIColor;

    sub log {
        my($self, %params) = @_;

        print colored($params{message},
                      $self->{color});
    }

The color (as configured in the Log::Log4perl configuration file) 
is available as $self-E<gt>{color} in the appender object. Don't
forget to return

    1;

at the end of ColorScreenAppender.pm and you're done. Install the new appender
somewhere where perl can find it and try it with a test script like 

    use Log::Log4perl qw(:easy);
    Log::Log4perl->init("test.conf");
    ERROR("blah");

to see the new colored output. Is this cool or what?

=head2 How can I drill down on references before logging them?

If you've got a reference to a nested structure or object, then 
you probably don't want to log it as C<HASH(0x81141d4)> but rather
dump it as something like

    $VAR1 = {
              'a' => 'b',
              'd' => 'e'
            };

via a module like Data::Dumper. While it's syntactically correct to say

    $logger->debug(Data::Dumper::Dumper($ref));

this call imposes a huge performance penalty on your application
if the message is suppressed by Log::Log4perl, because Data::Dumper
will perform its expensive operations in any case, because it doesn't
know that its output will be thrown away immediately.

As of Log::Log4perl 0.28, there's a better way: Use the 
message output filter format as in

    $logger->debug( {filter => \&Data::Dumper::Dumper,
                     value  => $ref} );

and Log::Log4perl won't call the filter function unless the message really
gets written out to an appender. Just make sure to pass the whole slew as a
reference to a hash specifying a filter function (as a sub reference)
under the key C<filter> and the value to be passed to the filter function in
C<value>). 
When it comes to logging, Log::Log4perl will call the filter function,
pass the C<value> as an argument and log the return value.
Saves you serious cycles.

=head2 How can I collect all FATAL messages in an extra log file?

Suppose you have employed Log4perl all over your system and you've already
activated logging in various subsystems. On top of that, without disrupting
any other settings, how can you collect all FATAL messages all over the system
and send them to a separate log file? 

If you define a root logger like this:

    log4perl.logger                  = FATAL, File
    log4perl.appender.File           = Log::Log4perl::Appender::File
    log4perl.appender.File.filename  = /tmp/fatal.txt
    log4perl.appender.File.layout    = PatternLayout
    log4perl.appender.File.layout.ConversionPattern= %d %m %n
        # !!! Something's missing ...

you'll be surprised to not only receive all FATAL messages
issued anywhere in the system,
but also everything else -- gazillions of 
ERROR, WARN, INFO and even DEBUG messages will end up in
your fatal.txt logfile!
Reason for this is Log4perl's (or better: Log4j's) appender additivity. 
Once a 
lower-level logger decides to fire, the message is going to be forwarded
to all appenders upstream -- without further priority checks with their
attached loggers.

There's a way to prevent this, however: If your appender defines a
minimum threshold, only messages of this priority or higher are going
to be logged. So, just add

    log4perl.appender.File.Threshold = FATAL 

to the configuration above, and you'll get what you wanted in the 
first place: An overall system FATAL message collector.

=head2 How can I bundle several log messages into one?

Would you like to tally the messages arriving at your appender and
dump out a summary once they're exceeding a certain threshold?
So that something like

    $logger->error("Blah");
    $logger->error("Blah");
    $logger->error("Blah");

won't be logged as 

    Blah
    Blah
    Blah

but as

    [3] Blah

instead? If you'd like to hold off on logging a message until it has been
sent a couple of times, you can roll that out by creating a buffered 
appender.

Let's define a new appender like

    package TallyAppender;

    sub new {
        my($class, %options) = @_;

        my $self = { maxcount => 5,
                     %options
                   };

        bless $self, $class;

        $self->{last_message}        = "";
        $self->{last_message_count}  = 0;

        return $self;
    }

with two additional instance variables C<last_message> and 
C<last_message_count>, storing the content of the last message sent
and a counter of how many times this has happened. Also, it features
a configuration parameter C<maxcount> which defaults to 5 in the
snippet above but can be set in the Log4perl configuration file like this:

    log4perl.logger = INFO, A
    log4perl.appender.A=TallyAppender
    log4perl.appender.A.maxcount = 3

The main tallying logic lies in the appender's C<log> method,
which is called every time Log4perl thinks a message needs to get logged
by our appender:

    sub log {
        my($self, %params) = @_;

            # Message changed? Print buffer.
        if($self->{last_message} and
           $params{message} ne $self->{last_message}) {
            print "[$self->{last_message_count}]: " .
                  "$self->{last_message}";
            $self->{last_message_count} = 1;
            $self->{last_message} = $params{message};
            return;
        }

        $self->{last_message_count}++;
        $self->{last_message} = $params{message};

            # Threshold exceeded? Print, reset counter
        if($self->{last_message_count} >= 
           $self->{maxcount}) {
            print "[$self->{last_message_count}]: " .
                  "$params{message}";
            $self->{last_message_count} = 0;
            $self->{last_message}       = "";
            return;
        }
    }

We basically just check if the oncoming message in C<$param{message}>
is equal to what we've saved before in the C<last_message> instance
variable. If so, we're increasing C<last_message_count>.
We print the message in two cases: If the new message is different
than the buffered one, because then we need to dump the old stuff
and store the new. Or, if the counter exceeds the threshold, as
defined by the C<maxcount> configuration parameter.

Please note that the appender always gets the fully rendered message and
just compares it as a whole -- so if there's a date/timestamp in there,
that might confuse your logic. You can work around this by specifying
%m %n as a layout and add the date later on in the appender. Or, make
the comparison smart enough to omit the date.

At last, don't forget what happens if the program is being shut down.
If there's still messages in the buffer, they should be printed out
at that point. That's easy to do in the appender's DESTROY method,
which gets called at object destruction time:

    sub DESTROY {
        my($self) = @_;

        if($self->{last_message_count}) {
            print "[$self->{last_message_count}]: " .
                  "$self->{last_message}";
            return;
        }
    }

This will ensure that none of the buffered messages are lost. 
Happy buffering!

=head2 I want to log ERROR and WARN messages to different files! How can I do that?

Let's assume you wanted to have each logging statement written to a
different file, based on the statement's priority. Messages with priority
C<WARN> are supposed to go to C</tmp/app.warn>, events prioritized
as C<ERROR> should end up in C</tmp/app.error>.

Now, if you define two appenders C<AppWarn> and C<AppError>
and assign them both to the root logger,
messages bubbling up from any loggers below will be logged by both
appenders because of Log4perl's message propagation feature. If you limit
their exposure via the appender threshold mechanism and set 
C<AppWarn>'s threshold to C<WARN> and C<AppError>'s to C<ERROR>, you'll
still get C<ERROR> messages in C<AppWarn>, because C<AppWarn>'s C<WARN>
setting will just filter out messages with a I<lower> priority than
C<WARN> -- C<ERROR> is higher and will be allowed to pass through.

What we need for this is a Log4perl I<Custom Filter>, available with 
Log::Log4perl 0.30.

Both appenders need to verify that
the priority of the oncoming messages exactly I<matches> the priority 
the appender is supposed to log messages of. To accomplish this task,
let's define two custom filters, C<MatchError> and C<MatchWarn>, which,
when attached to their appenders, will limit messages passed on to them
to those matching a given priority: 

    log4perl.logger = WARN, AppWarn, AppError

        # Filter to match level ERROR
    log4perl.filter.MatchError = Log::Log4perl::Filter::LevelMatch
    log4perl.filter.MatchError.LevelToMatch  = ERROR
    log4perl.filter.MatchError.AcceptOnMatch = true

        # Filter to match level WARN
    log4perl.filter.MatchWarn  = Log::Log4perl::Filter::LevelMatch
    log4perl.filter.MatchWarn.LevelToMatch  = WARN
    log4perl.filter.MatchWarn.AcceptOnMatch = true

        # Error appender
    log4perl.appender.AppError = Log::Log4perl::Appender::File
    log4perl.appender.AppError.filename = /tmp/app.err
    log4perl.appender.AppError.layout   = SimpleLayout
    log4perl.appender.AppError.Filter   = MatchError

        # Warning appender
    log4perl.appender.AppWarn = Log::Log4perl::Appender::File
    log4perl.appender.AppWarn.filename = /tmp/app.warn
    log4perl.appender.AppWarn.layout   = SimpleLayout
    log4perl.appender.AppWarn.Filter   = MatchWarn

The appenders C<AppWarn> and C<AppError> defined above are logging to C</tmp/app.warn> and
C</tmp/app.err> respectively and have the custom filters C<MatchWarn> and C<MatchError>
attached.
This setup will direct all WARN messages, issued anywhere in the system, to /tmp/app.warn (and 
ERROR messages to /tmp/app.error) -- without any overlaps.

=head2 On our server farm, Log::Log4perl configuration files differ slightly from host to host. Can I roll them all into one?

You sure can, because Log::Log4perl allows you to specify attribute values 
dynamically. Let's say that one of your appenders expects the host's IP address
as one of its attributes. Now, you could certainly roll out different 
configuration files for every host and specify the value like

    log4perl.appender.MyAppender    = Log::Log4perl::Appender::SomeAppender
    log4perl.appender.MyAppender.ip = 10.0.0.127

but that's a maintenance nightmare. Instead, you can have Log::Log4perl 
figure out the IP address at configuration time and set the appender's
value correctly:

        # Set the IP address dynamically
    log4perl.appender.MyAppender    = Log::Log4perl::Appender::SomeAppender
    log4perl.appender.MyAppender.ip = sub { \
       use Sys::Hostname; \
       use Socket; \
       return inet_ntoa(scalar gethostbyname hostname); \
    }

If Log::Log4perl detects that an attribute value starts with something like
C<"sub {...">, it will interpret it as a perl subroutine which is to be executed
once at configuration time (not runtime!) and its return value is
to be used as the attribute value. This comes in handy
for rolling out applications whichs Log::Log4perl configuration files
show small host-specific differences, because you can deploy the unmodified
application distribution on all instances of the server farm.

=head2 Log4perl doesn't interpret my backslashes correctly!

If you're using Log4perl's feature to specify the configuration as a
string in your program (as opposed to a separate configuration file),
chances are that you've written it like this:

    # *** WRONG! ***

    Log::Log4perl->init( \ <<END_HERE);
        log4perl.logger = WARN, A1
        log4perl.appender.A1 = Log::Log4perl::Appender::Screen
        log4perl.appender.A1.layout = \
            Log::Log4perl::Layout::PatternLayout
        log4perl.appender.A1.layout.ConversionPattern = %m%n
    END_HERE

    # *** WRONG! ***

and you're getting the following error message:

    Layout not specified for appender A1 at .../Config.pm line 342.

What's wrong? The problem is that you're using a here-document with
substitution enabled (C<E<lt>E<lt>END_HERE>) and that Perl won't 
interpret backslashes at line-ends as continuation characters but 
will essentially throw them out. So, in the code above, the layout line
will look like

    log4perl.appender.A1.layout =

to Log::Log4perl which causes it to report an error. To interpret the backslash
at the end of the line correctly as a line-continuation character, use
the non-interpreting mode of the here-document like in 

    # *** RIGHT! ***

    Log::Log4perl->init( \ <<'END_HERE');
        log4perl.logger = WARN, A1
        log4perl.appender.A1 = Log::Log4perl::Appender::Screen
        log4perl.appender.A1.layout = \
            Log::Log4perl::Layout::PatternLayout
        log4perl.appender.A1.layout.ConversionPattern = %m%n
    END_HERE

    # *** RIGHT! ***

(note the single quotes around C<'END_HERE'>) or use C<q{...}> 
instead of a here-document and Perl will treat the backslashes at 
line-end as intended.

=head2 I want to suppress certain messages based on their content!

Let's assume you've plastered all your functions with Log4perl 
statements like

    sub some_func {

        INFO("Begin of function");

        # ... Stuff happens here ...

        INFO("End of function");
    }

to issue two log messages, one at the beginning and one at the end of
each function. Now you want to suppress the message at the beginning
and only keep the one at the end, what can you do? You can't use the category
mechanism, because both messages are issued from the same package.

Log::Log4perl's custom filters (0.30 or better) provide an interface for the 
Log4perl user to step in right before a message gets logged and decide if 
it should be written out or suppressed, based on the message content or other
parameters:

    use Log::Log4perl qw(:easy);

    Log::Log4perl::init( \ <<'EOT' );
        log4perl.logger             = INFO, A1
        log4perl.appender.A1        = Log::Log4perl::Appender::Screen
        log4perl.appender.A1.layout = \
            Log::Log4perl::Layout::PatternLayout
        log4perl.appender.A1.layout.ConversionPattern = %m%n

        log4perl.filter.M1 = Log::Log4perl::Filter::StringMatch
        log4perl.filter.M1.StringToMatch = Begin
        log4perl.filter.M1.AcceptOnMatch = false

        log4perl.appender.A1.Filter = M1
EOT

The last four statements in the configuration above are defining a custom 
filter C<M1> of type C<Log::Log4perl::Filter::StringMatch>, which comes with 
Log4perl right out of the box and allows you to define a text pattern to match
(as a perl regular expression) and a flag C<AcceptOnMatch> indicating
if a match is supposed to suppress the message or let it pass through.

The last line then assigns this filter to the C<A1> appender, which will
call it every time it receives a message to be logged and throw all
messages out I<not> matching the regular expression C<Begin>.

Instead of using the standard C<Log::Log4perl::Filter::StringMatch> filter,
you can define your own, simply using a perl subroutine:

    log4perl.filter.ExcludeBegin  = sub { !/Begin/ }
    log4perl.appender.A1.Filter   = ExcludeBegin

For details on custom filters, check L<Log::Log4perl::Filter>.

=head2 My new module uses Log4perl -- but what happens if the calling program didn't configure it?

If a Perl module uses Log::Log4perl, it will typically rely on the
calling program to initialize it. If it is using Log::Log4perl in C<:easy>
mode, like in 

    package MyMod;
    use Log::Log4perl qw(:easy);

    sub foo {
        DEBUG("In foo");
    }

    1;

and the calling program doesn't initialize Log::Log4perl at all (e.g. because
it has no clue that it's available), Log::Log4perl will silently
ignore all logging messages. However, if the module is using Log::Log4perl 
in regular mode like in

    package MyMod;
    use Log::Log4perl qw(get_logger);

    sub foo {
        my $logger = get_logger("");
        $logger->debug("blah");
    }

    1;

and the main program is just using the module like in

    use MyMode;
    MyMode::foo();

then Log::Log4perl will also ignore all logging messages but
issue a warning like

    Log4perl: Seems like no initialization happened. 
    Forgot to call init()?

(only once!) to remind novice users to not forget to initialize 
the logging system before using it. 
However, if you want to suppress this message, just
add the C<:nowarn> target to the module's C<use Log::Log4perl> call:

    use Log::Log4perl qw(get_logger :nowarn);

This will have Log::Log4perl silently ignore all logging statements if
no initialization has taken place. 

If the module wants to figure out if some other program part has 
already initialized Log::Log4perl, it can do so by calling

    Log::Log4perl::initialized()

which will return a true value in case Log::Log4perl has been initialized 
and a false value if not.

=head2 How can I synchronize access to an appender?

If you're using the same instance of an appender in multiple processes, 
each passing on messages to it in parallel, you might end up with 
overlapping log entries.

Imagine a file appender that you create in the main program, and which
will then be shared between the parent and a forked child process. When
it comes to logging, Log::Log4perl won't synchronize access to it.
Depending on your operating system's flush mechanism, buffer size and the size
of your messages, there's a small chance of an overlap.

A guaranteed way of having messages separated is putting a
Log::Log4perl::Appender::Synchronized composite appender in 
between Log::Log4perl and the real appender. It will make sure to
let messages pass through this virtual gate one by one only. 

Here's a sample configuration to synchronize access to a file appender:

    log4perl.category.Bar.Twix          = WARN, Syncer

    log4perl.appender.Logfile           = Log::Log4perl::Appender::File
    log4perl.appender.Logfile.autoflush = 1
    log4perl.appender.Logfile.filename  = test.log
    log4perl.appender.Logfile.layout    = SimpleLayout

    log4perl.appender.Syncer            = Log::Log4perl::Appender::Synchronized
    log4perl.appender.Syncer.appender   = Logfile

C<Log::Log4perl::Appender::Synchronized> uses 
the C<IPC::Shareable> module and its semaphores, which will slow down writing
the log messages, but ensures sequential access featuring atomic checks.
Check L<Log::Log4perl::Appender::Synchronized> for details.

=head2 Can I use Log::Log4perl with log4j's Chainsaw?

Yes, Log::Log4perl can be configured to send its events to log4j's 
graphical log UI I<Chainsaw>.

=for html
<p>
<TABLE><TR><TD>
<A HREF="/images/chainsaw2.jpg"><IMG SRC="/images/chainsaw2s.jpg"></A>
<TR><TD>
<I>Figure 1: Chainsaw receives Log::Log4perl events</I>
</TABLE>
<p>

=for text
Figure1: Chainsaw receives Log::Log4perl events

Here's how it works:

=over 4

=item *

Get Guido Carls' E<lt>gcarls@cpan.orgE<gt> Log::Log4perl extension
C<Log::Log4perl::Layout::XMLLayout> from CPAN and install it:

    perl -MCPAN -eshell
    cpan> install Log::Log4perl::Layout::XMLLayout

=item *

Install and start Chainsaw, which is part of the C<log4j> distribution now
(see http://jakarta.apache.org/log4j ). Create a configuration file like

  <log4j:configuration debug="true">
    <plugin name="XMLSocketReceiver" 
            class="org.apache.log4j.net.XMLSocketReceiver">
      <param name="decoder" value="org.apache.log4j.xml.XMLDecoder"/> 
      <param name="Port" value="4445"/> 
    </plugin>
    <root> <level value="debug"/> </root> 
  </log4j:configuration>

and name it e.g. C<config.xml>. Then start Chainsaw like

  java -Dlog4j.debug=true -Dlog4j.configuration=config.xml \
    -classpath ".:log4j-1.3alpha.jar:log4j-chainsaw-1.3alpha.jar" \
    org.apache.log4j.chainsaw.LogUI

and watch the GUI coming up.

=item *

Configure Log::Log4perl to use a socket appender with an XMLLayout, pointing
to the host/port where Chainsaw (as configured above) is waiting with its
XMLSocketReceiver:

  use Log::Log4perl qw(get_logger);
  use Log::Log4perl::Layout::XMLLayout;

  my $conf = q(
    log4perl.category.Bar.Twix          = WARN, Appender
    log4perl.appender.Appender          = Log::Log4perl::Appender::Socket
    log4perl.appender.Appender.PeerAddr = localhost
    log4perl.appender.Appender.PeerPort = 4445
    log4perl.appender.Appender.layout   = Log::Log4perl::Layout::XMLLayout
  );

  Log::Log4perl::init(\$conf);

    # Nasty hack to suppress encoding header
  my $app = Log::Log4perl::appenders->{"Appender"};
  $app->layout()->{enc_set} = 1;

  my $logger = get_logger("Bar");
  $logger->error("One");

The nasty hack shown in the code snippet above is currently (October 2003) 
necessary, because Chainsaw expects XML messages to arrive in a format like

  <log4j:event logger="Bar.Twix"
               timestamp="1066794904310"
               level="ERROR"
               thread="10567">
    <log4j:message><![CDATA[Two]]></log4j:message>
    <log4j:NDC><![CDATA[undef]]></log4j:NDC>
    <log4j:locationInfo class="main"
      method="main"
      file="./t"
      line="32">
    </log4j:locationInfo>
  </log4j:event>

without a preceding 

  <?xml version = "1.0" encoding = "iso8859-1"?>

which Log::Log4perl::Layout::XMLLayout applies to the first event sent
over the socket.

=back

See figure 1 for a screenshot of Chainsaw in action, receiving events from
the Perl script shown above.

Many thanks to Chainsaw's
Scott Deboy <sdeboy@comotivsystems.com> for his support!

=head2 How can I run Log::Log4perl under mod_perl?

In persistent environments it's important to play by the rules outlined
in section L<Log::Log4perl/"Initialize once and only once">. 
If you haven't read this yet, please go ahead and read it right now. It's 
very important.

And no matter if you use a startup handler to init() Log::Log4perl or use the
init_once() strategy (added in 0.42), either way you're very likely to have
unsynchronized writes to logfiles.

If Log::Log4perl is configured with a log file appender, and it is 
initialized via
the Apache startup handler, the file handle created initially will be
shared among all Apache processes. Similarly, with the init_once()
approach: although every process has a separate L4p configuration,
processes are gonna share the appender file I<names> instead, effectively
opening several different file handles on the same file.

Now, having several appenders using the same file handle or having
several appenders logging to the same file unsynchronized, this might
result in overlapping messages. Sometimes, this is acceptable. If it's
not, here's two strategies:

=over 4

=item *

Use the L<Log::Log4perl::Appender::Synchronized> appender to connect to 
your file appenders. Here's the writeup: 
http://log4perl.sourceforge.net/releases/Log-Log4perl/docs/html/Log/Log4perl/FAQ.html#23804 

=item *

Use a different logfile for every process like in

     #log4perl.conf
     ...
     log4perl.appender.A1.filename = sub { "mylog.$$.log" }

=back

=cut

=head1 SEE ALSO

Log::Log4perl

=head1 AUTHOR

Mike Schilli, E<lt>log4perl@perlmeister.comE<gt>

=cut
