#!/usr/bin/perl -w

use strict;

BEGIN
{
    $ENV{PERL_NO_VALIDATION} = 1;
    require Params::Validate;
    Params::Validate->import(':all');
}

use lib '.', './t';

require 'defaults.pl';
