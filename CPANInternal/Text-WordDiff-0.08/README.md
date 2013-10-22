Text/WordDiff version 0.08
==========================

This library's module, Text::WordDiff, is a variation on the lovely
[Text::Diff](http://search.cpan.org/perldoc?Text::Diff) module. Rather than
generating traditional line-oriented diffs, however, it generates
word-oriented diffs. This can be useful for tracking changes in narrative
documents or documents with very long lines. To diff source code, one is still
best off using Text::Diff. But if you want to see how a short story changed
from one version to the next, this module will do the job very nicely.

INSTALLATION

To install this module, type the following:

    perl Build.PL
    ./Build
    ./Build test
    ./Build install

Or, if you don't have Module::Build installed, type the following:

    perl Makefile.PL
    make
    make test
    make install

Dependencies
------------

Text::WordDiff requires the following modules:

* Algorithm::Diff '1.19',
* Term::ANSIColor '0',
* HTML::Entities '0',

Copyright and Licence
---------------------

Copyright (c) 2005-2011 David E. Wheeler. Some Rights Reserved.

This module is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

