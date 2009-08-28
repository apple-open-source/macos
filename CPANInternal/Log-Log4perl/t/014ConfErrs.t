use Log::Log4perl;
use Test;

$testfile = 't/tmp/test12.log';

unlink $testfile if (-e $testfile);

# *****************************************************
# nonexistent appender class
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, myAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::FileAppenderx
log4j.appender.myAppender.layout = Log::Log4perl::Layout::SimpleLayout
log4j.appender.myAppender.File   = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);
};
ok($@, '/ERROR: can\'t load appenderclass \'Log::Log4perl::Appender::FileAppenderx\'/');


# *****************************************************
# nonexistent layout class
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, myAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::TestBuffer
log4j.appender.myAppender.layout = Log::Log4perl::Layout::SimpleLayoutx
log4j.appender.myAppender.File   = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);
};
ok($@,'/ERROR: trying to set layout for myAppender to \'Log::Log4perl::Layout::SimpleLayoutx\' failed/');

# *****************************************************
# nonexistent appender class containing a ';'
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, myAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::TestBuffer;
log4j.appender.myAppender.layout = Log::Log4perl::Layout::SimpleLayout
log4j.appender.myAppender.File   = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);
};
ok($@, '/ERROR: can\'t load appenderclass \'Log::Log4perl::Appender::TestBuffer;\'/');

# *****************************************************
# nonexistent layout class containing a ';'
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, myAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::TestBuffer
log4j.appender.myAppender.layout = Log::Log4perl::Layout::SimpleLayout;
log4j.appender.myAppender.File   = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);
};
ok($@, "/trying to set layout for myAppender to 'Log::Log4perl::Layout::SimpleLayout;' failed/");

# *****************************************************
# Relative Layout class
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, myAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::TestBuffer
log4j.appender.myAppender.layout = SimpleLayout
log4j.appender.myAppender.File   = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);
};
    # It's supposed to find it.
ok($@, '');

# *****************************************************
# bad priority
$conf = <<EOL;
log4j.category.simplelayout.test=xxINFO, myAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::File
log4j.appender.myAppender.layout = Log::Log4perl::Layout::SimpleLayout
log4j.appender.myAppender.File   = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);

};
ok($@,"/level 'xxINFO' is not a valid error level/");



# *****************************************************
# nonsense conf file 1
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, myAppender

log4j.appender.myAppender          = Log::Log4perl::Appender::Screen
log4j.appender.myAppender.nolayout = Log::Log4perl::Layout::SimpleLayout
log4j.appender.myAppender.File     = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);
};
ok($@,'/Layout not specified for appender myAppender at/');

# *****************************************************
# nonsense conf file 2
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, myAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::FileAppender
log4j.appender.myAppender.layout = Log::Log4perl::Layout::SimpleLayout
log4j.appender.myAppender        = $testfile
EOL

eval{

    Log::Log4perl->init(\$conf);

};
ok($@,"/log4j.appender.myAppender redefined/");



# *****************************************************
# never define an appender
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, XXmyAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::TestBuffer
log4j.appender.myAppender.layout = Log::Log4perl::Layout::SimpleLayout
log4j.appender.myAppender.File   = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);

};
ok($@,"/ERROR: you didn't tell me how to implement your appender 'XXmyAppender'/");


# *****************************************************
# never define a layout
$conf = <<EOL;
log4j.category.simplelayout.test=INFO, myAppender

log4j.appender.myAppender        = Log::Log4perl::Appender::TestBuffer

EOL

eval{
    Log::Log4perl->init(\$conf);

};
ok($@,"/Layout not specified for appender myAppender/");




# ************************************
# check continuation chars, this should parse fine
$conf = <<EOL;
log4j.category.simplelayout.test=\\
                        INFO, \\
                        myAppender

log4j.appender.myAppender        \\
                    = Log::Log4perl::Appender::TestBuffer
    #this is stupid, I know
log4j.appender.myAppender.layout = Log::Log4perl::Lay\\
                        out::SimpleL\\
                            ayout     
log4j.appender.myAppender.File   = $testfile
EOL

eval{
    Log::Log4perl->init(\$conf);

};
ok($@,"");

# *****************************************************
# init_once
# *****************************************************
Log::Log4perl->reset();
$conf = <<EOL;
log4perl.category = INFO, myAppender

log4perl.appender.myAppender        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.myAppender.layout = SimpleLayout
EOL

Log::Log4perl->init_once(\$conf);
my $logger = Log::Log4perl::get_logger("");
$logger->error("foobar");
$buffer = Log::Log4perl::Appender::TestBuffer->by_name("myAppender");

#print "BUFFER: [", $buffer->buffer(), "]\n";
ok($buffer->buffer(),"ERROR - foobar\n");

$conf = <<EOL;
log4perl.category = FATAL, myAppender

log4perl.appender.myAppender        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.myAppender.layout = SimpleLayout
EOL

   # change config, call init_once(), which should ignore the new
   # settings.
$buffer->buffer("");
Log::Log4perl->init_once(\$conf);
$logger = Log::Log4perl::get_logger("");
$logger->error("foobar");
my $buffer = Log::Log4perl::Appender::TestBuffer->by_name("myAppender");

#print "BUFFER: [", $buffer->buffer(), "]\n";
ok($buffer->buffer(),"ERROR - foobar\n");

$conf = <<EOL;
log4perl.logger.Foo.Bar          = INFO, Screen
log4perl.logger.Foo.Bar          = INFO, Screen
log4perl.appender.Screen         = Log::Log4perl::Appender::TestBuffer
log4perl.appender.Screen.layout  = SimpleLayout
EOL
eval {
    Log::Log4perl::init( \$conf );
};
ok($@, '/log4perl.logger.Foo.Bar redefined/');

BEGIN { plan tests => 14, }

END{   
     unlink $testfile if (-e $testfile);
}

