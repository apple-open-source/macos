#!/usr/bin/perl -w

use strict;

use lib './lib', '../lib';

use Log::Dispatch::Email::MailSend;

Mail::Mailer->import( sendmail => 't/sendmail' );

my $email = Log::Dispatch::Email::MailSend->new(
    name      => 'email',
    min_level => 'emerg',
    to        => 'foo@example.com',
    subject   => 'Log this',
);

$email->log( message => 'Something bad is happening', level => 'emerg' );

exit 5;
