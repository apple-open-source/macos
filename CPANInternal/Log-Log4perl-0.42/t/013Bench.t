use Test;
use Benchmark qw/timeit timestr/;
use Log::Log4perl;

$count = 100_000;

unless ($ENV{LOG4PERL_BENCH}) {
    print "set \$ENV{LOG4PERL_BENCH} to a true value to run benchmarks, skipping...\n";
    ok(1);
    exit;
}

$conf = <<EOL;

#specify LOGLEVEL, appender1, appender2, ...
log4j.category.simplelayout       = INFO, simpleLayoutAppndr

log4j.category.patternlayout      = INFO,  PatternLayoutAppndr

log4j.category.multiappender      = INFO, PatternLayoutAppndr, 2ndPatternLayoutAppndr,
log4j.category.multiappender.c1   = INFO,  3rdPatternLayoutAppndr
log4j.category.multiappender.c1.c2   = INFO, 2ndPatternLayoutAppndr



# ---------------------------------------------
# PatternLayoutAppndr
log4j.appender.PatternLayoutAppndr        = Log::Log4perl::Appender::TestBuffer
log4j.appender.PatternLayoutAppndr.layout = org.apache.log4j.PatternLayout
log4j.appender.PatternLayoutAppndr.layout.ConversionPattern=%d %4r [%t] %-5p %c %t - %m%n

# ---------------------------------------------
# 2ndPatternLayoutAppndr
log4j.appender.2ndPatternLayoutAppndr        = Log::Log4perl::Appender::TestBuffer
log4j.appender.2ndPatternLayoutAppndr.layout = org.apache.log4j.PatternLayout
log4j.appender.2ndPatternLayoutAppndr.layout.ConversionPattern=%d %4r [%t] %-5p %c %t - %m%n

# ---------------------------------------------
# 3rdPatternLayoutAppndr
log4j.appender.3rdPatternLayoutAppndr        = Log::Log4perl::Appender::TestBuffer
log4j.appender.3rdPatternLayoutAppndr.layout = org.apache.log4j.PatternLayout
log4j.appender.3rdPatternLayoutAppndr.layout.ConversionPattern=%d %4r [%t] %-5p %c %t - %m%n


# ---------------------------------------------
# a SimpleLayout
log4j.appender.simpleLayoutAppndr        = Log::Log4perl::Appender::TestBuffer
log4j.appender.simpleLayoutAppndr.layout = org.apache.log4j.SimpleLayout




EOL

Log::Log4perl::init(\$conf);

$simplelayout = Log::Log4perl->get_logger('simplelayout');

$basecategory = Log::Log4perl->get_logger('patternlayout');

$firstlevelcategory = Log::Log4perl->get_logger('patternlayout.foo');

$secondlevelcategory = Log::Log4perl->get_logger('patternlayout.foo.bar');

print "Iterations: $count\n\n";


print "Just is_debug/info/warn/error/fatal() methods: \n";
$t = timeit $count, sub{my $v = $basecategory->is_debug();
                        $v = $basecategory->is_info();
                        $v = $basecategory->is_warn();
                        $v = $basecategory->is_error();
                        $v = $basecategory->is_fatal();
                       };
print timestr($t),"\n\n";

print "no logging: \n";
$t = timeit $count, sub{$basecategory->debug('debug message')};
print timestr($t),"\n\n";

print "a simple layout: \n";
$t = timeit $count, sub{$simplelayout->info('info message')};
print timestr($t),"\n\n";

print "pattern layout: \n";
$t = timeit $count, sub {$basecategory->info('info message')};
print timestr($t),"\n\n";

print "one level inheritance, no logging: \n";
$t = timeit $count, sub {$firstlevelcategory->debug('debug message')};
print timestr($t),"\n\n";

print "one level inheritance, logging: \n";
$t = timeit $count, sub {$firstlevelcategory->info('info message')};
print timestr($t),"\n\n";

print "two level inheritance, no logging: \n";
$t = timeit $count, sub {$secondlevelcategory->debug('debug message')};
print timestr($t),"\n\n";

print "two level inheritance, logging \n";
$t = timeit $count, sub {$secondlevelcategory->info('info message')};
print timestr($t),"\n\n";

#free up some memory?
undef($basecategory);
undef ($firstlevelcategory);
undef($secondlevelcategory);


$multi1 = Log::Log4perl->get_logger('multiappender');
$multi2 = Log::Log4perl->get_logger('multiappender.c1');
$multi3 = Log::Log4perl->get_logger('multiappender.c1.c2');

print "two appenders: \n";
$t = timeit $count, sub {$multi1->info('info message')};
print timestr($t),"\n\n";

print "three appenders, one level of inheritance: \n";
$t = timeit $count, sub {$multi2->info('info message')};
print timestr($t),"\n\n";

print "same appenders, two levels of inheritance: \n";
$t = timeit $count, sub {$multi3->info('info message')};
print timestr($t),"\n\n";






print


ok(1);

BEGIN { plan tests => 1, }
