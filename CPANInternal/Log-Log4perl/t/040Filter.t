###########################################
# Test Suite for Log::Log4perl::Filter
# Mike Schilli, 2003 (m@perlmeister.com)
###########################################
use warnings;
use strict;

use Test::More tests => 29;

use Log::Log4perl;

#############################################
# Use a pattern-matching subroutine as filter
#############################################

Log::Log4perl->init(\ <<'EOT');
  log4perl.logger.Some = INFO, A1
  log4perl.filter.MyFilter    = sub { /let this through/ }
  log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.A1.Filter = MyFilter
  log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

my $buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
my $logger = Log::Log4perl->get_logger("Some.Where");

    # Let this through
$logger->info("Here's the info, let this through!");

    # Suppress this
$logger->info("Here's the info, suppress this!");

like($buffer->buffer(), qr(let this through), "pattern-match let through");
unlike($buffer->buffer(), qr(suppress), "pattern-match block");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# Block in filter based on message level
#############################################
Log::Log4perl->init(\ <<'EOT');
  log4perl.logger.Some = INFO, A1
  log4perl.filter.MyFilter        = sub {    \
       my %p = @_;                           \
       ($p{log4p_level} eq "WARN") ? 1 : 0;  \
                                          }
  log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.A1.Filter = MyFilter
  log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
$logger = Log::Log4perl->get_logger("Some.Where");

    # Suppress this
$logger->info("This doesn't make it");

    # Let this through
$logger->warn("This passes the hurdle");


like($buffer->buffer(), qr(passes the hurdle), "level-match let through");
unlike($buffer->buffer(), qr(make it), "level-match block");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# Filter combination with Filter::Boolean
#############################################
Log::Log4perl->init(\ <<'EOT');
    log4perl.logger = INFO, A1

    log4perl.filter.Match1       = sub { /let this through/ }
    log4perl.filter.Match2       = sub { /and that, too/ }
    log4perl.filter.Match3       = Log::Log4perl::Filter::StringMatch
    log4perl.filter.Match3.StringToMatch = suppress
    log4perl.filter.Match3.AcceptOnMatch = true

    log4perl.filter.MyBoolean       = Log::Log4perl::Filter::Boolean
    log4perl.filter.MyBoolean.logic = !Match3 && (Match1 || Match2)

    log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.Filter = MyBoolean
    log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
$logger = Log::Log4perl->get_logger("Some.Where");

    # Let through
$logger->info("let this through");
like($buffer->buffer(), qr(let this through), "Boolean 1");
$buffer->buffer("");

    # Block
$logger->info("suppress, let this through");
is($buffer->buffer(), "", "Boolean 2");
$buffer->buffer("");

    # Let through
$logger->info("and that, too");
like($buffer->buffer(), qr(and that, too), "Boolean 3");
$buffer->buffer("");

    # Block
$logger->info("and that, too suppress");
is($buffer->buffer(), "", "Boolean 4");
$buffer->buffer("");

    # Block
$logger->info("let this through - and that, too - suppress");
is($buffer->buffer(), "", "Boolean 5");
$buffer->buffer("");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# LevelMatchFilter
#############################################
Log::Log4perl->init(\ <<'EOT');
    log4perl.logger = INFO, A1
    log4perl.filter.Match1      = Log::Log4perl::Filter::LevelMatch
    log4perl.filter.Match1.LevelToMatch = INFO
    log4perl.filter.Match1.AcceptOnMatch = true
    log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.Filter = Match1
    log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
$logger = Log::Log4perl->get_logger("Some.Where");

    # Let through
$logger->info("let this through");
like($buffer->buffer(), qr(let this through), "Matched Level");
$buffer->buffer("");

    # Block
$logger->warn("suppress, let this through");
is($buffer->buffer(), "", "Non-Matched Level 1");
$buffer->buffer("");

    # Block
$logger->debug("and that, too");
is($buffer->buffer(), "", "Non-Matched Level 2");
$buffer->buffer("");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# LevelMatchFilter - negative
#############################################
Log::Log4perl->init(\ <<'EOT');
    log4perl.logger = INFO, A1
    log4perl.filter.Match1      = Log::Log4perl::Filter::LevelMatch
    log4perl.filter.Match1.LevelToMatch = INFO
    log4perl.filter.Match1.AcceptOnMatch = false
    log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.Filter = Match1
    log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
$logger = Log::Log4perl->get_logger("Some.Where");

    # Block
$logger->info("let this through");
is($buffer->buffer(), "", "Non-Matched Level 1 - negative");
$buffer->buffer("");

    # Pass
$logger->warn("suppress, let this through");
like($buffer->buffer(), qr(let this through), "Matched Level - negative");
$buffer->buffer("");

    # Pass
$logger->fatal("and that, too");
like($buffer->buffer(), qr(and that, too), "Matched Level - negative");
$buffer->buffer("");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# StringMatchFilter
#############################################
Log::Log4perl->init(\ <<'EOT');
    log4perl.logger = INFO, A1
    log4perl.filter.Match1      = Log::Log4perl::Filter::StringMatch
    log4perl.filter.Match1.StringToMatch = block this
    log4perl.filter.Match1.AcceptOnMatch = false
    log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.Filter = Match1
    log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
$logger = Log::Log4perl->get_logger("Some.Where");

    # Let through
$logger->info("let this through");
like($buffer->buffer(), qr(let this through), "StringMatch - passed");
$buffer->buffer("");

    # Block
$logger->info("block this");
is($buffer->buffer(), "", "StringMatch - blocked");
$buffer->buffer("");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# StringMatchFilter - negative
#############################################
Log::Log4perl->init(\ <<'EOT');
    log4perl.logger = INFO, A1
    log4perl.filter.Match1      = Log::Log4perl::Filter::StringMatch
    log4perl.filter.Match1.StringToMatch = let this through
    log4perl.filter.Match1.AcceptOnMatch = true
    log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.Filter = Match1
    log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
$logger = Log::Log4perl->get_logger("Some.Where");

    # Let through
$logger->info("let this through");
like($buffer->buffer(), qr(let this through), "StringMatch - passed");
$buffer->buffer("");

    # Block
$logger->info("block this");
is($buffer->buffer(), "", "StringMatch - blocked");
$buffer->buffer("");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# Non-existing filter class
#############################################
eval {
    Log::Log4perl->init(\ <<'EOT');
        log4perl.logger = INFO, A1
        log4perl.filter.Match1      = Log::Log4perl::Filter::GobbleDeGook
        log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
        log4perl.appender.A1.Filter = Match1
        log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT
};

like($@, qr/Log::Log4perl::Filter::GobbleDeGook/, "Unknown Filter");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# Syntax error in subroutine
#############################################
eval {
    Log::Log4perl->init(\ <<'EOT');
        log4perl.logger = INFO, A1
        log4perl.filter.Match1      = sub { */+- };
        log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
        log4perl.appender.A1.Filter = Match1
        log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT
};

like($@, qr/Can't evaluate/, "Detect flawed filter subroutine");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# LevelRangeFilter
#############################################
Log::Log4perl->init(\ <<'EOT');
    log4perl.logger = DEBUG, A1
    log4perl.filter.Range1      = Log::Log4perl::Filter::LevelRange
    log4perl.filter.Range1.LevelMin = INFO
    log4perl.filter.Range1.LevelMax = WARN
    log4perl.filter.Range1.AcceptOnMatch = true
    log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.Filter = Range1
    log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
$logger = Log::Log4perl->get_logger("Some.Where");

    # Block
$logger->debug("blah");
is($buffer->buffer(), "", "Outside Range");
$buffer->buffer("");

    # Let through
$logger->info("let this through");
like($buffer->buffer(), qr(let this through), "Matched Range");
$buffer->buffer("");

    # Let through
$logger->warn("let this through");
like($buffer->buffer(), qr(let this through), "Matched Range");
$buffer->buffer("");

    # Block
$logger->error("blah");
is($buffer->buffer(), "", "Outside Range");
$buffer->buffer("");

Log::Log4perl->reset();
$buffer->reset();

#############################################
# LevelRangeFilter - negative
#############################################
Log::Log4perl->init(\ <<'EOT');
    log4perl.logger = DEBUG, A1
    log4perl.filter.Range1      = Log::Log4perl::Filter::LevelRange
    log4perl.filter.Range1.LevelMin = INFO
    log4perl.filter.Range1.LevelMax = WARN
    log4perl.filter.Range1.AcceptOnMatch = false
    log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.Filter = Range1
    log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout
EOT

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("A1");

    # Define a logger
$logger = Log::Log4perl->get_logger("Some.Where");

    # Let through
$logger->debug("debug msg");
like($buffer->buffer(), qr(debug msg), "Outside Range - negative");
$buffer->buffer("");

    # Block
$logger->info("block this");
is($buffer->buffer(), "", "Matched Range - negative");
$buffer->buffer("");

    # Block
$logger->warn("block this");
is($buffer->buffer(), "", "Matched Range - negative");
$buffer->buffer("");

    # Let through
$logger->error("error msg");
like($buffer->buffer(), qr(error msg), "Outside Range - negative");
$buffer->buffer("");

Log::Log4perl->reset();
$buffer->reset();
