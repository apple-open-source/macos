#!/usr/bin/perl -wT

use strict;

use lib './t';

eval { "$0$^X" && kill 0; 1 };

do '01-validate.t';
