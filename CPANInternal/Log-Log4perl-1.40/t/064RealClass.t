# get_logger($self) in the base class returns a logger for the subclass
# category

use strict;
use Test::More;
use Log::Log4perl::Appender::TestBuffer;

plan tests => 1;

package AppBaseClass;
use Log::Log4perl qw(get_logger :easy);
sub meth {
    my( $self ) = @_;
    get_logger( ref $self )->warn("in base class");
}

package AppSubClass;
our @ISA = qw(AppBaseClass);
use Log::Log4perl qw(get_logger :easy);
sub new {
    bless {}, shift;
}

package main;

use Log::Log4perl qw(get_logger :easy);

# $Log::Log4perl::CHATTY_DESTROY_METHODS = 1;

my $conf = q(
log4perl.category.AppSubClass     = WARN, LogBuffer
log4perl.appender.LogBuffer        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.LogBuffer.layout = Log::Log4perl::Layout::PatternLayout
log4perl.appender.LogBuffer.layout.ConversionPattern = %m%n
);

Log::Log4perl::init(\$conf);

my $buffer = Log::Log4perl::Appender::TestBuffer->by_name("LogBuffer");

my $sub = AppSubClass->new();
$sub->meth();

is $buffer->buffer(), "in base class\n", "subclass logger in base class";
