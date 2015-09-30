BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use warnings;
use strict;

use Test::More tests => 3;

use Log::Log4perl;
use Log::Log4perl::Appender::TestBuffer;

eval {
    Log::Log4perl->init('nonexistant_file');
};

ok((not Log::Log4perl->initialized()), 'Failed init doesn\'t flag initialized');

Log::Log4perl->reset();

eval {
    Log::Log4perl->init_once('nonexistant_file');
};

ok((not Log::Log4perl->initialized()), 'Failed init_once doesn\'t flag '
                                    .'initialized');

Log::Log4perl->reset();

eval {
    Log::Log4perl->init(\ <<EOT);
log4j.rootLogger=DEBUG, A1
log4j.appender.A1=Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout=org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern=%-4r [%t] %-5p %c - %m%n
EOT
};

ok(Log::Log4perl->initialized(), 'init flags initialized');

1; # End of 060Initialized.t
