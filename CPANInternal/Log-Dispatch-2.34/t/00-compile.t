use strict;
use warnings;

use Test::More;

my %deps = (
    ApacheLog             => 'Apache::Log',
    File                  => '',
    'File::Locked'        => '',
    Handle                => '',
    Screen                => '',
    Syslog                => 'Sys::Syslog 0.25',
    'Email::MailSend'     => 'Mail::Send',
    'Email::MIMELite'     => 'MIME::Lite',
    'Email::MailSendmail' => 'Mail::Sendmail',
    'Email::MailSender'   => 'Mail::Sender',
);

plan tests => 1 + scalar keys %deps;

use_ok('Log::Dispatch');

for my $subclass ( sort keys %deps ) {
    my $module = "Log::Dispatch::$subclass";

    if ( !$deps{$subclass}
        || ( eval "use $deps{$subclass}; 1" && !$@ ) ) {
        use_ok($module);
    }
    else {
    SKIP:
        {
            skip "Cannot load $module without $deps{$subclass}", 1;
        }
    }
}
